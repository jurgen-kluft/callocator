#include "ccore/c_target.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"
#include "cbase/c_allocator.h"

#include "callocator/c_allocator_dlmalloc.h"

namespace ncore
{
    typedef uint_t msize_t; ///< Described below */

    struct malloc_chunk
    {
        msize_t              prev_foot; ///< Size of previous chunk (if free).  */
        msize_t              head;      ///< Size and inuse bits. */
        struct malloc_chunk* fd;        ///< double links -- used only if free. */
        struct malloc_chunk* bk;
    };

    typedef struct malloc_chunk  mchunk;
    typedef struct malloc_chunk* mchunkptr;
    typedef struct malloc_chunk* sbinptr;  ///< The type of bins of chunks */
    typedef u32                  bindex_t; ///< Described below */
    typedef u32                  binmap_t; ///< Described below */
    typedef u32                  flag_t;   ///< The type of various bit flag sets */

    struct malloc_tree_chunk
    {
        ///< The first four fields must be compatible with malloc_chunk
        msize_t                   prev_foot;
        msize_t                   head;
        struct malloc_tree_chunk* fd;
        struct malloc_tree_chunk* bk;

        struct malloc_tree_chunk* child[2];
        struct malloc_tree_chunk* parent;
        bindex_t                  index;
    };

    typedef struct malloc_tree_chunk  tchunk;
    typedef struct malloc_tree_chunk* tchunkptr;
    typedef struct malloc_tree_chunk* tbinptr; ///< The type of bins of trees */

    struct malloc_segment
    {
        u8*                 base;   ///< base address */
        msize_t                size;   ///< allocated size */
        flag_t                 sflags; ///< user and extern flag */
        struct malloc_segment* next;   ///< ptr to next segment */
    };

    typedef struct malloc_segment  msegment;
    typedef struct malloc_segment* msegmentptr;

#define NSMALLBINS (32U)
#define NTREEBINS (32U)

    struct malloc_state
    {
        binmap_t  smallmap;
        binmap_t  treemap;
        msize_t   dvsize;
        msize_t   topsize;
        u8*    least_addr;
        mchunkptr dv;
        mchunkptr top;
        msize_t   release_checks;
        msize_t   magic;
        mchunkptr smallbins[(NSMALLBINS + 1) * 2];
        tbinptr   treebins[NTREEBINS];
        msize_t   footprint;
        msize_t   max_footprint;
        flag_t    mflags;
        msegment  seg;
    };

    typedef struct malloc_state* mstate;

    struct malloc_params
    {
        volatile msize_t magic;
        msize_t          page_size;
        flag_t           default_mflags;
    };

    typedef void* (*SysAllocFunc)(msize_t size);
    typedef void (*SysFreeFunc)(void* ptrsize);

    /**
     * A memory heap capable of managing multiple segments (based on dlmalloc)
     */
    class mem_heap_base_t
    {
    protected:
        malloc_params mParams;
        mstate        mState;

        SysAllocFunc mSysAlloc;
        SysFreeFunc  mSysFree;

    public:
        void*  __alloc(msize_t bytes);                                                ///< Normal allocation
        void*  __allocA(msize_t alignment, msize_t size);                             ///< Aligned allocation
        void*  __allocR(void* ptr, msize_t alignment, msize_t size);                  ///< Re allocation
        void*  __allocN(msize_t n_elements, msize_t element_size);                    ///< Elements allocation
        void** __allocIC(msize_t n_elements, msize_t element_size, void** chunks);    ///< Independent continues with equal sized elements
        void** __allocICO(msize_t n_elements, msize_t* element_sizes, void** chunks); ///< Independent continues with different size specified for every element
        u32    __free(void* ptr);

        u32 __usable_size(void* mem);

    protected:
        void*  __internal_realloc(mstate m, void* oldmem, msize_t alignment, msize_t bytes);
        void*  __internal_memalign(msize_t alignment, msize_t bytes);
        void** __internal_ic_alloc(msize_t n_elements, msize_t* sizes, s32 opts, void* chunks[]);

        void    __add_segment(void* tbase, msize_t tsize, s32 sflags);
        msize_t __release_unused_segments(mstate m);

        void* __tmalloc_large(mstate m, msize_t nb);
        void* __tmalloc_small(mstate m, msize_t nb);

        msize_t __footprint();
        msize_t __max_footprint();

        s32 __init_mparams();
        s32 __change_mparam(s32 param_number, s32 value);
    };

    /**
     * A memory heap capable of managing multiple segments (based on dlmalloc)
     */
    class mem_heap_t : public mem_heap_base_t
    {
    public:
        void __initialize();
        void __destroy();

        void __manage(void* mem, msize_t size);

        static ncore::u32 __sGetMemSize(void* mem);

    private:
        malloc_state mStateData;
    };

#define FOOTERS 1
#define INSECURE 0
#define REALLOC_ZERO_BYTES_FREES 1

#define MALLOC_ALIGNMENT ((msize_t)8)

#define DEFAULT_GRANULARITY ((msize_t)64U * (msize_t)1024U)

#define DLMALLOC_VERSION 20804

/* The maximum possible msize_t value has all bits set */
#define MAX_SIZE_T (~(msize_t)0)

#ifndef ABORT_ON_ASSERT_FAILURE
#define ABORT_ON_ASSERT_FAILURE 1
#endif /* ABORT_ON_ASSERT_FAILURE */

#ifndef PROCEED_ON_ERROR
#define PROCEED_ON_ERROR 0
#endif /* PROCEED_ON_ERROR */

#define MAX_RELEASE_CHECK_RATE 256

#define M_GRANULARITY (-2)

    /*------------------------------ internal #includes ---------------------- */

    static void FatalError() {}

#if defined(TARGET_PC)
#pragma warning(disable : 4146) /* no "unsigned" warnings */
#endif

#ifdef XMEM_HEAP_DEBUG
#if ABORT_ON_ASSERT_FAILURE
#undef ASSERT
/// TODO: These should be callbacks!!!!
#define ASSERT(x) \
    if (!(x))     \
    FatalError()
#endif
#else /* XMEM_HEAP_DEBUG */
#ifndef ASSERT
#define ASSERT(x) ((void*)0)
#endif
#define XMEM_HEAP_DEBUG 0
#endif /* XMEM_HEAP_DEBUG */

#if defined(TARGET_PC)
#define malloc_getpagesize 65536
#elif defined(TARGET_MAC)
#define malloc_getpagesize 65536
#elif defined(TARGET_WII)
#define malloc_getpagesize 65536
#elif defined(TARGET_PSP)
#define malloc_getpagesize 65536
#elif defined(TARGET_PS3)
#define malloc_getpagesize 65536
#elif defined(TARGET_360)
#define malloc_getpagesize 65536
#elif defined(TARGET_3DS)
#define malloc_getpagesize 65536
#endif

/* ------------------- msize_t and alignment properties -------------------- */

/* The byte and bit size of a msize_t */
#define SIZE_T_SIZE (sizeof(msize_t))
#define SIZE_T_BITSIZE (sizeof(msize_t) << 3)

/* Some constants coerced to msize_t */
/* Annoying but necessary to avoid errors on some platforms */
#define SIZE_T_ZERO ((msize_t)0)
#define SIZE_T_ONE ((msize_t)1)
#define SIZE_T_TWO ((msize_t)2)
#define SIZE_T_FOUR ((msize_t)4)
#define TWO_SIZE_T_SIZES (SIZE_T_SIZE << 1)
#define FOUR_SIZE_T_SIZES (SIZE_T_SIZE << 2)
#define SIX_SIZE_T_SIZES (FOUR_SIZE_T_SIZES + TWO_SIZE_T_SIZES)
#define HALF_MAX_SIZE_T (MAX_SIZE_T / 2U)

/* The bit mask value corresponding to MALLOC_ALIGNMENT */
#define CHUNK_ALIGN_MASK (MALLOC_ALIGNMENT - SIZE_T_ONE)

/* True if address a has acceptable alignment */
#define is_aligned(A) (((msize_t)((A)) & (CHUNK_ALIGN_MASK)) == 0)

/* the number of bytes to offset an address to align it */
#define align_offset(A) ((((msize_t)(A)&CHUNK_ALIGN_MASK) == 0) ? 0 : ((MALLOC_ALIGNMENT - ((msize_t)(A)&CHUNK_ALIGN_MASK)) & CHUNK_ALIGN_MASK))

    /* -------------------------- MMAP preliminaries ------------------------- */

#define USER_BIT (4U)
#define EXTERN_BIT (8U)

    /* ------------------- Chunks sizes and alignments ----------------------- */

#define MCHUNK_SIZE (sizeof(mchunk))

#if FOOTERS
#define CHUNK_OVERHEAD (TWO_SIZE_T_SIZES)
#else /* FOOTERS */
#define CHUNK_OVERHEAD (SIZE_T_SIZE)
#endif /* FOOTERS */

/* The smallest size we can malloc is an aligned minimal chunk */
#define MIN_CHUNK_SIZE ((MCHUNK_SIZE + CHUNK_ALIGN_MASK) & ~CHUNK_ALIGN_MASK)

/* conversion from malloc headers to user pointers, and back */
#define chunk2mem(p) ((void*)((u8*)(p) + TWO_SIZE_T_SIZES))
#define mem2chunk(mem) ((mchunkptr)((u8*)(mem)-TWO_SIZE_T_SIZES))
/* chunk associated with aligned address A */
#define align_as_chunk(A) (mchunkptr)((A) + align_offset(chunk2mem(A)))

/* Bounds on request (not chunk) sizes. */
#define ONE_GIGA_BYTE 1 * 1024 * 1024 * 1024
#define MAX_REQUEST (ONE_GIGA_BYTE - MIN_CHUNK_SIZE)
#define MIN_REQUEST (MIN_CHUNK_SIZE - CHUNK_OVERHEAD - SIZE_T_ONE)

/* pad request bytes into a usable size */
#define pad_request(req) (((req) + CHUNK_OVERHEAD + CHUNK_ALIGN_MASK) & ~CHUNK_ALIGN_MASK)

/* pad request, checking for minimum (but not maximum) */
#define request2size(req) (((req) < MIN_REQUEST) ? MIN_CHUNK_SIZE : pad_request(req))

    /* ------------------ Operations on head and foot fields ----------------- */

    /*
    The head field of a chunk is or'ed with PINUSE_BIT when previous
    adjacent chunk in use, and or'ed with CINUSE_BIT if this chunk is in
    use.

    FLAG4_BIT is not used by this malloc, but might be useful in extensions.
    */

#define PINUSE_BIT (SIZE_T_ONE)
#define CINUSE_BIT (SIZE_T_TWO)
#define FLAG4_BIT (SIZE_T_FOUR)
#define INUSE_BITS (PINUSE_BIT | CINUSE_BIT)
#define FLAG_BITS (PINUSE_BIT | CINUSE_BIT | FLAG4_BIT)

/* Head value for fence posts */
#define FENCEPOST_HEAD (INUSE_BITS | SIZE_T_SIZE)

/* extraction of fields from head words */
#define cinuse(p) ((p)->head & CINUSE_BIT)
#define pinuse(p) ((p)->head & PINUSE_BIT)
#define is_inuse(p) (((p)->head & INUSE_BITS) != PINUSE_BIT)

#define chunksize(p) ((p)->head & ~(FLAG_BITS))

#define clear_pinuse(p) ((p)->head &= ~PINUSE_BIT)

/* Treat space at ptr +/- offset as a chunk */
#define chunk_plus_offset(p, s) ((mchunkptr)(((u8*)(p)) + (s)))
#define chunk_minus_offset(p, s) ((mchunkptr)(((u8*)(p)) - (s)))

/* Ptr to next or previous physical malloc_chunk. */
#define next_chunk(p) ((mchunkptr)(((u8*)(p)) + ((p)->head & ~FLAG_BITS)))
#define prev_chunk(p) ((mchunkptr)(((u8*)(p)) - ((p)->prev_foot)))

/* extract next chunk's pinuse bit */
#define next_pinuse(p) ((next_chunk(p)->head) & PINUSE_BIT)

/* Get/set size at footer */
#define get_foot(p, s) (((mchunkptr)((u8*)(p) + (s)))->prev_foot)
#define set_foot(p, s) (((mchunkptr)((u8*)(p) + (s)))->prev_foot = (s))

/* Set size, pinuse bit, and foot */
#define set_size_and_pinuse_of_free_chunk(p, s) ((p)->head = (s | PINUSE_BIT), set_foot(p, s))

/* Set size, pinuse bit, foot, and clear next pinuse */
#define set_free_with_pinuse(p, s, n) (clear_pinuse(n), set_size_and_pinuse_of_free_chunk(p, s))

/* Get the internal overhead associated with chunk p */
#define overhead_for(p) (CHUNK_OVERHEAD)

/* Return true if malloced space is not necessarily cleared */
#define calloc_must_clear(p) (1)

/* A little helper macro for trees */
#define leftmost_child(t) ((t)->child[0] != 0 ? (t)->child[0] : (t)->child[1])

#define is_user_segment(S) ((S)->sflags & USER_BIT)
#define is_extern_segment(S) ((S)->sflags & EXTERN_BIT)

/* Bin types, widths and sizes */
#define SMALLBIN_SHIFT (3U)
#define SMALLBIN_WIDTH (SIZE_T_ONE << SMALLBIN_SHIFT)
#define TREEBIN_SHIFT (8U)
#define MIN_LARGE_SIZE (SIZE_T_ONE << TREEBIN_SHIFT)
#define MAX_SMALL_SIZE (MIN_LARGE_SIZE - SIZE_T_ONE)
#define MAX_SMALL_REQUEST (MAX_SMALL_SIZE - CHUNK_ALIGN_MASK - CHUNK_OVERHEAD)

/* ------------- Global malloc_state and malloc_params ------------------- */

/*
    malloc_params holds global properties, including those that can be
    dynamically set using mallopt. There is a single instance, mparams,
    initialized in init_mparams. Note that the non-zeroness of "magic"
    also serves as an initialization flag.
*/

/* Ensure mparams initialized */
#define ensure_initialization() (void)(mParams.magic != 0 || __init_mparams())
#define is_initialized(M) ((M)->top != 0)

/* -------------------------- system alloc setup ------------------------- */

/* Operations on mflags */

/* For sys_alloc, enough padding to ensure can malloc request on success */
#define SYS_ALLOC_PADDING (TOP_FOOT_SIZE + MALLOC_ALIGNMENT)

/*  True if segment S holds address A */
#define segment_holds(S, A) ((u8*)(A) >= S->base && (u8*)(A) < S->base + S->size)

    /* Return segment holding given address */
    static msegmentptr segment_holding(mstate m, u8* addr)
    {
        msegmentptr sp = &m->seg;
        for (;;)
        {
            if (addr >= sp->base && addr < sp->base + sp->size)
                return sp;
            if ((sp = sp->next) == 0)
                return 0;
        }
    }

#if 0
	/* Return true if segment contains a segment link */
	static s32 has_segment_link(mstate m, msegmentptr ss) 
	{
		msegmentptr sp = &m->seg;
		for (;;) 
		{
			if ((u8*)sp >= ss->base && (u8*)sp < ss->base + ss->size)
				return 1;
			if ((sp = sp->next) == 0)
				return 0;
		}
	}
#endif

/*
    TOP_FOOT_SIZE is padding at the end of a segment, including space
    that may be needed to place segment records and fence posts when new
    noncontiguous segments are added.
*/
#define TOP_FOOT_SIZE (align_offset(chunk2mem(0)) + pad_request(sizeof(struct malloc_segment)) + MIN_CHUNK_SIZE)

    /* -------------------------------  Hooks -------------------------------- */

    /*
        PREACTION should be defined to return 0 on success, and nonzero on
        failure. If you are not using locking, you can redefine these to do
        anything you like.
    */

#ifndef PREACTION
#define PREACTION(M) (0)
#endif /* PREACTION */

#ifndef POSTACTION
#define POSTACTION(M)
#endif /* POSTACTION */

    /*
        CORRUPTION_ERROR_ACTION is triggered upon detected bad addresses.
        USAGE_ERROR_ACTION is triggered on detected bad frees and reallocs.
        The argument p is an address that might have triggered the fault.
        It is ignored by the two predefined actions, but might be
        useful in custom actions that try to help diagnose errors.
    */

#if PROCEED_ON_ERROR

    /* A count of the number of corruption errors causing resets */
    s32 malloc_corruption_error_count;

    /* default corruption action */
    static void reset_on_error(mstate m);

#define CORRUPTION_ERROR_ACTION(m) reset_on_error(m)
#define USAGE_ERROR_ACTION(m, p)

#else /* PROCEED_ON_ERROR */

#ifndef CORRUPTION_ERROR_ACTION
/// TODO: These should be callbacks!!!!
#define CORRUPTION_ERROR_ACTION(m) FatalError()
#endif /* CORRUPTION_ERROR_ACTION */

#ifndef USAGE_ERROR_ACTION
/// TODO: These should be callbacks!!!!
#define USAGE_ERROR_ACTION(m, p) FatalError()
#endif /* USAGE_ERROR_ACTION */

#endif /* PROCEED_ON_ERROR */

    /* -------------------------- Debugging setup ---------------------------- */

#if !XMEM_HEAP_DEBUG

#define check_free_chunk(M, P)
#define check_inuse_chunk(M, R, P)
#define check_top_chunk(M, P)
#define check_malloced_chunk(M, R, P, N)
#define check_malloc_state(M, R)

#else /* XMEM_HEAP_DEBUG */

#define check_free_chunk(M, P) do_check_free_chunk(M, P)
#define check_inuse_chunk(M, R, P) do_check_inuse_chunk(M, R, P)
#define check_top_chunk(M, P) do_check_top_chunk(M, P)
#define check_malloced_chunk(M, R, P, N) do_check_malloced_chunk(M, R, P, N)
#define check_malloc_state(M, R) do_check_malloc_state(M, R)

    static void    do_check_any_chunk(mstate m, mchunkptr p);
    static void    do_check_top_chunk(mstate m, mchunkptr p);
    static void    do_check_inuse_chunk(mstate m, malloc_params& mparams, mchunkptr p);
    static void    do_check_free_chunk(mstate m, mchunkptr p);
    static void    do_check_malloced_chunk(mstate m, malloc_params& mparams, void* mem, msize_t s);
    static void    do_check_tree(mstate m, tchunkptr t);
    static void    do_check_treebin(mstate m, bindex_t i);
    static void    do_check_smallbin(mstate m, malloc_params& mparams, bindex_t i);
    static void    do_check_malloc_state(mstate m, malloc_params& mparams);
    static s32     bin_find(mstate m, mchunkptr x);
    static msize_t traverse_and_check(mstate m, malloc_params& mparams);
#endif /* XMEM_HEAP_DEBUG */

    /* ---------------------------- Indexing Bins ---------------------------- */

#define is_small(s) (((s) >> SMALLBIN_SHIFT) < NSMALLBINS)
#define small_index(s) ((s) >> SMALLBIN_SHIFT)
#define small_index2size(i) ((i) << SMALLBIN_SHIFT)
#define MIN_SMALL_INDEX (small_index(MIN_CHUNK_SIZE))

/* addressing by index. See above about small bin repositioning */
#define smallbin_at(M, i) ((sbinptr)((u8*)&((M)->smallbins[(i) << 1])))
#define treebin_at(M, i) (&((M)->treebins[i]))

#define compute_tree_index(S, I)                                        \
    {                                                                   \
        msize_t X = S >> TREEBIN_SHIFT;                                 \
        if (X == 0)                                                     \
            I = 0;                                                      \
        else if (X > 0xFFFF)                                            \
            I = NTREEBINS - 1;                                          \
        else                                                            \
        {                                                               \
            u32 Y = (u32)X;                                             \
            u32 N = ((Y - 0x100) >> 16) & 8;                            \
            u32 K = (((Y <<= N) - 0x1000) >> 16) & 4;                   \
            N += K;                                                     \
            N += K = (((Y <<= K) - 0x4000) >> 16) & 2;                  \
            K      = 14 - N + ((Y <<= K) >> 15);                        \
            I      = (K << 1) + ((S >> (K + (TREEBIN_SHIFT - 1)) & 1)); \
        }                                                               \
    }

/* Bit representing maximum resolved size in a treebin at i */
#define bit_for_tree_index(i) (i == NTREEBINS - 1) ? (SIZE_T_BITSIZE - 1) : (((i) >> 1) + TREEBIN_SHIFT - 2)

/* Shift placing maximum resolved bit in a treebin at i as sign bit */
#define leftshift_for_tree_index(i) ((i == NTREEBINS - 1) ? 0 : ((SIZE_T_BITSIZE - SIZE_T_ONE) - (((i) >> 1) + TREEBIN_SHIFT - 2)))

/* The size of the smallest chunk held in bin with index i */
#define minsize_for_tree_index(i) ((SIZE_T_ONE << (((i) >> 1) + TREEBIN_SHIFT)) | (((msize_t)((i)&SIZE_T_ONE)) << (((i) >> 1) + TREEBIN_SHIFT - 1)))

/* ------------------------ Operations on bin maps ----------------------- */

/* bit corresponding to given index */
#define idx2bit(i) ((binmap_t)(1) << (i))

/* Mark/Clear bits with given index */
#define mark_smallmap(M, i) ((M)->smallmap |= idx2bit(i))
#define clear_smallmap(M, i) ((M)->smallmap &= ~idx2bit(i))
#define smallmap_is_marked(M, i) ((M)->smallmap & idx2bit(i))

#define mark_treemap(M, i) ((M)->treemap |= idx2bit(i))
#define clear_treemap(M, i) ((M)->treemap &= ~idx2bit(i))
#define treemap_is_marked(M, i) ((M)->treemap & idx2bit(i))

/* isolate the least set bit of a bitmap */
#define least_bit(x) ((x) & -(x))

/* mask with all bits to left of least bit of x on */
#define left_bits(x) ((x << 1) | -(x << 1))

/* mask with all bits to left of or equal to least bit of x on */
#define same_or_left_bits(x) ((x) | -(x))

/* index corresponding to given bit. */
#define compute_bit2idx(X, I)       \
    {                               \
        u32 Y = X - 1;              \
        u32 K = Y >> (16 - 4) & 16; \
        u32 N = K;                  \
        Y >>= K;                    \
        N += K = Y >> (8 - 3) & 8;  \
        Y >>= K;                    \
        N += K = Y >> (4 - 2) & 4;  \
        Y >>= K;                    \
        N += K = Y >> (2 - 1) & 2;  \
        Y >>= K;                    \
        N += K = Y >> (1 - 0) & 1;  \
        Y >>= K;                    \
        I = (bindex_t)(N + Y);      \
    }

    /* ----------------------- Runtime Check Support ------------------------- */

    /*
        For security, the main invariant is that malloc/free/etc never
        writes to a static address other than malloc_state, unless static
        malloc_state itself has been corrupted, which cannot occur via
        malloc (because of these checks). In essence this means that we
        believe all pointers, sizes, maps etc held in malloc_state, but
        check all of those linked or offsetted from other embedded data
        structures. These checks are interspersed with main code in a way
        that tends to minimize their run-time cost.

        When FOOTERS is defined, in addition to range checking, we also
        verify footer fields of inuse chunks, which can be used guarantee
        that the mstate controlling malloc/free is intact.  This is a
        streamlined version of the approach described by William Robertson
        et al in "Run-time Detection of Heap-based Overflows" LISA'03
        http://www.usenix.org/events/lisa03/tech/robertson.html The footer
        of an inuse chunk holds the xor of its mstate and a random seed,
        that is checked upon calls to free() and realloc().  This is
        (probablistically) unguessable from outside the program, but can be
        computed by any code successfully malloc'ing any chunk, so does not
        itself provide protection against code that has already broken
        security through some other means.  Unlike Robertson et al, we
        always dynamically check addresses of all offset chunks (previous,
        next, etc). This turns out to be cheaper than relying on hashes.
    */

#if !INSECURE
/* Check if address a is at least as high as any from MORECORE or MMAP */
#define ok_address(M, a) ((u8*)(a) >= (M)->least_addr)
/* Check if address of next chunk n is higher than base chunk p */
#define ok_next(p, n) ((u8*)(p) < (u8*)(n))
/* Check if p has inuse status */
#define ok_inuse(p) is_inuse(p)
/* Check if p has its pinuse bit on */
#define ok_pinuse(p) pinuse(p)
#else /* !INSECURE */
#define ok_address(M, a) (1)
#define ok_next(b, n) (1)
#define ok_inuse(p) (1)
#define ok_pinuse(p) (1)
#endif /* !INSECURE */

#if (FOOTERS && !INSECURE)
/* Check if (alleged) mstate m has expected magic field */
#define ok_magic(M) ((M)->magic == mParams.magic)
#else /* (FOOTERS && !INSECURE) */
#define ok_magic(M) (1)
#endif /* (FOOTERS && !INSECURE) */

/* In gcc, use __builtin_expect to minimize impact of checks */
#if !INSECURE
#define RTCHECK(e) (e)
#else /* !INSECURE */
#define RTCHECK(e) (1)
#endif /* !INSECURE */

    /* macros to set up inuse chunks with or without footers */

#if !FOOTERS

#define mark_inuse_foot(M, p, s)

/* Macros for setting head/foot of non-mmapped chunks */

/* Set cinuse bit and pinuse bit of next chunk */
#define set_inuse(M, p, s) ((p)->head = (((p)->head & PINUSE_BIT) | s | CINUSE_BIT), ((mchunkptr)(((u8*)(p)) + (s)))->head |= PINUSE_BIT)

/* Set cinuse and pinuse of this chunk and pinuse of next chunk */
#define set_inuse_and_pinuse(M, p, s) ((p)->head = (s | PINUSE_BIT | CINUSE_BIT), ((mchunkptr)(((u8*)(p)) + (s)))->head |= PINUSE_BIT)

/* Set size, cinuse and pinuse bit of this chunk */
#define set_size_and_pinuse_of_inuse_chunk(M, p, s) ((p)->head = (s | PINUSE_BIT | CINUSE_BIT))

#else /* FOOTERS */

/* Set foot of inuse chunk to be xor of mstate and seed */
#define mark_inuse_foot(M, p, s) (((mchunkptr)((u8*)(p) + (s)))->prev_foot = ((msize_t)(M) ^ mParams.magic))

#define get_mstate_for(p) ((mstate)(((mchunkptr)((u8*)(p) + (chunksize(p))))->prev_foot ^ mParams.magic))

#define set_inuse(M, p, s) ((p)->head = (((p)->head & PINUSE_BIT) | s | CINUSE_BIT), (((mchunkptr)(((u8*)(p)) + (s)))->head |= PINUSE_BIT), mark_inuse_foot(M, p, s))

#define set_inuse_and_pinuse(M, p, s) ((p)->head = (s | PINUSE_BIT | CINUSE_BIT), (((mchunkptr)(((u8*)(p)) + (s)))->head |= PINUSE_BIT), mark_inuse_foot(M, p, s))

#define set_size_and_pinuse_of_inuse_chunk(M, p, s) ((p)->head = (s | PINUSE_BIT | CINUSE_BIT), mark_inuse_foot(M, p, s))

#endif /* !FOOTERS */

    /* ---------------------------- setting malloc_params -------------------------- */

    s32 mem_heap_base_t::__init_mparams()
    {
        nmem::memset(&mParams, 0, sizeof(malloc_params));
        {
            msize_t magic;
            msize_t psize;
            msize_t gsize;

            psize = malloc_getpagesize;
            gsize = ((DEFAULT_GRANULARITY != 0) ? DEFAULT_GRANULARITY : psize);

            /* Sanity-check configuration:
            msize_t must be unsigned and as wide as pointer type.
            ints must be at least 4 bytes.
            alignment must be at least 8.
            Alignment, min chunk size, and page size must all be powers of 2.
            */
            if ((sizeof(msize_t) != sizeof(u8*)) || (MAX_SIZE_T < MIN_CHUNK_SIZE) || (sizeof(s32) < 4) || (MALLOC_ALIGNMENT < (msize_t)8U) || ((MALLOC_ALIGNMENT & (MALLOC_ALIGNMENT - SIZE_T_ONE)) != 0) ||
                ((MCHUNK_SIZE & (MCHUNK_SIZE - SIZE_T_ONE)) != 0) || ((gsize & (gsize - SIZE_T_ONE)) != 0) || ((psize & (psize - SIZE_T_ONE)) != 0))
            {

                // TODO: This should be a callback!!!!
                FatalError();
            }

            mParams.page_size = psize;

            /* Set up lock for main malloc area */
            mState->mflags = mParams.default_mflags;

            {
                magic = (msize_t)((msize_t)(u64)(void*)&mParams ^ (msize_t)0x55555555U);
                magic |= (msize_t)8U;  /* ensure nonzero */
                magic &= ~(msize_t)7U; /* improve chances of fault for bad values */
                mParams.magic = magic;
            }
        }

        return 1;
    }

    /* support for mallopt */
    s32 mem_heap_base_t::__change_mparam(s32 param_number, s32 value)
    {
        msize_t val;
        ensure_initialization();
        val = (value == -1) ? MAX_SIZE_T : (msize_t)value;
        switch (param_number)
        {
            case M_GRANULARITY:
                if (val >= mParams.page_size && ((val & (val - 1)) == 0))
                {
                    return 1;
                }
                else
                    return 0;
            default: return 0;
        }
    }

#if XMEM_HEAP_DEBUG
    /* ------------------------- Debugging Support --------------------------- */

    /* Check properties of any chunk, whether free, inuse, mmapped etc  */
    static void do_check_any_chunk(mstate m, mchunkptr p)
    {
        ASSERT((is_aligned(chunk2mem(p))) || (p->head == FENCEPOST_HEAD));
        ASSERT(ok_address(m, p));
    }

    /* Check properties of top chunk */
    static void do_check_top_chunk(mstate m, mchunkptr p)
    {
        msegmentptr sp = segment_holding(m, (u8*)p);
        msize_t     sz = p->head & ~INUSE_BITS; /* third-lowest bit can be set! */
        ASSERT(sp != 0);
        ASSERT((is_aligned(chunk2mem(p))) || (p->head == FENCEPOST_HEAD));
        ASSERT(ok_address(m, p));
        ASSERT(sz == m->topsize);
        ASSERT(sz > 0);
        ASSERT(sz == ((sp->base + sp->size) - (u8*)p) - TOP_FOOT_SIZE);
        ASSERT(pinuse(p));
        ASSERT(!pinuse(chunk_plus_offset(p, sz)));
    }

    /* Check properties of inuse chunks */
    static void do_check_inuse_chunk(mstate m, malloc_params& mparams, mchunkptr p)
    {
        do_check_any_chunk(m, p);
        ASSERT(is_inuse(p));
        ASSERT(next_pinuse(p));
        /* If not pinuse and not mmapped, previous chunk has OK offset */
        ASSERT(pinuse(p) || next_chunk(prev_chunk(p)) == p);
    }

    /* Check properties of free chunks */
    static void do_check_free_chunk(mstate m, mchunkptr p)
    {
        msize_t   sz   = chunksize(p);
        mchunkptr next = chunk_plus_offset(p, sz);
        do_check_any_chunk(m, p);
        ASSERT(!is_inuse(p));
        ASSERT(!next_pinuse(p));
        if (p != m->dv && p != m->top)
        {
            if (sz >= MIN_CHUNK_SIZE)
            {
                ASSERT((sz & CHUNK_ALIGN_MASK) == 0);
                ASSERT(is_aligned(chunk2mem(p)));
                ASSERT(next->prev_foot == sz);
                ASSERT(pinuse(p));
                ASSERT(next == m->top || is_inuse(next));
                ASSERT(p->fd->bk == p);
                ASSERT(p->bk->fd == p);
            }
            else /* markers are always of size SIZE_T_SIZE */
                ASSERT(sz == SIZE_T_SIZE);
        }
    }

    /* Check properties of malloced chunks at the point they are malloced */
    static void do_check_malloced_chunk(mstate m, malloc_params& mparams, void* mem, msize_t s)
    {
        if (mem != 0)
        {
            mchunkptr p  = mem2chunk(mem);
            msize_t   sz = p->head & ~INUSE_BITS;
            do_check_inuse_chunk(m, mparams, p);
            ASSERT((sz & CHUNK_ALIGN_MASK) == 0);
            ASSERT(sz >= MIN_CHUNK_SIZE);
            ASSERT(sz >= s);
            /* unless mmapped, size is less than MIN_CHUNK_SIZE more than request */
            ASSERT(sz < (s + MIN_CHUNK_SIZE));
        }
    }

    /* Check a tree and its subtrees.  */
    static void do_check_tree(mstate m, tchunkptr t)
    {
        tchunkptr head   = 0;
        tchunkptr u      = t;
        bindex_t  tindex = t->index;
        msize_t   tsize  = chunksize(t);
        bindex_t  idx;
        compute_tree_index(tsize, idx);
        ASSERT(tindex == idx);
        ASSERT(tsize >= MIN_LARGE_SIZE);
        ASSERT(tsize >= minsize_for_tree_index(idx));
        ASSERT((idx == NTREEBINS - 1) || (tsize < minsize_for_tree_index((idx + 1))));

        do
        { /* traverse through chain of same-sized nodes */
            do_check_any_chunk(m, ((mchunkptr)u));
            ASSERT(u->index == tindex);
            ASSERT(chunksize(u) == tsize);
            ASSERT(!is_inuse(u));
            ASSERT(!next_pinuse(u));
            ASSERT(u->fd->bk == u);
            ASSERT(u->bk->fd == u);
            if (u->parent == 0)
            {
                ASSERT(u->child[0] == 0);
                ASSERT(u->child[1] == 0);
            }
            else
            {
                ASSERT(head == 0); /* only one node on chain has parent */
                head = u;
                ASSERT(u->parent != u);
                ASSERT(u->parent->child[0] == u || u->parent->child[1] == u || *((tbinptr*)(u->parent)) == u);
                if (u->child[0] != 0)
                {
                    ASSERT(u->child[0]->parent == u);
                    ASSERT(u->child[0] != u);
                    do_check_tree(m, u->child[0]);
                }
                if (u->child[1] != 0)
                {
                    ASSERT(u->child[1]->parent == u);
                    ASSERT(u->child[1] != u);
                    do_check_tree(m, u->child[1]);
                }
                if (u->child[0] != 0 && u->child[1] != 0)
                {
                    ASSERT(chunksize(u->child[0]) < chunksize(u->child[1]));
                }
            }
            u = u->fd;
        } while (u != t);
        ASSERT(head != 0);
    }

    /*  Check all the chunks in a treebin.  */
    static void do_check_treebin(mstate m, bindex_t i)
    {
        tbinptr*  tb    = treebin_at(m, i);
        tchunkptr t     = *tb;
        s32       empty = (m->treemap & (1U << i)) == 0;
        if (t == 0)
            ASSERT(empty);
        if (empty == 0)
            do_check_tree(m, t);
    }

    /*  Check all the chunks in a smallbin.  */
    static void do_check_smallbin(mstate m, malloc_params& mparams, bindex_t i)
    {
        sbinptr   b     = smallbin_at(m, i);
        mchunkptr p     = b->bk;
        u32       empty = (m->smallmap & (1U << i)) == 0;
        if (p == b)
            ASSERT(empty);

        if (empty == 0)
        {
            for (; p != b; p = p->bk)
            {
                msize_t   size = chunksize(p);
                mchunkptr q;
                /* each chunk claims to be free */
                do_check_free_chunk(m, p);
                /* chunk belongs in bin */
                ASSERT(small_index(size) == i);
                ASSERT(p->bk == b || chunksize(p->bk) == chunksize(p));
                /* chunk is followed by an inuse chunk */
                q = next_chunk(p);
                if (q->head != FENCEPOST_HEAD)
                    do_check_inuse_chunk(m, mparams, q);
            }
        }
    }

    /* Find x in a bin. Used in other check functions. */
    static s32 bin_find(mstate m, mchunkptr x)
    {
        msize_t size = chunksize(x);
        if (is_small(size))
        {
            bindex_t sidx = small_index(size);
            sbinptr  b    = smallbin_at(m, sidx);
            if (smallmap_is_marked(m, sidx))
            {
                mchunkptr p = b;
                do
                {
                    if (p == x)
                        return 1;
                } while ((p = p->fd) != b);
            }
        }
        else
        {
            bindex_t tidx;
            compute_tree_index(size, tidx);
            if (treemap_is_marked(m, tidx))
            {
                tchunkptr t        = *treebin_at(m, tidx);
                msize_t   sizebits = size << leftshift_for_tree_index(tidx);
                while (t != 0 && chunksize(t) != size)
                {
                    t = t->child[(sizebits >> (SIZE_T_BITSIZE - SIZE_T_ONE)) & 1];
                    sizebits <<= 1;
                }
                if (t != 0)
                {
                    tchunkptr u = t;
                    do
                    {
                        if (u == (tchunkptr)x)
                            return 1;
                    } while ((u = u->fd) != t);
                }
            }
        }
        return 0;
    }

    /* Traverse each chunk and check it; return total */
    static msize_t traverse_and_check(mstate m, malloc_params& mparams)
    {
        msize_t sum = 0;
        if (is_initialized(m))
        {
            msegmentptr s = &m->seg;
            sum += m->topsize + TOP_FOOT_SIZE;
            while (s != 0)
            {
                mchunkptr q     = align_as_chunk(s->base);
                mchunkptr lastq = 0;
                ASSERT(pinuse(q));
                while (segment_holds(s, q) && q != m->top && q->head != FENCEPOST_HEAD)
                {
                    sum += chunksize(q);
                    if (is_inuse(q))
                    {
                        ASSERT(!bin_find(m, q));
                        do_check_inuse_chunk(m, mparams, q);
                    }
                    else
                    {
                        ASSERT(q == m->dv || bin_find(m, q));
                        ASSERT(lastq == 0 || is_inuse(lastq)); /* Not 2 consecutive free */
                        do_check_free_chunk(m, q);
                    }
                    lastq = q;
                    q     = next_chunk(q);
                }
                s = s->next;
            }
        }
        return sum;
    }

    /* Check all properties of malloc_state. */
    static void do_check_malloc_state(mstate m, malloc_params& mparams)
    {
        bindex_t i;
        msize_t  total;
        /* check bins */
        for (i = 0; i < NSMALLBINS; ++i)
            do_check_smallbin(m, mparams, i);
        for (i = 0; i < NTREEBINS; ++i)
            do_check_treebin(m, i);

        if (m->dvsize != 0)
        { /* check dv chunk */
            do_check_any_chunk(m, m->dv);
            ASSERT(m->dvsize == chunksize(m->dv));
            ASSERT(m->dvsize >= MIN_CHUNK_SIZE);
            ASSERT(bin_find(m, m->dv) == 0);
        }

        if (m->top != 0)
        { /* check top chunk */
            do_check_top_chunk(m, m->top);
            /*ASSERT(m->topsize == chunksize(m->top)); redundant */
            ASSERT(m->topsize > 0);
            ASSERT(bin_find(m, m->top) == 0);
        }

        total = traverse_and_check(m, mparams);
        ASSERT(total <= m->footprint);
        ASSERT(m->footprint <= m->max_footprint);
    }

#endif ///< XMEM_HEAP_DEBUG

    /* ----------------------------- statistics ------------------------------ */

    //	void mem_heap_base_t::__internal_malloc_stats(xmem_managed_size& stats)
    //	{
    //		mstate m = mState;
    //
    //		ensure_initialization();
    //		if (!PREACTION(m))
    //		{
    //			msize_t maxfp = 0;
    //			msize_t fp = 0;
    //			msize_t used = 0;
    //			check_malloc_state(m, mParams);
    //			if (is_initialized(m))
    //			{
    //				msegmentptr s = &m->seg;
    //				maxfp = m->max_footprint;
    //				fp = m->footprint;
    //				used = fp - (m->topsize + TOP_FOOT_SIZE);
    //
    //				while (s != 0)
    //				{
    //					mchunkptr q = align_as_chunk(s->base);
    //					while (segment_holds(s, q) && q != m->top && q->head != FENCEPOST_HEAD)
    //					{
    //						if (!is_inuse(q))
    //							used -= chunksize(q);
    //						q = next_chunk(q);
    //					}
    //					s = s->next;
    //				}
    //			}
    //
    //			stats.mCurrentInuseSize = used;
    //			stats.mCurrentSystemSize = fp;
    //			stats.mMaxSystemSize = maxfp;
    // 			console_t::writeLine("max system bytes = %10lu", va_list_t((u32)(maxfp)));
    // 			console_t::writeLine("system bytes     = %10lu", va_list_t((u32)(fp)));
    // 			console_t::writeLine("in use bytes     = %10lu", va_list_t((u32)(used)));
    //
    //			POSTACTION(m);
    //		}
    //	}
    //
/* ----------------------- Operations on smallbins ----------------------- */

/*
Various forms of linking and unlinking are defined as macros.  Even
the ones for trees, which are very long but have very short typical
paths.  This is ugly but reduces reliance on inlining support of
compilers.
*/

/* Link a free chunk into a small bin  */
#define insert_small_chunk(M, P, S)             \
    {                                           \
        bindex_t  I = small_index(S);           \
        mchunkptr B = smallbin_at(M, I);        \
        mchunkptr F = B;                        \
        ASSERT(S >= MIN_CHUNK_SIZE);            \
        if (!smallmap_is_marked(M, I))          \
            mark_smallmap(M, I);                \
        else if (RTCHECK(ok_address(M, B->fd))) \
            F = B->fd;                          \
        else                                    \
        {                                       \
            CORRUPTION_ERROR_ACTION(M);         \
        }                                       \
        B->fd = P;                              \
        F->bk = P;                              \
        P->fd = F;                              \
        P->bk = B;                              \
    }

/* Unlink a chunk from a small bin  */
#define unlink_small_chunk(M, P, S)                                                                                     \
    {                                                                                                                   \
        mchunkptr F = P->fd;                                                                                            \
        mchunkptr B = P->bk;                                                                                            \
        bindex_t  I = small_index(S);                                                                                   \
        ASSERT(P != B);                                                                                                 \
        ASSERT(P != F);                                                                                                 \
        ASSERT(chunksize(P) == small_index2size(I));                                                                    \
        if (F == B)                                                                                                     \
            clear_smallmap(M, I);                                                                                       \
        else if (RTCHECK((F == smallbin_at(M, I) || ok_address(M, F)) && (B == smallbin_at(M, I) || ok_address(M, B)))) \
        {                                                                                                               \
            F->bk = B;                                                                                                  \
            B->fd = F;                                                                                                  \
        }                                                                                                               \
        else                                                                                                            \
        {                                                                                                               \
            CORRUPTION_ERROR_ACTION(M);                                                                                 \
        }                                                                                                               \
    }

/* Unlink the first chunk from a small bin */
#define unlink_first_small_chunk(M, B, P, I)         \
    {                                                \
        mchunkptr F = P->fd;                         \
        ASSERT(P != B);                              \
        ASSERT(P != F);                              \
        ASSERT(chunksize(P) == small_index2size(I)); \
        if (B == F)                                  \
            clear_smallmap(M, I);                    \
        else if (RTCHECK(ok_address(M, F)))          \
        {                                            \
            B->fd = F;                               \
            F->bk = B;                               \
        }                                            \
        else                                         \
        {                                            \
            CORRUPTION_ERROR_ACTION(M);              \
        }                                            \
    }

/* Replace dv node, binning the old one */
/* Used only when dvsize known to be small */
#define replace_dv(M, P, S)                 \
    {                                       \
        msize_t DVS = M->dvsize;            \
        if (DVS != 0)                       \
        {                                   \
            mchunkptr DV = M->dv;           \
            ASSERT(is_small(DVS));          \
            insert_small_chunk(M, DV, DVS); \
        }                                   \
        M->dvsize = S;                      \
        M->dv     = P;                      \
    }

/* ------------------------- Operations on trees ------------------------- */

/* Insert chunk into tree */
#define insert_large_chunk(M, X, S)                                                       \
    {                                                                                     \
        tbinptr* H;                                                                       \
        bindex_t I;                                                                       \
        compute_tree_index(S, I);                                                         \
        H           = treebin_at(M, I);                                                   \
        X->index    = I;                                                                  \
        X->child[0] = X->child[1] = 0;                                                    \
        if (!treemap_is_marked(M, I))                                                     \
        {                                                                                 \
            mark_treemap(M, I);                                                           \
            *H        = X;                                                                \
            X->parent = (tchunkptr)H;                                                     \
            X->fd = X->bk = X;                                                            \
        }                                                                                 \
        else                                                                              \
        {                                                                                 \
            tchunkptr T = *H;                                                             \
            msize_t   K = S << leftshift_for_tree_index(I);                               \
            for (;;)                                                                      \
            {                                                                             \
                if (chunksize(T) != S)                                                    \
                {                                                                         \
                    tchunkptr* C = &(T->child[(K >> (SIZE_T_BITSIZE - SIZE_T_ONE)) & 1]); \
                    K <<= 1;                                                              \
                    if (*C != 0)                                                          \
                        T = *C;                                                           \
                    else if (RTCHECK(ok_address(M, C)))                                   \
                    {                                                                     \
                        *C        = X;                                                    \
                        X->parent = T;                                                    \
                        X->fd = X->bk = X;                                                \
                        break;                                                            \
                    }                                                                     \
                    else                                                                  \
                    {                                                                     \
                        CORRUPTION_ERROR_ACTION(M);                                       \
                        break;                                                            \
                    }                                                                     \
                }                                                                         \
                else                                                                      \
                {                                                                         \
                    tchunkptr F = T->fd;                                                  \
                    if (RTCHECK(ok_address(M, T) && ok_address(M, F)))                    \
                    {                                                                     \
                        T->fd = F->bk = X;                                                \
                        X->fd         = F;                                                \
                        X->bk         = T;                                                \
                        X->parent     = 0;                                                \
                        break;                                                            \
                    }                                                                     \
                    else                                                                  \
                    {                                                                     \
                        CORRUPTION_ERROR_ACTION(M);                                       \
                        break;                                                            \
                    }                                                                     \
                }                                                                         \
            }                                                                             \
        }                                                                                 \
    }

    /*
    Unlink steps:

    1. If x is a chained node, unlink it from its same-sized fd/bk links
    and choose its bk node as its replacement.
    2. If x was the last node of its size, but not a leaf node, it must
    be replaced with a leaf node (not merely one with an open left or
    right), to make sure that lefts and rights of descendents
    correspond properly to bit masks.  We use the rightmost descendent
    of x.  We could use any other leaf, but this is easy to locate and
    tends to counteract removal of leftmosts elsewhere, and so keeps
    paths shorter than minimally guaranteed.  This doesn't loop much
    because on average a node in a tree is near the bottom.
    3. If x is the base of a chain (i.e., has parent links) relink
    x's parent and children to x's replacement (or null if none).
    */

#define unlink_large_chunk(M, X)                                                            \
    {                                                                                       \
        tchunkptr XP = X->parent;                                                           \
        tchunkptr R;                                                                        \
        if (X->bk != X)                                                                     \
        {                                                                                   \
            tchunkptr F = X->fd;                                                            \
            R           = X->bk;                                                            \
            if (RTCHECK(ok_address(M, F)))                                                  \
            {                                                                               \
                F->bk = R;                                                                  \
                R->fd = F;                                                                  \
            }                                                                               \
            else                                                                            \
            {                                                                               \
                CORRUPTION_ERROR_ACTION(M);                                                 \
            }                                                                               \
        }                                                                                   \
        else                                                                                \
        {                                                                                   \
            tchunkptr* RP;                                                                  \
            if (((R = *(RP = &(X->child[1]))) != 0) || ((R = *(RP = &(X->child[0]))) != 0)) \
            {                                                                               \
                tchunkptr* CP;                                                              \
                while ((*(CP = &(R->child[1])) != 0) || (*(CP = &(R->child[0])) != 0))      \
                {                                                                           \
                    R = *(RP = CP);                                                         \
                }                                                                           \
                if (RTCHECK(ok_address(M, RP)))                                             \
                    *RP = 0;                                                                \
                else                                                                        \
                {                                                                           \
                    CORRUPTION_ERROR_ACTION(M);                                             \
                }                                                                           \
            }                                                                               \
        }                                                                                   \
        if (XP != 0)                                                                        \
        {                                                                                   \
            tbinptr* H = treebin_at(M, X->index);                                           \
            if (X == *H)                                                                    \
            {                                                                               \
                if ((*H = R) == 0)                                                          \
                    clear_treemap(M, X->index);                                             \
            }                                                                               \
            else if (RTCHECK(ok_address(M, XP)))                                            \
            {                                                                               \
                if (XP->child[0] == X)                                                      \
                    XP->child[0] = R;                                                       \
                else                                                                        \
                    XP->child[1] = R;                                                       \
            }                                                                               \
            else                                                                            \
                CORRUPTION_ERROR_ACTION(M);                                                 \
            if (R != 0)                                                                     \
            {                                                                               \
                if (RTCHECK(ok_address(M, R)))                                              \
                {                                                                           \
                    tchunkptr C0, C1;                                                       \
                    R->parent = XP;                                                         \
                    if ((C0 = X->child[0]) != 0)                                            \
                    {                                                                       \
                        if (RTCHECK(ok_address(M, C0)))                                     \
                        {                                                                   \
                            R->child[0] = C0;                                               \
                            C0->parent  = R;                                                \
                        }                                                                   \
                        else                                                                \
                            CORRUPTION_ERROR_ACTION(M);                                     \
                    }                                                                       \
                    if ((C1 = X->child[1]) != 0)                                            \
                    {                                                                       \
                        if (RTCHECK(ok_address(M, C1)))                                     \
                        {                                                                   \
                            R->child[1] = C1;                                               \
                            C1->parent  = R;                                                \
                        }                                                                   \
                        else                                                                \
                            CORRUPTION_ERROR_ACTION(M);                                     \
                    }                                                                       \
                }                                                                           \
                else                                                                        \
                    CORRUPTION_ERROR_ACTION(M);                                             \
            }                                                                               \
        }                                                                                   \
    }

    /* Relays to large vs small bin operations */

#define insert_chunk(M, P, S)              \
    if (is_small(S))                       \
        insert_small_chunk(M, P, S) else   \
        {                                  \
            tchunkptr TP = (tchunkptr)(P); \
            insert_large_chunk(M, TP, S);  \
        }

#define unlink_chunk(M, P, S)              \
    if (is_small(S))                       \
        unlink_small_chunk(M, P, S) else   \
        {                                  \
            tchunkptr TP = (tchunkptr)(P); \
            unlink_large_chunk(M, TP);     \
        }

    /* Relays to internal calls to malloc/free from realloc, memalign etc */

    /* -------------------------- xmem_space management -------------------------- */

    /* Initialize top chunk and its size */
    static void init_top(mstate m, mchunkptr p, msize_t psize)
    {
        /* Ensure alignment */
        msize_t offset = align_offset(chunk2mem(p));
        p              = (mchunkptr)((u8*)p + offset);
        psize -= offset;

        m->top     = p;
        m->topsize = psize;
        p->head    = psize | PINUSE_BIT;
        /* set size of fake trailing chunk holding overhead space only once */
        chunk_plus_offset(p, psize)->head = TOP_FOOT_SIZE;
    }

    /* Initialize bins for a new mstate that is otherwise zeroed out */
    static void init_bins(mstate m)
    {
        /* Establish circular links for smallbins */
        bindex_t i;
        for (i = 0; i < NSMALLBINS; ++i)
        {
            sbinptr bin = smallbin_at(m, i);
            bin->fd = bin->bk = bin;
        }
    }

#if PROCEED_ON_ERROR

    /* default corruption action */
    static void reset_on_error(mstate m)
    {
        s32 i;
        ++malloc_corruption_error_count;
        /* Reinitialize fields to forget about all memory */
        m->smallbins = m->treebins = 0;
        m->dvsize = m->topsize = 0;
        m->seg.base            = 0;
        m->seg.size            = 0;
        m->seg.next            = 0;
        m->top = m->dv = 0;
        for (i = 0; i < NTREEBINS; ++i)
            *treebin_at(m, i) = 0;
        init_bins(m);
    }

#endif /* PROCEED_ON_ERROR */

    /* Add a segment to hold a new noncontiguous region */
    void mem_heap_base_t::__add_segment(void* tbase, msize_t tsize, s32 sflags)
    {
        mstate m = mState;

        /* Determine locations and sizes of segment, fenceposts, old top */
        u8*      old_top = (u8*)m->top;
        msegmentptr oldsp   = segment_holding(m, old_top);
        u8*      old_end = oldsp->base + oldsp->size;
        msize_t     ssize   = pad_request(sizeof(struct malloc_segment));
        u8*      rawsp   = old_end - (ssize + FOUR_SIZE_T_SIZES + CHUNK_ALIGN_MASK);
        msize_t     offset  = align_offset(chunk2mem(rawsp));
        u8*      asp     = rawsp + offset;
        u8*      csp     = (asp < (old_top + MIN_CHUNK_SIZE)) ? old_top : asp;
        mchunkptr   sp      = (mchunkptr)csp;
        msegmentptr ss      = (msegmentptr)(chunk2mem(sp));
        mchunkptr   tnext   = chunk_plus_offset(sp, ssize);
        mchunkptr   p       = tnext;
        s32         nfences = 0;

        /* reset top to new space */
        init_top(m, (mchunkptr)tbase, tsize - TOP_FOOT_SIZE);

        /* Set up segment record */
        ASSERT(is_aligned(ss));
        set_size_and_pinuse_of_inuse_chunk(m, sp, ssize);
        *ss           = m->seg; /* Push current record */
        m->seg.base   = (u8*)tbase;
        m->seg.size   = tsize;
        m->seg.next   = ss;
        m->seg.sflags = sflags;

        /* Insert trailing fenceposts */
        for (;;)
        {
            mchunkptr nextp = chunk_plus_offset(p, SIZE_T_SIZE);
            p->head         = FENCEPOST_HEAD;
            ++nfences;
            if ((u8*)(&(nextp->head)) < old_end)
                p = nextp;
            else
                break;
        }
        ASSERT(nfences >= 2);

        /* Insert the rest of old top into a bin as an ordinary free chunk */
        if (csp != old_top)
        {
            mchunkptr q     = (mchunkptr)old_top;
            msize_t   psize = (msize_t)(csp - old_top);
            mchunkptr tn    = chunk_plus_offset(q, psize);
            set_free_with_pinuse(q, psize, tn);
            insert_chunk(m, q, psize);
        }

        check_top_chunk(m, m->top);
    }

    msize_t mem_heap_base_t::__release_unused_segments(mstate m)
    {
        msize_t     released = 0;
        int         nsegs    = 0;
        msegmentptr pred     = &m->seg;
        msegmentptr sp       = pred->next;
        while (sp != 0)
        {
            u8*      base = sp->base;
            msize_t     size = sp->size;
            msegmentptr next = sp->next;
            ++nsegs;
            if (is_extern_segment(sp))
            {
                mchunkptr p     = align_as_chunk(base);
                msize_t   psize = chunksize(p);

                /* Can free if first chunk holds entire segment and not pinned */
                if (!is_inuse(p) && (u8*)p + psize >= base + size - TOP_FOOT_SIZE)
                {
                    tchunkptr tp = (tchunkptr)p;
                    ASSERT(segment_holds(sp, (u8*)sp));
                    if (p == m->dv)
                    {
                        m->dv     = 0;
                        m->dvsize = 0;
                    }
                    else
                    {
                        unlink_large_chunk(m, tp);
                    }

                    if (mSysFree != nullptr)
                    {
                        mSysFree(base);

                        released += size;
                        m->footprint -= size;
                        /* unlink obsoleted record */
                        sp       = pred;
                        sp->next = next;
                    }
                    else
                    { /* back out if cannot free */
                        insert_large_chunk(m, tp, psize);
                    }
                }
            }
            pred = sp;
            sp   = next;
        }

        /* Reset check counter */
        m->release_checks = ((nsegs > MAX_RELEASE_CHECK_RATE) ? nsegs : MAX_RELEASE_CHECK_RATE);
        return released;
    }

    /* -----------------------  Initialization of mstate ----------------------- */

    /*
        Initialize global mstate from a 'given' memory block
    */

    void mem_heap_t::__initialize()
    {
        mSysAlloc = nullptr;
        mSysFree  = nullptr;

        nmem::memset(&mStateData, 0, sizeof(malloc_state));
        mState = &mStateData;

        __init_mparams();
    }

    void mem_heap_t::__destroy()
    {
        // Release all segments that where obtained from the system
        __release_unused_segments(mState);
    }

    /* mstate, give block of memory*/
    void mem_heap_t::__manage(void* block, msize_t nb)
    {
        nmem::memset(block, 0, nb);

        mstate m = mState;

        u8*  tbase = 0;
        msize_t tsize = 0;
        {
            msegmentptr ss    = (m->top == 0) ? 0 : segment_holding(m, (u8*)m->top);
            msize_t     asize = 0;

            u8* base = (u8*)block;
            asize       = nb;

            tbase = base;
            tsize = asize;
        }

        if ((m->footprint += tsize) > m->max_footprint)
            m->max_footprint = m->footprint;

        if (!is_initialized(m))
        { /* first-time initialization */
            if (m->least_addr == 0 || tbase < m->least_addr)
                m->least_addr = tbase;
            m->seg.base       = tbase;
            m->seg.size       = tsize;
            m->seg.sflags     = 0;
            m->magic          = mParams.magic;
            m->release_checks = MAX_RELEASE_CHECK_RATE;
            init_bins(m);
            init_top(m, (mchunkptr)tbase, tsize - TOP_FOOT_SIZE);
        }
        else
        {
            if (tbase < m->least_addr)
                m->least_addr = tbase;
            __add_segment(tbase, tsize, USER_BIT);
        }
    }

    /* ---------------------------- malloc support --------------------------- */

    /* allocate a large request from the best fitting chunk in a treebin */
    void* mem_heap_base_t::__tmalloc_large(mstate m, msize_t nb)
    {
        tchunkptr v     = 0;
        msize_t   rsize = -nb; /* Unsigned negation */
        tchunkptr t;
        bindex_t  idx;
        compute_tree_index(nb, idx);
        if ((t = *treebin_at(m, idx)) != 0)
        {
            /* Traverse tree for this bin looking for node with size == nb */
            msize_t   sizebits = nb << leftshift_for_tree_index(idx);
            tchunkptr rst      = 0; /* The deepest untaken right subtree */
            for (;;)
            {
                tchunkptr rt;
                msize_t   trem = chunksize(t) - nb;
                if (trem < rsize)
                {
                    v = t;
                    if ((rsize = trem) == 0)
                        break;
                }
                rt = t->child[1];
                t  = t->child[(sizebits >> (SIZE_T_BITSIZE - SIZE_T_ONE)) & 1];
                if (rt != 0 && rt != t)
                    rst = rt;
                if (t == 0)
                {
                    t = rst; /* set t to least subtree holding sizes > nb */
                    break;
                }
                sizebits <<= 1;
            }
        }
        if (t == 0 && v == 0)
        { /* set t to root of next non-empty treebin */
            binmap_t leftbits = left_bits(idx2bit(idx)) & m->treemap;
            if (leftbits != 0)
            {
                bindex_t i;
                binmap_t leastbit = least_bit(leftbits);
                compute_bit2idx(leastbit, i);
                t = *treebin_at(m, i);
            }
        }

        while (t != 0)
        { /* find smallest of tree or subtree */
            msize_t trem = chunksize(t) - nb;
            if (trem < rsize)
            {
                rsize = trem;
                v     = t;
            }
            t = leftmost_child(t);
        }

        /*  If dv is a better fit, return 0 so malloc will use it */
        if (v != 0 && rsize < (msize_t)(m->dvsize - nb))
        {
            if (RTCHECK(ok_address(m, v)))
            { /* split */
                mchunkptr r = chunk_plus_offset(v, nb);
                ASSERT(chunksize(v) == rsize + nb);
                if (RTCHECK(ok_next(v, r)))
                {
                    unlink_large_chunk(m, v);
                    if (rsize < MIN_CHUNK_SIZE)
                        set_inuse_and_pinuse(m, v, (rsize + nb));
                    else
                    {
                        set_size_and_pinuse_of_inuse_chunk(m, v, nb);
                        set_size_and_pinuse_of_free_chunk(r, rsize);
                        insert_chunk(m, r, rsize);
                    }
                    return chunk2mem(v);
                }
            }
            CORRUPTION_ERROR_ACTION(m);
        }
        return 0;
    }

    /* allocate a small request from the best fitting chunk in a treebin */
    void* mem_heap_base_t::__tmalloc_small(mstate m, msize_t nb)
    {
        tchunkptr t, v;
        msize_t   rsize;
        bindex_t  i;
        binmap_t  leastbit = least_bit(m->treemap);
        compute_bit2idx(leastbit, i);
        v = t = *treebin_at(m, i);
        rsize = chunksize(t) - nb;

        while ((t = leftmost_child(t)) != 0)
        {
            msize_t trem = chunksize(t) - nb;
            if (trem < rsize)
            {
                rsize = trem;
                v     = t;
            }
        }

        if (RTCHECK(ok_address(m, v)))
        {
            mchunkptr r = chunk_plus_offset(v, nb);
            ASSERT(chunksize(v) == rsize + nb);
            if (RTCHECK(ok_next(v, r)))
            {
                unlink_large_chunk(m, v);
                if (rsize < MIN_CHUNK_SIZE)
                {
                    set_inuse_and_pinuse(m, v, (rsize + nb));
                }
                else
                {
                    set_size_and_pinuse_of_inuse_chunk(m, v, nb);
                    set_size_and_pinuse_of_free_chunk(r, rsize);
                    replace_dv(m, r, rsize);
                }
                return chunk2mem(v);
            }
        }

        CORRUPTION_ERROR_ACTION(m);
        return 0;
    }

    /* --------------------------- realloc support --------------------------- */

    void* mem_heap_base_t::__internal_realloc(mstate m, void* oldmem, msize_t alignment, msize_t bytes)
    {
        if (bytes >= MAX_REQUEST)
        {
            return 0;
        }

        if (!PREACTION(m))
        {
            mchunkptr oldp    = mem2chunk(oldmem);
            msize_t   oldsize = chunksize(oldp);
            mchunkptr next    = chunk_plus_offset(oldp, oldsize);
            mchunkptr newp    = 0;
            void*     extra   = 0;

            /* Try to either shrink or extend into top. Else malloc-copy-free */

            if (RTCHECK(ok_address(m, oldp) && ok_inuse(oldp) && ok_next(oldp, next) && ok_pinuse(next)))
            {
                msize_t nb = request2size(bytes);
                if (oldsize >= nb)
                { /* already big enough */
                    msize_t rsize = oldsize - nb;
                    newp          = oldp;
                    if (rsize >= MIN_CHUNK_SIZE)
                    {
                        mchunkptr remainder = chunk_plus_offset(newp, nb);
                        set_inuse(m, newp, nb);
                        set_inuse_and_pinuse(m, remainder, rsize);
                        extra = chunk2mem(remainder);
                    }
                }
                else if (next == m->top && oldsize + m->topsize > nb)
                {
                    /* Expand into top */
                    msize_t   newsize    = oldsize + m->topsize;
                    msize_t   newtopsize = newsize - nb;
                    mchunkptr newtop     = chunk_plus_offset(oldp, nb);
                    set_inuse(m, oldp, nb);
                    newtop->head = newtopsize | PINUSE_BIT;
                    m->top       = newtop;
                    m->topsize   = newtopsize;
                    newp         = oldp;
                }
            }
            else
            {
                USAGE_ERROR_ACTION(m, oldmem);
                POSTACTION(m);
                return 0;
            }
#if XMEM_HEAP_DEBUG
            if (newp != 0)
            {
                check_inuse_chunk(m, mParams, newp); /* Check requires lock */
            }
#endif

            POSTACTION(m);

            void* newmem;
            if (newp != 0)
            {
                if (extra != 0)
                {
                    __free(extra);
                }
                newmem = chunk2mem(newp);
            }
            else
            {
                newmem = __allocA(alignment, bytes);
                if (newmem != 0)
                {
                    msize_t oc = oldsize - overhead_for(oldp);
                    nmem::memcpy(newmem, oldmem, (oc < bytes) ? oc : bytes);
                    __free(oldmem);
                }
            }

            return newmem;
        }
        else
        {
            return 0;
        }
    }

    /* --------------------------- memalign support -------------------------- */

    void* mem_heap_base_t::__internal_memalign(msize_t alignment, msize_t bytes)
    {
        mstate m = mState;

        if (alignment <= MALLOC_ALIGNMENT) /* Can just use malloc */
            return __alloc(bytes);

        if (alignment < MIN_CHUNK_SIZE) /* must be at least a minimum chunk size */
            alignment = MIN_CHUNK_SIZE;

        if ((alignment & (alignment - SIZE_T_ONE)) != 0)
        {
            /* Ensure a power of 2 */
            msize_t a = MALLOC_ALIGNMENT << 1;
            while (a < alignment)
                a <<= 1;
            alignment = a;
        }

        if (bytes >= MAX_REQUEST - alignment)
        {
            if (m != 0)
            { /* Test isn't needed but avoids compiler warning */
                // MALLOC_FAILURE_ACTION;
                return nullptr;
            }
        }
        else
        {
            msize_t nb  = request2size(bytes);
            msize_t req = nb + alignment + MIN_CHUNK_SIZE - CHUNK_OVERHEAD;
            u8*  mem = (u8*)(__alloc(req));
            if (mem != 0)
            {
                void*     leader  = 0;
                void*     trailer = 0;
                mchunkptr p       = mem2chunk(mem);

                if (PREACTION(m))
                    return 0;

                if ((((msize_t)(mem)) % alignment) != 0)
                { /* misaligned */
                    /*
                    Find an aligned spot inside chunk.  Since we need to give
                    back leading space in a chunk of at least MIN_CHUNK_SIZE, if
                    the first calculation places us at a spot with less than
                    MIN_CHUNK_SIZE leader, we can move to the next aligned spot.
                    We've allocated enough total room so that this is always
                    possible.
                    */
                    u8*    br       = (u8*)mem2chunk((msize_t)(((msize_t)(mem + alignment - SIZE_T_ONE)) & -alignment));
                    u8*    pos      = ((msize_t)(br - (u8*)(p)) >= MIN_CHUNK_SIZE) ? br : br + alignment;
                    mchunkptr newp     = (mchunkptr)pos;
                    msize_t   leadsize = (msize_t)(pos - (u8*)(p));
                    msize_t   newsize  = chunksize(p) - leadsize;

                    { /* Give back leader, use the rest */
                        set_inuse(m, newp, newsize);
                        set_inuse(m, p, leadsize);
                        leader = chunk2mem(p);
                    }
                    p = newp;
                }

                /* Give back spare room at the end */
                {
                    msize_t size = chunksize(p);
                    if (size > nb + MIN_CHUNK_SIZE)
                    {
                        msize_t   remainder_size = size - nb;
                        mchunkptr remainder      = chunk_plus_offset(p, nb);
                        set_inuse(m, p, nb);
                        set_inuse(m, remainder, remainder_size);
                        trailer = chunk2mem(remainder);
                    }
                }

                ASSERT(chunksize(p) >= nb);
                ASSERT((((msize_t)(chunk2mem(p))) % alignment) == 0);
                check_inuse_chunk(m, mParams, p);
                POSTACTION(m);
                if (leader != 0)
                {
                    __free(leader);
                }
                if (trailer != 0)
                {
                    __free(trailer);
                }
                return chunk2mem(p);
            }
        }
        return 0;
    }

    /* ------------------------ comalloc/coalloc support --------------------- */
    void** mem_heap_base_t::__internal_ic_alloc(msize_t n_elements, msize_t* sizes, s32 opts, void* chunks[])
    {
        /*
        This provides common support for independent_X routines, handling
        all of the combinations that can result.

        The opts arg has:
        bit 0 set if all elements are same size (using sizes[0])
        bit 1 set if elements should be zeroed
        */

        msize_t   element_size;   /* chunksize of each element, if all same */
        msize_t   contents_size;  /* total size of elements */
        msize_t   array_size;     /* request size of pointer array */
        void*     mem;            /* malloced aggregate space */
        mchunkptr p;              /* corresponding chunk */
        msize_t   remainder_size; /* remaining bytes while splitting */
        void**    marray;         /* either "chunks" or malloced ptr array */
        mchunkptr array_chunk;    /* chunk for malloced ptr array */
        msize_t   size;
        msize_t   i;

        mstate m = mState;

        ensure_initialization();
        /* compute array length, if needed */
        if (chunks != 0)
        {
            if (n_elements == 0)
                return chunks; /* nothing to do */
            marray     = chunks;
            array_size = 0;
        }
        else
        {
            /* if empty req, must still return chunk representing empty array */
            if (n_elements == 0)
                return (void**)(__alloc(0));
            marray     = 0;
            array_size = request2size(n_elements * (sizeof(void*)));
        }

        /* compute total element size */
        if (opts & 0x1)
        { /* all-same-size */
            element_size  = request2size(*sizes);
            contents_size = n_elements * element_size;
        }
        else
        { /* add up all the sizes */
            element_size  = 0;
            contents_size = 0;
            for (i = 0; i != n_elements; ++i)
                contents_size += request2size(sizes[i]);
        }

        size = contents_size + array_size;

        /*
            Allocate the aggregate chunk.
        */
        mem = __alloc(size - CHUNK_OVERHEAD);

        if (mem == 0)
            return 0;

        if (PREACTION(m))
            return 0;

        p              = mem2chunk(mem);
        remainder_size = chunksize(p);

        if (opts & 0x2)
        { /* optionally clear the elements */
            nmem::memset((msize_t*)mem, 0, remainder_size - SIZE_T_SIZE - array_size);
        }

        /* If not provided, allocate the pointer array as final part of chunk */
        if (marray == 0)
        {
            msize_t array_chunk_size;
            array_chunk      = chunk_plus_offset(p, contents_size);
            array_chunk_size = remainder_size - contents_size;
            marray           = (void**)(chunk2mem(array_chunk));
            set_size_and_pinuse_of_inuse_chunk(m, array_chunk, array_chunk_size);
            remainder_size = contents_size;
        }

        /* split out elements */
        for (i = 0;; ++i)
        {
            marray[i] = chunk2mem(p);
            if (i != n_elements - 1)
            {
                if (element_size != 0)
                    size = element_size;
                else
                    size = request2size(sizes[i]);
                remainder_size -= size;
                set_size_and_pinuse_of_inuse_chunk(m, p, size);
                p = chunk_plus_offset(p, size);
            }
            else
            { /* the final element absorbs any overallocation slop */
                set_size_and_pinuse_of_inuse_chunk(m, p, remainder_size);
                break;
            }
        }

#if XMEM_HEAP_DEBUG

        if (marray != chunks)
        {
            /* final element must have exactly exhausted chunk */
            if (element_size != 0)
            {
                ASSERT(remainder_size == element_size);
            }
            else
            {
                ASSERT(remainder_size == request2size(sizes[i]));
            }
            check_inuse_chunk(m, mParams, mem2chunk(marray));
        }
        for (i = 0; i != n_elements; ++i)
            check_inuse_chunk(m, mParams, mem2chunk(marray[i]));

#endif /* XMEM_HEAP_DEBUG */

        POSTACTION(m);
        return marray;
    }

    /* -------------------------- public routines ---------------------------- */

    void* mem_heap_base_t::__alloc(msize_t bytes)
    {
        /*
        Basic algorithm:
        If a small request (< 256 bytes minus per-chunk overhead):
        1. If one exists, use a remainderless chunk in associated smallbin.
        (Remainderless means that there are too few excess bytes to
        represent as a chunk.)
        2. If it is big enough, use the dv chunk, which is normally the
        chunk adjacent to the one used for the most recent small request.
        3. If one exists, split the smallest available chunk in a bin,
        saving remainder in dv.
        4. If it is big enough, use the top chunk.
        5. If available, get memory from system and use it
        Otherwise, for a large request:
        1. Find the smallest available binned chunk that fits, and use it
        if it is better fitting than dv chunk, splitting if necessary.
        2. If better fitting than any binned chunk, use the dv chunk.
        3. If it is big enough, use the top chunk.
        4.
        5. If available, get memory from system and use it

        The ugly goto's here ensure that postaction occurs along all paths.
        */

        if (!PREACTION(mState))
        {
            void*   mem;
            msize_t nb;
            if (bytes <= MAX_SMALL_REQUEST)
            {
                bindex_t idx;
                binmap_t smallbits;
                nb        = (bytes < MIN_REQUEST) ? MIN_CHUNK_SIZE : pad_request(bytes);
                idx       = small_index(nb);
                smallbits = mState->smallmap >> idx;

                if ((smallbits & 0x3U) != 0)
                { /* Remainderless fit to a smallbin. */
                    mchunkptr b, p;
                    idx += ~smallbits & 1; /* Uses next bin if idx empty */
                    b = smallbin_at(mState, idx);
                    p = b->fd;
                    ASSERT(chunksize(p) == small_index2size(idx));
                    unlink_first_small_chunk(mState, b, p, idx);
                    set_inuse_and_pinuse(mState, p, small_index2size(idx));
                    mem = chunk2mem(p);
                    check_malloced_chunk(mState, mParams, mem, nb);
                    goto postaction;
                }
                else if (nb > mState->dvsize)
                {
                    if (smallbits != 0)
                    { /* Use chunk in next nonempty smallbin */
                        mchunkptr b, p, r;
                        msize_t   rsize;
                        bindex_t  i;
                        binmap_t  leftbits = (smallbits << idx) & left_bits(idx2bit(idx));
                        binmap_t  leastbit = least_bit(leftbits);
                        compute_bit2idx(leastbit, i);
                        b = smallbin_at(mState, i);
                        p = b->fd;
                        if (chunksize(p) != small_index2size(i))
                            ASSERT(chunksize(p) == small_index2size(i));
                        unlink_first_small_chunk(mState, b, p, i);
                        rsize = small_index2size(i) - nb;
                        /* Fit here cannot be remainderless if 4byte sizes */
                        if (SIZE_T_SIZE != 4 && rsize < MIN_CHUNK_SIZE)
                        {
                            set_inuse_and_pinuse(mState, p, small_index2size(i));
                        }
                        else
                        {
                            set_size_and_pinuse_of_inuse_chunk(mState, p, nb);
                            r = chunk_plus_offset(p, nb);
                            set_size_and_pinuse_of_free_chunk(r, rsize);
                            replace_dv(mState, r, rsize);
                        }
                        mem = chunk2mem(p);
                        check_malloced_chunk(mState, mParams, mem, nb);
                        goto postaction;
                    }

                    else if (mState->treemap != 0 && (mem = __tmalloc_small(mState, nb)) != 0)
                    {
                        check_malloced_chunk(mState, mParams, mem, nb);
                        goto postaction;
                    }
                }
            }
            else if (bytes >= MAX_REQUEST)
            {
                /* Too big to allocate. Force failure (in sys alloc) */
                // FatalError();
                return nullptr;
            }
            else
            {
                nb = pad_request(bytes);
                if (mState->treemap != 0 && (mem = __tmalloc_large(mState, nb)) != 0)
                {
                    check_malloced_chunk(mState, mParams, mem, nb);
                    goto postaction;
                }
            }

            if (nb <= mState->dvsize)
            {
                msize_t   rsize = mState->dvsize - nb;
                mchunkptr p     = mState->dv;
                if (rsize >= MIN_CHUNK_SIZE)
                { /* split dv */
                    mchunkptr r = mState->dv = chunk_plus_offset(p, nb);
                    mState->dvsize           = rsize;
                    set_size_and_pinuse_of_free_chunk(r, rsize);
                    set_size_and_pinuse_of_inuse_chunk(mState, p, nb);
                }
                else
                { /* exhaust dv */
                    msize_t dvs    = mState->dvsize;
                    mState->dvsize = 0;
                    mState->dv     = 0;
                    set_inuse_and_pinuse(mState, p, dvs);
                }
                mem = chunk2mem(p);
                check_malloced_chunk(mState, mParams, mem, nb);
                goto postaction;
            }
            else if (nb < mState->topsize)
            { /* Split top */
                msize_t   rsize = mState->topsize -= nb;
                mchunkptr p     = mState->top;
                mchunkptr r = mState->top = chunk_plus_offset(p, nb);
                r->head                   = rsize | PINUSE_BIT;
                set_size_and_pinuse_of_inuse_chunk(mState, p, nb);
                mem = chunk2mem(p);
                check_top_chunk(mState, mState->top);
                check_malloced_chunk(mState, mParams, mem, nb);
                goto postaction;
            }

            if (mSysAlloc != nullptr)
            {
                // Allocate an extra segment
                u32 tsize = 8 * 1024 * 1024;
                if (nb > tsize)
                    tsize = ((nb / tsize) + 1) * tsize;
                u8* tbase = (u8*)mSysAlloc(tsize);

                if ((mState->footprint += tsize) > mState->max_footprint)
                    mState->max_footprint = mState->footprint;

                __add_segment(tbase, tsize, EXTERN_BIT);

                nb = pad_request(bytes);
                if (nb < mState->topsize)
                { /* Allocate from new or extended top space */
                    msize_t   rsize = mState->topsize -= nb;
                    mchunkptr p     = mState->top;
                    mchunkptr r = mState->top = chunk_plus_offset(p, nb);
                    r->head                   = rsize | PINUSE_BIT;
                    set_size_and_pinuse_of_inuse_chunk(mState, p, nb);
                    check_top_chunk(mState, mState->top);
                    check_malloced_chunk(mState, mParams, chunk2mem(p), nb);
                    return chunk2mem(p);
                }
            }

            return nullptr;

        postaction:
            POSTACTION(mState);
            return mem;
        }
        else
        {
            return 0;
        }
    }

    u32 mem_heap_base_t::__free(void* mem)
    {
        /*
            Consolidate freed chunks with preceding or succeeding bordering
            free chunks, if they exist, and then place in a bin. Intermixed
            with special cases for top, dv and usage errors.
        */

        if (mem != 0)
        {
            mchunkptr p    = mem2chunk(mem);
            u32 const size = (is_inuse(p)) ? chunksize(p) - overhead_for(p) : 0;

#if FOOTERS
            mstate fm = get_mstate_for(p);
            if (!ok_magic(fm))
            {
                USAGE_ERROR_ACTION(fm, p);
                return 0;
            }
#else  /* FOOTERS */
            mstate fm = mState;
#endif /* FOOTERS */
            if (!PREACTION(fm))
            {
                check_inuse_chunk(fm, mParams, p);
                if (RTCHECK(ok_address(fm, p) && ok_inuse(p)))
                {
                    msize_t   psize = chunksize(p);
                    mchunkptr next  = chunk_plus_offset(p, psize);
                    if (!pinuse(p))
                    {
                        msize_t prevsize = p->prev_foot;
                        {
                            mchunkptr prev = chunk_minus_offset(p, prevsize);
                            psize += prevsize;
                            p = prev;
                            if (RTCHECK(ok_address(fm, prev)))
                            { /* consolidate backward */
                                if (p != fm->dv)
                                {
                                    unlink_chunk(fm, p, prevsize);
                                }
                                else if ((next->head & INUSE_BITS) == INUSE_BITS)
                                {
                                    fm->dvsize = psize;
                                    set_free_with_pinuse(p, psize, next);
                                    goto postaction;
                                }
                            }
                            else
                            {
                                goto erroraction;
                            }
                        }
                    }

                    if (RTCHECK(ok_next(p, next) && ok_pinuse(next)))
                    {
                        if (!cinuse(next))
                        { /* consolidate forward */
                            if (next == fm->top)
                            {
                                msize_t tsize = fm->topsize += psize;
                                fm->top       = p;
                                p->head       = tsize | PINUSE_BIT;
                                if (p == fm->dv)
                                {
                                    fm->dv     = 0;
                                    fm->dvsize = 0;
                                }

                                goto postaction;
                            }
                            else if (next == fm->dv)
                            {
                                msize_t dsize = fm->dvsize += psize;
                                fm->dv        = p;
                                set_size_and_pinuse_of_free_chunk(p, dsize);
                                goto postaction;
                            }
                            else
                            {
                                msize_t nsize = chunksize(next);
                                psize += nsize;
                                unlink_chunk(fm, next, nsize);
                                set_size_and_pinuse_of_free_chunk(p, psize);
                                if (p == fm->dv)
                                {
                                    fm->dvsize = psize;
                                    goto postaction;
                                }
                            }
                        }
                        else
                        {
                            set_free_with_pinuse(p, psize, next);
                        }

                        if (is_small(psize))
                        {
                            insert_small_chunk(fm, p, psize);
                            check_free_chunk(fm, p);
                        }
                        else
                        {
                            tchunkptr tp = (tchunkptr)p;
                            insert_large_chunk(fm, tp, psize);
                            check_free_chunk(fm, p);
                            if (--fm->release_checks == 0)
                                __release_unused_segments(fm);
                        }
                        goto postaction;
                    }
                }
            erroraction:
                USAGE_ERROR_ACTION(fm, p);
            postaction:
                POSTACTION(fm);
            }
            return size;
        }
        return 0;
    }

    void* mem_heap_base_t::__allocN(msize_t n_elements, msize_t elem_size)
    {
        void*   mem;
        msize_t req = 0;
        mstate  ms  = mState;
        if (!ok_magic(ms))
        {
            USAGE_ERROR_ACTION(ms, ms);
            return 0;
        }
        if (n_elements != 0)
        {
            req = n_elements * elem_size;
            if (((n_elements | elem_size) & ~(msize_t)0xffff) && (req / n_elements != elem_size))
                req = MAX_SIZE_T; /* force downstream failure on overflow */
        }
        mem = __alloc(req);
        if (mem != 0 && calloc_must_clear(mem2chunk(mem)))
            nmem::memset(mem, 0, req);
        return mem;
    }

    void* mem_heap_base_t::__allocR(void* oldmem, msize_t alignment, msize_t bytes)
    {
        if (oldmem == 0)
            return __allocA(alignment, bytes);

#ifdef REALLOC_ZERO_BYTES_FREES
        if (bytes == 0)
        {
            __free(oldmem);
            return 0;
        }
#endif /* REALLOC_ZERO_BYTES_FREES */
        else
        {
#if !FOOTERS
            mstate m = mState;
#else  /* FOOTERS */
            mstate m  = get_mstate_for(mem2chunk(oldmem));
            if (!ok_magic(m))
            {
                USAGE_ERROR_ACTION(m, oldmem);
                return 0;
            }
#endif /* FOOTERS */
            return __internal_realloc(m, oldmem, alignment, bytes);
        }
    }

    void* mem_heap_base_t::__allocA(msize_t alignment, msize_t bytes)
    {
        if (!ok_magic(mState))
        {
            USAGE_ERROR_ACTION(mState, mState);
            return 0;
        }
        return __internal_memalign(alignment, bytes);
    }

    void** mem_heap_base_t::__allocIC(msize_t n_elements, msize_t elem_size, void* chunks[])
    {
        msize_t sz = elem_size; /* serves as 1-element array */
        mstate  ms = mState;
        if (!ok_magic(ms))
        {
            USAGE_ERROR_ACTION(ms, ms);
            return 0;
        }
        return __internal_ic_alloc(n_elements, &sz, 3, chunks);
    }

    void** mem_heap_base_t::__allocICO(msize_t n_elements, msize_t sizes[], void* chunks[])
    {
        mstate ms = mState;
        if (!ok_magic(ms))
        {
            USAGE_ERROR_ACTION(ms, ms);
            return 0;
        }
        return __internal_ic_alloc(n_elements, sizes, 0, chunks);
    }

    u32 mem_heap_base_t::__usable_size(void* mem)
    {
        if (mem != 0)
        {
            mchunkptr p = mem2chunk(mem);
            if (is_inuse(p))
                return chunksize(p) - overhead_for(p);
        }
        return 0;
    }

    msize_t mem_heap_base_t::__footprint()
    {
        msize_t result = 0;
        mstate  ms     = mState;
        if (ok_magic(ms))
        {
            result = ms->footprint;
        }
        else
        {
            USAGE_ERROR_ACTION(ms, ms);
        }
        return result;
    }

    msize_t mem_heap_base_t::__max_footprint()
    {
        msize_t result = 0;
        mstate  ms     = mState;
        if (ok_magic(ms))
        {
            result = ms->max_footprint;
        }
        else
        {
            USAGE_ERROR_ACTION(ms, ms);
        }
        return result;
    }

    //	void mem_heap_base_t::__stats(xmem_managed_size& stats)
    //	{
    //		mstate ms = mState;
    //		if (ok_magic(ms))
    //		{
    //			__internal_malloc_stats(stats);
    //		}
    //		else
    //		{
    //			USAGE_ERROR_ACTION(ms,ms);
    //		}
    //	}

    ncore::u32 mem_heap_t::__sGetMemSize(void* mem)
    {
        mchunkptr chunkPtr = mem2chunk(mem);
        return chunksize(chunkPtr);
    }

    class allocator_dlmalloc : public alloc_t
    {
        mem_heap_t mDlMallocHeap;

    public:
        void init(void* mem, s32 mem_size)
        {
            mDlMallocHeap.__initialize();
            mDlMallocHeap.__manage(mem, mem_size);
        }

        virtual void* v_allocate(u32 size, u32 alignment)
        {
            if (alignment <= MEMALIGN)
                return mDlMallocHeap.__alloc((msize_t)size);

            return mDlMallocHeap.__allocA(alignment, (msize_t)size);
        }

        virtual u32 v_deallocate(void* ptr)
        {
            if (ptr != nullptr)
                return mDlMallocHeap.__free(ptr);
            return 0;
        }

        virtual void v_release() { mDlMallocHeap.__destroy(); }

        void* operator new(uint_t num_bytes) { return nullptr; }
        void* operator new(uint_t num_bytes, void* mem) { return mem; }
        void  operator delete(void* pMem) {}
        void  operator delete(void* pMem, void*) {}
    };

    alloc_t* gCreateDlAllocator(void* mem, u32 memsize)
    {
        allocator_dlmalloc* allocator = new (mem) allocator_dlmalloc();

        s32 allocator_class_size = math::ceilpo2(sizeof(allocator_dlmalloc));
        mem                      = (void*)((u8*)mem + allocator_class_size);

        allocator->init(mem, memsize - allocator_class_size);
        return allocator;
    }

}; // namespace ncore

#ifdef TARGET_PS3
#pragma diag_warning = no_corresponding_delete
#endif
