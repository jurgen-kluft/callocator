#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_debug.h"
#include "ccore/c_math.h"
#include "ccore/c_memory.h"
#include "ccore/c_vmem.h"

#include "callocator/c_allocator_heap.h"

#ifdef CC_COMPILER_MSVC
#    include <intrin.h>
#endif

// A heap allocator implemented according to the TLSF allocator, Two-Level Segregate Fit
namespace ncore
{
    namespace nheap
    {
        enum ELevelCounts
        {
            TLSF_SL_COUNT = 16,
#if CC_PLATFORM_PTR_SIZE == 8
            TLSF_FL_COUNT = 32,
            TLSF_FL_MAX   = 38,
#else
            TLSF_FL_COUNT = 25,
            TLSF_FL_MAX   = 30,
#endif
        };

        struct block_t // size in bytes = 8 + 8 + 8 + 8 = 32 bytes
        {
            // Valid only if the previous block is free and is actually
            // stored at the end of the previous block.
            block_t* prev;
            u64      header;    // Size and block bits
            block_t* next_free; // Next and previous free blocks.
            block_t* prev_free; // Only valid if the corresponding block is free.
        };

        struct context_t // size in bytes = 4 + (4) + 128 + 4096 + 8 = 4240
        {
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            u32      fl;                                  // 4 bytes, first level index
            u32      sl[TLSF_FL_COUNT];                   // 32 * 4 bytes = 128 bytes, second level indices
            block_t* block[TLSF_FL_COUNT][TLSF_SL_COUNT]; // 32 * 16 * 8 bytes = 4096 bytes, pointers to free blocks
            u64      size;                                // 8 bytes, total size of the managed memory region
        };

        class allocator_t : public alloc_t
        {
        public:
            allocator_t(context_t* context);
            virtual ~allocator_t();

            virtual void* v_allocate(u32 size, u32 alignment) final;
            virtual void  v_deallocate(void* ptr) final;

            virtual bool  v_check(const char*& error_description) const;
            virtual void* v_resize(u64 size) = 0;

            DCORE_CLASS_PLACEMENT_NEW_DELETE

            context_t* m_context;
        };

#define TLSF_MAX_SIZE (((uint_t)1 << (TLSF_FL_MAX - 1)) - sizeof(uint_t))

        static void* g_aalloc(allocator_t* a, context_t* t, uint_t, uint_t);
        static void* g_malloc(allocator_t* a, context_t* t, uint_t size);
        static void* g_realloc(allocator_t* a, context_t* t, void*, uint_t);
        static void  g_free(allocator_t* a, context_t* t, void*);

        static void g_setup(context_t* t)
        {
            t->fl   = 0;
            t->size = 0;
            for (u32 i = 0; i < TLSF_FL_COUNT; ++i)
            {
                t->sl[i] = 0;
                for (u32 j = 0; j < TLSF_SL_COUNT; ++j)
                    t->block[i][j] = nullptr;
            }
        }

// All allocation sizes and addresses are aligned.
#if CC_PLATFORM_PTR_SIZE == 8
#    define ALIGN_SHIFT 3
#else
#    define ALIGN_SHIFT 2
#endif
#define ALIGN_SIZE ((uint_t)1 << ALIGN_SHIFT)

// First level (FL) and second level (SL) counts
#define SL_SHIFT 4
#define SL_COUNT (1U << SL_SHIFT)
#define FL_MAX TLSF_FL_MAX
#define FL_SHIFT (SL_SHIFT + ALIGN_SHIFT)
#define FL_COUNT (FL_MAX - FL_SHIFT + 1)

// Block status bits are stored in the least significant bits (LSB) of the size field.
#define BLOCK_BIT_FREE ((uint_t)1)
#define BLOCK_BIT_PREV_FREE ((uint_t)2)
#define BLOCK_BITS (BLOCK_BIT_FREE | BLOCK_BIT_PREV_FREE)

// A free block must be large enough to store its header minus the size of the* prev field.
#define BLOCK_OVERHEAD (sizeof(uint_t))
#define BLOCK_SIZE_MIN (sizeof(block_t) - sizeof(block_t*))
#define BLOCK_SIZE_MAX ((uint_t)1 << (FL_MAX - 1))
#define BLOCK_SIZE_SMALL ((uint_t)1 << FL_SHIFT)

        STATIC_ASSERTS(sizeof(uint_t) == 4 || sizeof(uint_t) == 8, "uint_t must be 32 or 64 bit");
        STATIC_ASSERTS(sizeof(uint_t) == sizeof(void*), "uint_t must equal pointer size");
        STATIC_ASSERTS(ALIGN_SIZE == BLOCK_SIZE_SMALL / SL_COUNT, "sizes are not properly set");
        STATIC_ASSERTS(BLOCK_SIZE_MIN < BLOCK_SIZE_SMALL, "min allocation size is wrong");
        STATIC_ASSERTS(BLOCK_SIZE_MAX == TLSF_MAX_SIZE + BLOCK_OVERHEAD, "max allocation size is wrong");
        STATIC_ASSERTS(FL_COUNT <= 32, "index too large");
        STATIC_ASSERTS(SL_COUNT <= 32, "index too large");
        STATIC_ASSERTS(FL_COUNT == TLSF_FL_COUNT, "invalid level configuration");
        STATIC_ASSERTS(SL_COUNT == TLSF_SL_COUNT, "invalid level configuration");

        static D_INLINE u32 bitmap_ffs(u32 x) { return (u32)math::findFirstBit(x); }
        static D_INLINE s8  log2floor(uint_t x) { return math::ilog2(x); }

        D_INLINE uint_t block_size(const block_t* block) { return block->header & ~BLOCK_BITS; }
        D_INLINE void   block_set_size(block_t* block, uint_t size)
        {
            ASSERTS(!(size % ALIGN_SIZE), "invalid size");
            block->header = size | (block->header & BLOCK_BITS);
        }

        D_INLINE bool block_is_free(const block_t* block) { return !!(block->header & BLOCK_BIT_FREE); }
        D_INLINE bool block_is_prev_free(const block_t* block) { return !!(block->header & BLOCK_BIT_PREV_FREE); }
        D_INLINE void block_set_prev_free(block_t* block, bool free) { block->header = free ? block->header | BLOCK_BIT_PREV_FREE : block->header & ~BLOCK_BIT_PREV_FREE; }

        D_INLINE uint_t align_up(uint_t x, uint_t align)
        {
            ASSERTS(!(align & (align - 1)), "must align to a power of two");
            return (((x - 1) | (align - 1)) + 1);
        }

        D_INLINE char*    align_ptr(char* p, uint_t align) { return (char*)align_up((uint_t)p, align); }
        D_INLINE char*    block_payload(block_t* block) { return (char*)block + CC_OFFSETOF(block_t, header) + BLOCK_OVERHEAD; }
        D_INLINE block_t* to_block(void* ptr)
        {
            block_t* block = (block_t*)ptr;
            ASSERTS(block_payload(block) == align_ptr(block_payload(block), ALIGN_SIZE), "block not aligned properly");
            return block;
        }

        D_INLINE block_t* block_from_payload(void* ptr) { return to_block((char*)ptr - CC_OFFSETOF(block_t, header) - BLOCK_OVERHEAD); }

        // Return location of previous block.
        D_INLINE block_t* block_prev(const block_t* block)
        {
            ASSERTS(block_is_prev_free(block), "previous block must be free");
            return block->prev;
        }

        // Return location of next existing block.
        D_INLINE block_t* block_next(block_t* block)
        {
            block_t* next = to_block(block_payload(block) + block_size(block) - BLOCK_OVERHEAD);
            ASSERTS(block_size(block), "block is last");
            return next;
        }

        // Link a new block with its neighbor, return the neighbor.
        D_INLINE block_t* block_link_next(block_t* block)
        {
            block_t* next = block_next(block);
            next->prev    = block;
            return next;
        }

        D_INLINE bool block_can_split(block_t* block, uint_t size) { return block_size(block) >= sizeof(block_t) + size; }
        D_INLINE void block_set_free(block_t* block, bool free)
        {
            ASSERTS(block_is_free(block) != free, "block free bit unchanged");
            block->header = free ? block->header | BLOCK_BIT_FREE : block->header & ~BLOCK_BIT_FREE;
            block_set_prev_free(block_link_next(block), free);
        }

        // Adjust allocation size to be aligned, and no smaller than internal minimum.
        D_INLINE uint_t adjust_size(uint_t size, uint_t align)
        {
            size = align_up(size, align);
            return size < BLOCK_SIZE_MIN ? BLOCK_SIZE_MIN : size;
        }

        // Round up to the next block size
        D_INLINE uint_t round_block_size(uint_t size)
        {
            ASSERTS(size >= (1 << SL_SHIFT), "size must be greater than (1<<SL_SHIFT)");
            uint_t t = ((uint_t)1 << (log2floor(size) - SL_SHIFT)) - 1;
            return size >= BLOCK_SIZE_SMALL ? (size + t) & ~t : size;
        }

        D_INLINE void mapping(uint_t size, u32* fl, u32* sl)
        {
            if (size < BLOCK_SIZE_SMALL)
            {
                // Store small blocks in first list.
                *fl = 0;
                *sl = (u32)size / (BLOCK_SIZE_SMALL / SL_COUNT);
            }
            else
            {
                u32 t = log2floor(size);
                *sl   = (u32)(size >> (t - SL_SHIFT)) ^ SL_COUNT;
                *fl   = t - FL_SHIFT + 1;
            }
            ASSERTS(*fl < FL_COUNT, "wrong first level");
            ASSERTS(*sl < SL_COUNT, "wrong second level");
        }

        D_INLINE block_t* block_find_suitable(context_t* t, u32* fl, u32* sl)
        {
            ASSERTS(*fl < FL_COUNT, "wrong first level");
            ASSERTS(*sl < SL_COUNT, "wrong second level");

            /* Search for a block in the list associated with the given fl/sl index. */
            u32 sl_map = t->sl[*fl] & (~0U << *sl);
            if (!sl_map)
            {
                // No block exists. Search in the next largest first-level list.
                u32 fl_map = t->fl & (u32)(~(u64)0 << (*fl + 1));

                // No free blocks available, memory has been exhausted.
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

        // Remove a free block from the free list.
        D_INLINE void remove_free_block(context_t* t, block_t* block, u32 fl, u32 sl)
        {
            ASSERTS(fl < FL_COUNT, "wrong first level");
            ASSERTS(sl < SL_COUNT, "wrong second level");

            block_t* prev = block->prev_free;
            block_t* next = block->next_free;
            if (next)
                next->prev_free = prev;
            if (prev)
                prev->next_free = next;

            // If this block is the head of the free list, set new head.
            if (t->block[fl][sl] == block)
            {
                t->block[fl][sl] = next;

                // If the new head is null, clear the bitmap.
                if (!next)
                {
                    t->sl[fl] &= ~(1U << sl);

                    // If the second bitmap is now empty, clear the fl bitmap.
                    if (!t->sl[fl])
                        t->fl &= ~(1U << fl);
                }
            }
        }

        // Insert a free block into the free block list and mark the bitmaps.
        D_INLINE void insert_free_block(context_t* t, block_t* block, u32 fl, u32 sl)
        {
            block_t* current = t->block[fl][sl];
            ASSERTS(block, "cannot insert a null entry into the free list");
            block->next_free = current;
            block->prev_free = 0;
            if (current)
                current->prev_free = block;
            t->block[fl][sl] = block;
            t->fl |= 1U << fl;
            t->sl[fl] |= 1U << sl;
        }

        // Remove a given block from the free list.
        D_INLINE void block_remove(context_t* t, block_t* block)
        {
            u32 fl, sl;
            mapping(block_size(block), &fl, &sl);
            remove_free_block(t, block, fl, sl);
        }

        // Insert a given block into the free list.
        D_INLINE void block_insert(context_t* t, block_t* block)
        {
            u32 fl, sl;
            mapping(block_size(block), &fl, &sl);
            insert_free_block(t, block, fl, sl);
        }

        // Split a block into two, the second of which is free.
        D_INLINE block_t* block_split(block_t* block, uint_t size)
        {
            block_t* rest      = to_block(block_payload(block) + size - BLOCK_OVERHEAD);
            uint_t   rest_size = block_size(block) - (size + BLOCK_OVERHEAD);
            ASSERTS(block_size(block) == rest_size + size + BLOCK_OVERHEAD, "rest block size is wrong");
            ASSERTS(rest_size >= BLOCK_SIZE_MIN, "block split with invalid size");
            rest->header = rest_size;
            ASSERTS(!(rest_size % ALIGN_SIZE), "invalid block size");
            block_set_free(rest, true);
            block_set_size(block, size);
            return rest;
        }

        // Absorb a free block's storage into an adjacent previous free block.
        D_INLINE block_t* block_absorb(block_t* prev, block_t* block)
        {
            ASSERTS(block_size(prev), "previous block can't be last");
            /* Note: Leaves flags untouched. */
            prev->header += block_size(block) + BLOCK_OVERHEAD;
            block_link_next(prev);
            return prev;
        }

        // Merge a just-freed block with an adjacent previous free block.
        D_INLINE block_t* block_merge_prev(context_t* t, block_t* block)
        {
            if (block_is_prev_free(block))
            {
                block_t* prev = block_prev(block);
                ASSERTS(prev, "prev block can't be null");
                ASSERTS(block_is_free(prev), "prev block is not free though marked as such");
                block_remove(t, prev);
                block = block_absorb(prev, block);
            }
            return block;
        }

        // Merge a just-freed block with an adjacent free block.
        D_INLINE block_t* block_merge_next(context_t* t, block_t* block)
        {
            block_t* next = block_next(block);
            ASSERTS(next, "next block can't be null");
            if (block_is_free(next))
            {
                ASSERTS(block_size(block), "previous block can't be last");
                block_remove(t, next);
                block = block_absorb(block, next);
            }
            return block;
        }

        // Trim any trailing block space off the end of a block, return to pool.
        D_INLINE void block_rtrim_free(context_t* t, block_t* block, uint_t size)
        {
            ASSERTS(block_is_free(block), "block must be free");
            if (!block_can_split(block, size))
                return;
            block_t* rest = block_split(block, size);
            block_link_next(block);
            block_set_prev_free(rest, true);
            block_insert(t, rest);
        }

        // Trim any trailing block space off the end of a used block, return to pool.
        D_INLINE void block_rtrim_used(context_t* t, block_t* block, uint_t size)
        {
            ASSERTS(!block_is_free(block), "block must be used");
            if (!block_can_split(block, size))
                return;
            block_t* rest = block_split(block, size);
            block_set_prev_free(rest, false);
            rest = block_merge_next(t, rest);
            block_insert(t, rest);
        }

        D_INLINE block_t* block_ltrim_free(context_t* t, block_t* block, uint_t size)
        {
            ASSERTS(block_is_free(block), "block must be free");
            ASSERTS(block_can_split(block, size), "block is too small");
            block_t* rest = block_split(block, size - BLOCK_OVERHEAD);
            block_set_prev_free(rest, true);
            block_link_next(block);
            block_insert(t, block);
            return rest;
        }

        D_INLINE void* block_use(context_t* t, block_t* block, uint_t size)
        {
            block_rtrim_free(t, block, size);
            block_set_free(block, false);
            return block_payload(block);
        }

        D_INLINE void check_sentinel(block_t* block)
        {
            (void)block;
            ASSERTS(!block_size(block), "sentinel should be last");
            ASSERTS(!block_is_free(block), "sentinel block should not be free");
        }

        static bool arena_grow(allocator_t* allocator, context_t* t, uint_t size)
        {
            uint_t req_size = (t->size ? t->size + BLOCK_OVERHEAD : 2 * BLOCK_OVERHEAD) + size;
            void*  addr     = allocator->v_resize(req_size);
            if (!addr)
                return false;
            ASSERTS((uint_t)addr % ALIGN_SIZE == 0, "wrong heap alignment address");
            block_t* block = to_block(t->size ? (char*)addr + t->size - 2 * BLOCK_OVERHEAD : (char*)addr - BLOCK_OVERHEAD);
            if (!t->size)
                block->header = 0;
            check_sentinel(block);
            block->header |= size | BLOCK_BIT_FREE;
            block = block_merge_prev(t, block);
            block_insert(t, block);
            block_t* sentinel = block_link_next(block);
            sentinel->header  = BLOCK_BIT_PREV_FREE;
            t->size           = req_size;
            check_sentinel(sentinel);
            return true;
        }

        static void arena_shrink(allocator_t* allocator, context_t* t, block_t* block)
        {
            check_sentinel(block_next(block));
            uint_t size = block_size(block);
            ASSERTS(t->size + BLOCK_OVERHEAD >= size, "invalid heap size before shrink");
            t->size = t->size - size - BLOCK_OVERHEAD;
            if (t->size == BLOCK_OVERHEAD)
                t->size = 0;
            allocator->v_resize(t->size);
            if (t->size)
            {
                block->header = 0;
                check_sentinel(block);
            }
        }

        D_INLINE block_t* block_find_free(allocator_t* allocator, context_t* t, uint_t size)
        {
            uint_t rounded = round_block_size(size);
            u32    fl, sl;
            mapping(rounded, &fl, &sl);
            block_t* block = block_find_suitable(t, &fl, &sl);
            if (CC_UNLIKELY(!block))
            {
                if (!arena_grow(allocator, t, rounded))
                    return NULL;
                block = block_find_suitable(t, &fl, &sl);
                ASSERTS(block, "no block found");
            }
            ASSERTS(block_size(block) >= size, "insufficient block size");
            remove_free_block(t, block, fl, sl);
            return block;
        }

        void* g_malloc(allocator_t* allocator, context_t* t, uint_t size)
        {
            size = adjust_size(size, ALIGN_SIZE);
            if (CC_UNLIKELY(size > TLSF_MAX_SIZE))
                return NULL;
            block_t* block = block_find_free(allocator, t, size);
            if (CC_UNLIKELY(!block))
                return NULL;
            return block_use(t, block, size);
        }

        void* g_aalloc(allocator_t* allocator, context_t* t, uint_t align, uint_t size)
        {
            uint_t adjust = adjust_size(size, ALIGN_SIZE);

            if (CC_UNLIKELY(!size || ((align | size) & (align - 1)) /* align!=2**x, size!=n*align */ || adjust > TLSF_MAX_SIZE - align - sizeof(block_t)))
                return NULL;

            if (align <= ALIGN_SIZE)
                return g_malloc(allocator, t, size);

            uint_t   asize = adjust_size(adjust + align - 1 + sizeof(block_t), align);
            block_t* block = block_find_free(allocator, t, asize);
            if (CC_UNLIKELY(!block))
                return NULL;

            char* mem = align_ptr(block_payload(block) + sizeof(block_t), align);
            block     = block_ltrim_free(t, block, (uint_t)(mem - block_payload(block)));
            return block_use(t, block, adjust);
        }

        void g_free(allocator_t* allocator, context_t* t, void* mem)
        {
            if (CC_UNLIKELY(!mem))
                return;

            block_t* block = block_from_payload(mem);
            ASSERTS(!block_is_free(block), "block already marked as free");

            block_set_free(block, true);
            block = block_merge_prev(t, block);
            block = block_merge_next(t, block);

            if (CC_UNLIKELY(!block_size(block_next(block))))
                arena_shrink(allocator, t, block);
            else
                block_insert(t, block);
        }

        void* g_realloc(allocator_t* allocator, context_t* t, void* mem, uint_t size)
        {
            // Zero-size requests are treated as free.
            if (CC_UNLIKELY(mem && !size))
            {
                g_free(allocator, t, mem);
                return NULL;
            }

            // Null-pointer requests are treated as malloc.
            if (CC_UNLIKELY(!mem))
                return g_malloc(allocator, t, size);

            block_t* block = block_from_payload(mem);
            uint_t   avail = block_size(block);
            size           = adjust_size(size, ALIGN_SIZE);
            if (CC_UNLIKELY(size > TLSF_MAX_SIZE))
                return NULL;

            ASSERTS(!block_is_free(block), "block already marked as free");

            // Do we need to expand to the next block?
            if (size > avail)
            {
                // If the next block is used or too small, we must relocate and copy.
                block_t* next = block_next(block);
                if (!block_is_free(next) || size > avail + block_size(next) + BLOCK_OVERHEAD)
                {
                    void* dst = g_malloc(allocator, t, size);
                    if (dst)
                    {
                        nmem::memcpy(dst, mem, avail);
                        g_free(allocator, t, mem);
                    }
                    return dst;
                }

                block_merge_next(t, block);
                block_set_prev_free(block_next(block), false);
            }

            // Trim the resulting block and return the original pointer.
            block_rtrim_used(t, block, size);
            return mem;
        }

#ifdef TLSF_ENABLE_CHECK
        const char* g_check(context_t* t)
        {
            for (u32 i = 0; i < FL_COUNT; ++i)
            {
                for (u32 j = 0; j < SL_COUNT; ++j)
                {
                    uint_t   fl_map = t->fl & (1U << i), sl_list = t->sl[i], sl_map = sl_list & (1U << j);
                    block_t* block = t->block[i][j];

                    // Check that first- and second-level lists agree.
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

                    // Check that there is at least one free block.
                    if (sl_list)
                        return "no free blocks in second-level map";

                    while (block)
                    {
                        u32 fl, sl;
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

        allocator_t::allocator_t(context_t* ctx) : m_context(ctx) { g_setup(ctx); }
        allocator_t::~allocator_t() {}

        void* allocator_t::v_allocate(u32 size, u32 alignment)
        {
            if (alignment <= ALIGN_SIZE)
                return g_malloc(this, m_context, size);
            return g_aalloc(this, m_context, alignment, size);
        }

        void allocator_t::v_deallocate(void* ptr) { g_free(this, m_context, ptr); }

        bool allocator_t::v_check(const char*& error_description) const
        {
#ifdef TLSF_ENABLE_CHECK
            error_description = g_check(m_context);
            return !error_description;
#else
            error_description = nullptr;
            return true;
#endif
        }
    } // namespace nheap

    // ----------------------------------------------------------------------------
    // alloc_tlsf_vmem_t
    // ----------------------------------------------------------------------------

    class alloc_tlsf_vmem_t : public nheap::allocator_t
    {
    public:
        void*    m_save_address;
        arena_t* m_arena;

        alloc_tlsf_vmem_t(nheap::context_t* context, arena_t* arena);
        virtual ~alloc_tlsf_vmem_t();

        virtual void* v_resize(u64 size);

        DCORE_CLASS_PLACEMENT_NEW_DELETE
    };

    alloc_tlsf_vmem_t::alloc_tlsf_vmem_t(nheap::context_t* context, arena_t* arena) : nheap::allocator_t(context), m_arena(arena) {}
    alloc_tlsf_vmem_t::~alloc_tlsf_vmem_t() {}

    void* alloc_tlsf_vmem_t::v_resize(u64 size)
    {
        narena::commit_from_address(m_arena, m_save_address, size);
        return m_save_address;
    }

    alloc_t* g_create_heap(int_t initial_size, int_t reserved_size)
    {
        arena_t*           arena   = narena::create(reserved_size + 4096 + 512, initial_size + 4096 + 512);
        void*              mem1    = narena::alloc(arena, sizeof(nheap::context_t));
        nheap::context_t*  context = new (mem1) nheap::context_t();
        void*              mem2    = narena::alloc(arena, sizeof(alloc_tlsf_vmem_t));
        alloc_tlsf_vmem_t* alloc   = new (mem2) alloc_tlsf_vmem_t(context, arena);

        alloc->m_save_address = narena::current_address(arena);
        return alloc;
    }

    void g_release_heap(alloc_t* allocator)
    {
        alloc_tlsf_vmem_t* alloc = (alloc_tlsf_vmem_t*)allocator;
        if (alloc->m_arena)
        {
            narena::release(alloc->m_arena);
        }
    }

} // namespace ncore
