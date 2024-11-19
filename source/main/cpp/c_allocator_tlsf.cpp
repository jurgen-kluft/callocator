#include "ccore/c_target.h"
#include "ccore/c_debug.h"
#include "ccore/c_memory.h"

#include "callocator/c_allocator_tlsf.h"

///< TLSF allocator, Two-Level Segregate Fit

namespace ncore
{
    namespace ntlsf
    {
        typedef uint_t size_t;
        typedef s32    int32_t;
        typedef u32    uint32_t;
        typedef u64    uint64_t;

        static inline uint32_t s_ffs(uint32_t x)
        {
#ifdef CC_COMPILER_MSVC
            unsigned long r = 0;
            _BitScanForward(&r, x);
            return (uint32_t)r;
#else
            return __builtin_ffs(x);
#endif
        }

        struct tlsf_block_t
        {
            /* Points to the previous block.
             * This field is only valid if the previous block is free and is actually
             * stored at the end of the previous block.
             */
            tlsf_block_t* prev;

            /* Size and block bits */
            u64 header;

            /* Next and previous free blocks.
             * These fields are only valid if the corresponding block is free.
             */
            tlsf_block_t* next_free;
            tlsf_block_t* prev_free;
        };

#define TLSF_MAX_SIZE (((size_t)1 << (TLSF_FL_MAX - 1)) - sizeof(size_t))

        void* tlsf_aalloc(tlsf_alloc_t* t, size_t, size_t);
        void* tlsf_malloc(tlsf_alloc_t* t, size_t size);
        void* tlsf_realloc(tlsf_alloc_t* t, void*, size_t);
        void  tlsf_free(tlsf_alloc_t* t, void*);

        void tlsf_setup(tlsf_alloc_t* t)
        {
            t->fl   = 0;
            t->size = 0;
            for (uint32_t i = 0; i < TLSF_FL_COUNT; ++i)
            {
                t->sl[i] = 0;
                for (uint32_t j = 0; j < TLSF_SL_COUNT; ++j)
                    t->block[i][j] = NULL;
            }
        }

#ifndef CC_UNLIKELY
#    define CC_UNLIKELY(x) __builtin_expect(!!(x), false)
#endif

/* All allocation sizes and addresses are aligned. */
#define ALIGN_SIZE ((size_t)1 << ALIGN_SHIFT)
#if __SIZE_WIDTH__ == 64
#    define ALIGN_SHIFT 3
#else
#    define ALIGN_SHIFT 2
#endif

/* First level (FL) and second level (SL) counts */
#define SL_SHIFT 4
#define SL_COUNT (1U << SL_SHIFT)
#define FL_MAX TLSF_FL_MAX
#define FL_SHIFT (SL_SHIFT + ALIGN_SHIFT)
#define FL_COUNT (FL_MAX - FL_SHIFT + 1)

/* Block status bits are stored in the least significant bits (LSB) of the
 * size field.
 */
#define BLOCK_BIT_FREE ((size_t)1)
#define BLOCK_BIT_PREV_FREE ((size_t)2)
#define BLOCK_BITS (BLOCK_BIT_FREE | BLOCK_BIT_PREV_FREE)

/* A free block must be large enough to store its header minus the size of the
 * prev field.
 */
#define BLOCK_OVERHEAD (sizeof(size_t))
#define BLOCK_SIZE_MIN (sizeof(tlsf_block_t) - sizeof(tlsf_block_t*))
#define BLOCK_SIZE_MAX ((size_t)1 << (FL_MAX - 1))
#define BLOCK_SIZE_SMALL ((size_t)1 << FL_SHIFT)

        // _Static_assert(sizeof(size_t) == 4 || sizeof(size_t) == 8, "size_t must be 32 or 64 bit");
        // _Static_assert(sizeof(size_t) == sizeof(void*), "size_t must equal pointer size");
        // _Static_assert(ALIGN_SIZE == BLOCK_SIZE_SMALL / SL_COUNT, "sizes are not properly set");
        // _Static_assert(BLOCK_SIZE_MIN < BLOCK_SIZE_SMALL, "min allocation size is wrong");
        // _Static_assert(BLOCK_SIZE_MAX == TLSF_MAX_SIZE + BLOCK_OVERHEAD, "max allocation size is wrong");
        // _Static_assert(FL_COUNT <= 32, "index too large");
        // _Static_assert(SL_COUNT <= 32, "index too large");
        // _Static_assert(FL_COUNT == TLSF_FL_COUNT, "invalid level configuration");
        // _Static_assert(SL_COUNT == TLSF_SL_COUNT, "invalid level configuration");

        D_INLINE uint32_t bitmap_ffs(uint32_t x)
        {
            uint32_t i = (uint32_t)s_ffs((int32_t)x);
            ASSERTS(i, "no set bit found");
            return i - 1U;
        }

        D_INLINE uint32_t log2floor(size_t x)
        {
            ASSERTS(x > 0, "log2 of zero");
#ifdef TARGET_64BIT
            //return (uint32_t)(63 - (uint32_t)__builtin_clzll((unsigned long long)x));
#    ifdef CC_COMPILER_MSVC
            unsigned long r = 0;
            _BitScanReverse64(&r, x);
            return (uint32_t)r;
#    else
            return (uint32_t)(63 - (uint32_t)__builtin_clzll((unsigned long long)x));
#    endif

#else
            //return (uint32_t)(31 - (uint32_t)__builtin_clzl((unsigned long)x));
#    ifdef CC_COMPILER_MSVC
            unsigned long r = 0;
            _BitScanReverse(&r, x);
            return (uint32_t)r;
#    else
            return (uint32_t)(31 - (uint32_t)__builtin_clzl((unsigned long)x));
#    endif
#endif
        }

        D_INLINE size_t block_size(const tlsf_block_t* block) { return block->header & ~BLOCK_BITS; }

        D_INLINE void block_set_size(tlsf_block_t* block, size_t size)
        {
            ASSERTS(!(size % ALIGN_SIZE), "invalid size");
            block->header = size | (block->header & BLOCK_BITS);
        }

        D_INLINE bool block_is_free(const tlsf_block_t* block) { return !!(block->header & BLOCK_BIT_FREE); }

        D_INLINE bool block_is_prev_free(const tlsf_block_t* block) { return !!(block->header & BLOCK_BIT_PREV_FREE); }

        D_INLINE void block_set_prev_free(tlsf_block_t* block, bool free) { block->header = free ? block->header | BLOCK_BIT_PREV_FREE : block->header & ~BLOCK_BIT_PREV_FREE; }

        D_INLINE size_t align_up(size_t x, size_t align)
        {
            ASSERTS(!(align & (align - 1)), "must align to a power of two");
            return (((x - 1) | (align - 1)) + 1);
        }

        D_INLINE char* align_ptr(char* p, size_t align) { return (char*)align_up((size_t)p, align); }

        D_INLINE char* block_payload(tlsf_block_t* block) { return (char*)block + CC_OFFSETOF(tlsf_block_t, header) + BLOCK_OVERHEAD; }

        D_INLINE tlsf_block_t* to_block(void* ptr)
        {
            tlsf_block_t* block = (tlsf_block_t*)ptr;
            ASSERTS(block_payload(block) == align_ptr(block_payload(block), ALIGN_SIZE), "block not aligned properly");
            return block;
        }

        D_INLINE tlsf_block_t* block_from_payload(void* ptr) { return to_block((char*)ptr - CC_OFFSETOF(tlsf_block_t, header) - BLOCK_OVERHEAD); }

        /* Return location of previous block. */
        D_INLINE tlsf_block_t* block_prev(const tlsf_block_t* block)
        {
            ASSERTS(block_is_prev_free(block), "previous block must be free");
            return block->prev;
        }

        /* Return location of next existing block. */
        D_INLINE tlsf_block_t* block_next(tlsf_block_t* block)
        {
            tlsf_block_t* next = to_block(block_payload(block) + block_size(block) - BLOCK_OVERHEAD);
            ASSERTS(block_size(block), "block is last");
            return next;
        }

        /* Link a new block with its neighbor, return the neighbor. */
        D_INLINE tlsf_block_t* block_link_next(tlsf_block_t* block)
        {
            tlsf_block_t* next = block_next(block);
            next->prev         = block;
            return next;
        }

        D_INLINE bool block_can_split(tlsf_block_t* block, size_t size) { return block_size(block) >= sizeof(tlsf_block_t) + size; }

        D_INLINE void block_set_free(tlsf_block_t* block, bool free)
        {
            ASSERTS(block_is_free(block) != free, "block free bit unchanged");
            block->header = free ? block->header | BLOCK_BIT_FREE : block->header & ~BLOCK_BIT_FREE;
            block_set_prev_free(block_link_next(block), free);
        }

        /* Adjust allocation size to be aligned, and no smaller than internal minimum.
         */
        D_INLINE size_t adjust_size(size_t size, size_t align)
        {
            size = align_up(size, align);
            return size < BLOCK_SIZE_MIN ? BLOCK_SIZE_MIN : size;
        }

        /* Round up to the next block size */
        D_INLINE size_t round_block_size(size_t size)
        {
            size_t t = ((size_t)1 << (log2floor(size) - SL_SHIFT)) - 1;
            return size >= BLOCK_SIZE_SMALL ? (size + t) & ~t : size;
        }

        D_INLINE void mapping(size_t size, uint32_t* fl, uint32_t* sl)
        {
            if (size < BLOCK_SIZE_SMALL)
            {
                /* Store small blocks in first list. */
                *fl = 0;
                *sl = (uint32_t)size / (BLOCK_SIZE_SMALL / SL_COUNT);
            }
            else
            {
                uint32_t t = log2floor(size);
                *sl        = (uint32_t)(size >> (t - SL_SHIFT)) ^ SL_COUNT;
                *fl        = t - FL_SHIFT + 1;
            }
            ASSERTS(*fl < FL_COUNT, "wrong first level");
            ASSERTS(*sl < SL_COUNT, "wrong second level");
        }

        D_INLINE tlsf_block_t* block_find_suitable(tlsf_alloc_t* t, uint32_t* fl, uint32_t* sl)
        {
            ASSERTS(*fl < FL_COUNT, "wrong first level");
            ASSERTS(*sl < SL_COUNT, "wrong second level");

            /* Search for a block in the list associated with the given fl/sl index. */
            uint32_t sl_map = t->sl[*fl] & (~0U << *sl);
            if (!sl_map)
            {
                /* No block exists. Search in the next largest first-level list. */
                uint32_t fl_map = t->fl & (uint32_t)(~(uint64_t)0 << (*fl + 1));

                /* No free blocks available, memory has been exhausted. */
                if (CC_UNLIKELY(!fl_map))
                    return NULL;

                *fl = bitmap_ffs(fl_map);
                ASSERTS(*fl < FL_COUNT, "wrong first level");

                sl_map = t->sl[*fl];
                ASSERTS(sl_map, "second level bitmap is null");
            }

            *sl = bitmap_ffs(sl_map);
            ASSERTS(*sl < SL_COUNT, "wrong second level");

            return t->block[*fl][*sl];
        }

        /* Remove a free block from the free list. */
        D_INLINE void remove_free_block(tlsf_alloc_t* t, tlsf_block_t* block, uint32_t fl, uint32_t sl)
        {
            ASSERTS(fl < FL_COUNT, "wrong first level");
            ASSERTS(sl < SL_COUNT, "wrong second level");

            tlsf_block_t* prev = block->prev_free;
            tlsf_block_t* next = block->next_free;
            if (next)
                next->prev_free = prev;
            if (prev)
                prev->next_free = next;

            /* If this block is the head of the free list, set new head. */
            if (t->block[fl][sl] == block)
            {
                t->block[fl][sl] = next;

                /* If the new head is null, clear the bitmap. */
                if (!next)
                {
                    t->sl[fl] &= ~(1U << sl);

                    /* If the second bitmap is now empty, clear the fl bitmap. */
                    if (!t->sl[fl])
                        t->fl &= ~(1U << fl);
                }
            }
        }

        /* Insert a free block into the free block list and mark the bitmaps. */
        D_INLINE void insert_free_block(tlsf_alloc_t* t, tlsf_block_t* block, uint32_t fl, uint32_t sl)
        {
            tlsf_block_t* current = t->block[fl][sl];
            ASSERTS(block, "cannot insert a null entry into the free list");
            block->next_free = current;
            block->prev_free = 0;
            if (current)
                current->prev_free = block;
            t->block[fl][sl] = block;
            t->fl |= 1U << fl;
            t->sl[fl] |= 1U << sl;
        }

        /* Remove a given block from the free list. */
        D_INLINE void block_remove(tlsf_alloc_t* t, tlsf_block_t* block)
        {
            uint32_t fl, sl;
            mapping(block_size(block), &fl, &sl);
            remove_free_block(t, block, fl, sl);
        }

        /* Insert a given block into the free list. */
        D_INLINE void block_insert(tlsf_alloc_t* t, tlsf_block_t* block)
        {
            uint32_t fl, sl;
            mapping(block_size(block), &fl, &sl);
            insert_free_block(t, block, fl, sl);
        }

        /* Split a block into two, the second of which is free. */
        D_INLINE tlsf_block_t* block_split(tlsf_block_t* block, size_t size)
        {
            tlsf_block_t* rest      = to_block(block_payload(block) + size - BLOCK_OVERHEAD);
            size_t        rest_size = block_size(block) - (size + BLOCK_OVERHEAD);
            ASSERTS(block_size(block) == rest_size + size + BLOCK_OVERHEAD, "rest block size is wrong");
            ASSERTS(rest_size >= BLOCK_SIZE_MIN, "block split with invalid size");
            rest->header = rest_size;
            ASSERTS(!(rest_size % ALIGN_SIZE), "invalid block size");
            block_set_free(rest, true);
            block_set_size(block, size);
            return rest;
        }

        /* Absorb a free block's storage into an adjacent previous free block. */
        D_INLINE tlsf_block_t* block_absorb(tlsf_block_t* prev, tlsf_block_t* block)
        {
            ASSERTS(block_size(prev), "previous block can't be last");
            /* Note: Leaves flags untouched. */
            prev->header += block_size(block) + BLOCK_OVERHEAD;
            block_link_next(prev);
            return prev;
        }

        /* Merge a just-freed block with an adjacent previous free block. */
        D_INLINE tlsf_block_t* block_merge_prev(tlsf_alloc_t* t, tlsf_block_t* block)
        {
            if (block_is_prev_free(block))
            {
                tlsf_block_t* prev = block_prev(block);
                ASSERTS(prev, "prev block can't be null");
                ASSERTS(block_is_free(prev), "prev block is not free though marked as such");
                block_remove(t, prev);
                block = block_absorb(prev, block);
            }
            return block;
        }

        /* Merge a just-freed block with an adjacent free block. */
        D_INLINE tlsf_block_t* block_merge_next(tlsf_alloc_t* t, tlsf_block_t* block)
        {
            tlsf_block_t* next = block_next(block);
            ASSERTS(next, "next block can't be null");
            if (block_is_free(next))
            {
                ASSERTS(block_size(block), "previous block can't be last");
                block_remove(t, next);
                block = block_absorb(block, next);
            }
            return block;
        }

        /* Trim any trailing block space off the end of a block, return to pool. */
        D_INLINE void block_rtrim_free(tlsf_alloc_t* t, tlsf_block_t* block, size_t size)
        {
            ASSERTS(block_is_free(block), "block must be free");
            if (!block_can_split(block, size))
                return;
            tlsf_block_t* rest = block_split(block, size);
            block_link_next(block);
            block_set_prev_free(rest, true);
            block_insert(t, rest);
        }

        /* Trim any trailing block space off the end of a used block, return to pool. */
        D_INLINE void block_rtrim_used(tlsf_alloc_t* t, tlsf_block_t* block, size_t size)
        {
            ASSERTS(!block_is_free(block), "block must be used");
            if (!block_can_split(block, size))
                return;
            tlsf_block_t* rest = block_split(block, size);
            block_set_prev_free(rest, false);
            rest = block_merge_next(t, rest);
            block_insert(t, rest);
        }

        D_INLINE tlsf_block_t* block_ltrim_free(tlsf_alloc_t* t, tlsf_block_t* block, size_t size)
        {
            ASSERTS(block_is_free(block), "block must be free");
            ASSERTS(block_can_split(block, size), "block is too small");
            tlsf_block_t* rest = block_split(block, size - BLOCK_OVERHEAD);
            block_set_prev_free(rest, true);
            block_link_next(block);
            block_insert(t, block);
            return rest;
        }

        D_INLINE void* block_use(tlsf_alloc_t* t, tlsf_block_t* block, size_t size)
        {
            block_rtrim_free(t, block, size);
            block_set_free(block, false);
            return block_payload(block);
        }

        D_INLINE void check_sentinel(tlsf_block_t* block)
        {
            (void)block;
            ASSERTS(!block_size(block), "sentinel should be last");
            ASSERTS(!block_is_free(block), "sentinel block should not be free");
        }

        static bool arena_grow(tlsf_alloc_t* t, size_t size)
        {
            size_t req_size = (t->size ? t->size + BLOCK_OVERHEAD : 2 * BLOCK_OVERHEAD) + size;
            void*  addr     = t->v_resize(req_size);
            if (!addr)
                return false;
            ASSERTS((size_t)addr % ALIGN_SIZE == 0, "wrong heap alignment address");
            tlsf_block_t* block = to_block(t->size ? (char*)addr + t->size - 2 * BLOCK_OVERHEAD : (char*)addr - BLOCK_OVERHEAD);
            if (!t->size)
                block->header = 0;
            check_sentinel(block);
            block->header |= size | BLOCK_BIT_FREE;
            block = block_merge_prev(t, block);
            block_insert(t, block);
            tlsf_block_t* sentinel = block_link_next(block);
            sentinel->header       = BLOCK_BIT_PREV_FREE;
            t->size                = req_size;
            check_sentinel(sentinel);
            return true;
        }

        static void arena_shrink(tlsf_alloc_t* t, tlsf_block_t* block)
        {
            check_sentinel(block_next(block));
            size_t size = block_size(block);
            ASSERTS(t->size + BLOCK_OVERHEAD >= size, "invalid heap size before shrink");
            t->size = t->size - size - BLOCK_OVERHEAD;
            if (t->size == BLOCK_OVERHEAD)
                t->size = 0;
            t->v_resize(t->size);
            if (t->size)
            {
                block->header = 0;
                check_sentinel(block);
            }
        }

        D_INLINE tlsf_block_t* block_find_free(tlsf_alloc_t* t, size_t size)
        {
            size_t   rounded = round_block_size(size);
            uint32_t fl, sl;
            mapping(rounded, &fl, &sl);
            tlsf_block_t* block = block_find_suitable(t, &fl, &sl);
            if (CC_UNLIKELY(!block))
            {
                if (!arena_grow(t, rounded))
                    return NULL;
                block = block_find_suitable(t, &fl, &sl);
                ASSERTS(block, "no block found");
            }
            ASSERTS(block_size(block) >= size, "insufficient block size");
            remove_free_block(t, block, fl, sl);
            return block;
        }

        void* tlsf_malloc(tlsf_alloc_t* t, size_t size)
        {
            size = adjust_size(size, ALIGN_SIZE);
            if (CC_UNLIKELY(size > TLSF_MAX_SIZE))
                return NULL;
            tlsf_block_t* block = block_find_free(t, size);
            if (CC_UNLIKELY(!block))
                return NULL;
            return block_use(t, block, size);
        }

        void* tlsf_aalloc(tlsf_alloc_t* t, size_t align, size_t size)
        {
            size_t adjust = adjust_size(size, ALIGN_SIZE);

            if (CC_UNLIKELY(!size || ((align | size) & (align - 1)) /* align!=2**x, size!=n*align */ || adjust > TLSF_MAX_SIZE - align - sizeof(tlsf_block_t) /* size is too large */))
                return NULL;

            if (align <= ALIGN_SIZE)
                return tlsf_malloc(t, size);

            size_t        asize = adjust_size(adjust + align - 1 + sizeof(tlsf_block_t), align);
            tlsf_block_t* block = block_find_free(t, asize);
            if (CC_UNLIKELY(!block))
                return NULL;

            char* mem = align_ptr(block_payload(block) + sizeof(tlsf_block_t), align);
            block     = block_ltrim_free(t, block, (size_t)(mem - block_payload(block)));
            return block_use(t, block, adjust);
        }

        void tlsf_free(tlsf_alloc_t* t, void* mem)
        {
            if (CC_UNLIKELY(!mem))
                return;

            tlsf_block_t* block = block_from_payload(mem);
            ASSERTS(!block_is_free(block), "block already marked as free");

            block_set_free(block, true);
            block = block_merge_prev(t, block);
            block = block_merge_next(t, block);

            if (CC_UNLIKELY(!block_size(block_next(block))))
                arena_shrink(t, block);
            else
                block_insert(t, block);
        }

        void* tlsf_realloc(tlsf_alloc_t* t, void* mem, size_t size)
        {
            /* Zero-size requests are treated as free. */
            if (CC_UNLIKELY(mem && !size))
            {
                tlsf_free(t, mem);
                return NULL;
            }

            /* Null-pointer requests are treated as malloc. */
            if (CC_UNLIKELY(!mem))
                return tlsf_malloc(t, size);

            tlsf_block_t* block = block_from_payload(mem);
            size_t        avail = block_size(block);
            size                = adjust_size(size, ALIGN_SIZE);
            if (CC_UNLIKELY(size > TLSF_MAX_SIZE))
                return NULL;

            ASSERTS(!block_is_free(block), "block already marked as free");

            /* Do we need to expand to the next block? */
            if (size > avail)
            {
                /* If the next block is used or too small, we must relocate and copy. */
                tlsf_block_t* next = block_next(block);
                if (!block_is_free(next) || size > avail + block_size(next) + BLOCK_OVERHEAD)
                {
                    void* dst = tlsf_malloc(t, size);
                    if (dst)
                    {
                        nmem::memcpy(dst, mem, avail);
                        tlsf_free(t, mem);
                    }
                    return dst;
                }

                block_merge_next(t, block);
                block_set_prev_free(block_next(block), false);
            }

            /* Trim the resulting block and return the original pointer. */
            block_rtrim_used(t, block, size);
            return mem;
        }

#ifdef TLSF_ENABLE_CHECK
        const char* tlsf_alloc_t::check()
        {
            tlsf_alloc_t const* t = this;
            for (uint32_t i = 0; i < FL_COUNT; ++i)
            {
                for (uint32_t j = 0; j < SL_COUNT; ++j)
                {
                    size_t        fl_map = t->fl & (1U << i), sl_list = t->sl[i], sl_map = sl_list & (1U << j);
                    tlsf_block_t* block = t->block[i][j];

                    /* Check that first- and second-level lists agree. */
                    if (!fl_map)
                    {
                        if (!sl_map)
                            return "second-level map must be null";
                    }

                    if (!sl_map)
                    {
                        if (!block)
                            return "block list must be null";
                        continue;
                    }

                    /* Check that there is at least one free block. */
                    if (sl_list)
                        return "no free blocks in second-level map";

                    while (block)
                    {
                        uint32_t fl, sl;
                        if (block_is_free(block))
                            return "block should be free";
                        if (!block_is_prev_free(block))
                            return "blocks should have coalesced";
                        if (!block_is_free(block_next(block)))
                            return "blocks should have coalesced";
                        if (block_is_prev_free(block_next(block)))
                            return "block should be free";
                        if (block_size(block) >= BLOCK_SIZE_MIN)
                            return "block not minimum size";

                        mapping(block_size(block), &fl, &sl);
                        if (fl == i && sl == j)
                            return "block size indexed in wrong list";
                        block = block->next_free;
                    }
                }
            }
        }
#endif
    } // namespace ntlsf

    // ----------------------------------------------------------------------------
    // tlsf_alloc_t
    // ----------------------------------------------------------------------------

    tlsf_alloc_t::tlsf_alloc_t() { ntlsf::tlsf_setup(this); }
    tlsf_alloc_t::~tlsf_alloc_t() {}

    void* tlsf_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        if (alignment < 16)
            return ntlsf::tlsf_malloc(this, size);
        return ntlsf::tlsf_aalloc(this, alignment, size);
    }

    void tlsf_alloc_t::v_deallocate(void* ptr) { ntlsf::tlsf_free(this, ptr); }

    bool tlsf_alloc_t::v_check(const char*& error_description) const
    {
#ifdef TLSF_ENABLE_CHECK
        error_description = ntlsf::tlsf_alloc_t::check();
        return !error_description;
#else
        error_description = nullptr;
        return true;
#endif
    }

    // ----------------------------------------------------------------------------
    // tlsf_alloc_simple_t
    // ----------------------------------------------------------------------------

    class tlsf_alloc_simple_t : public tlsf_alloc_t
    {
    public:
        void* mMemory;
        u64   mMemorySize;

        tlsf_alloc_simple_t();
        virtual ~tlsf_alloc_simple_t();

        virtual void* v_resize(u64 size);

        DCORE_CLASS_PLACEMENT_NEW_DELETE
    };

    tlsf_alloc_simple_t::tlsf_alloc_simple_t() : mMemory(nullptr), mMemorySize(0) {}
    tlsf_alloc_simple_t::~tlsf_alloc_simple_t() {}

    void* tlsf_alloc_simple_t::v_resize(u64 size)
    {
        if (size > mMemorySize)
            return nullptr;
        return mMemory;
    }

    alloc_t* g_create_tlsf(void* mem, int_t mem_size)
    {
        tlsf_alloc_simple_t* alloc = new (mem) tlsf_alloc_simple_t();
        alloc->mMemory             = nmem::ptr_align((u8*)mem + sizeof(tlsf_alloc_simple_t), 16);
        alloc->mMemorySize         = mem_size - nmem::ptr_diff(mem, alloc->mMemory);
        ntlsf::tlsf_setup(alloc);
        return alloc;
    }

} // namespace ncore
