#include "ccore/c_target.h"
#include "ccore/c_math.h"
#include "ccore/c_memory.h"
#include "ccore/c_arena.h"

#include "callocator/c_allocator_segward.h"

namespace ncore
{
    namespace nsegward
    {
        struct allocator_t
        {
            arena_t* m_arena;                // underlying virtual memory arena
            u64      m_segment_alloc_cursor; // current segment allocation cursor
            i16*     m_segment_counters;     // allocation counter per segment (< 32768 allocations per segment, < 0 means uncommitted)
            u16      m_segment;              // current segment index being allocated from
            u16      m_segment_count;        // total number of segments
            u16      m_segment_size_shift;   // size (1 << m_segment_size_shift) of each segment (power of two)
        };

        allocator_t* create(int_t segment_size, int_t total_size)
        {
            // power-of-2 upper bound of segment size
            segment_size = math::ceilpo2(segment_size);
            if (segment_size < 4096 || segment_size > (1 << 30))
                return nullptr;

            // align up the total size to be able to fit full segments
            total_size = (total_size + (segment_size - 1)) & ~(segment_size - 1);

            const i32 max_segments = (i32)(total_size / segment_size);
            const i32 min_segments = 3; // Need at least 3 segments to work properly
            if (max_segments < min_segments || max_segments >= 32768)
                return nullptr;

            arena_t* arena = narena::new_arena(total_size + (int_t)(4 * cKB), (int_t)(4 * cKB) + (min_segments * segment_size));

            allocator_t* allocator          = narena::allocate_and_clear<allocator_t>(arena);
            allocator->m_arena              = arena;
            allocator->m_segment_counters   = narena::allocate_array_and_clear<i16>(arena, max_segments);
            allocator->m_segment_count      = max_segments;
            allocator->m_segment_size_shift = (s8)math::ilog2(segment_size);

            // make sure the arena allocations start at 4 KB boundary
            narena::alloc(arena, (int_t)(4 * cKB) - (int_t)((const byte*)narena::current_address(arena) - (const byte*)narena::base(arena)));

            // start with first segment, and mark first segment as active
            allocator->m_segment              = 0;
            allocator->m_segment_alloc_cursor = 0;

            // commit first segment
            const int_t committed_size_in_bytes = (1 << allocator->m_segment_size_shift) + ((int_t)1 << allocator->m_arena->m_page_size_shift);
            DVERIFY(narena::commit(allocator->m_arena, committed_size_in_bytes), true);

            for (i16 i = 0; i < min_segments; i++)
                allocator->m_segment_counters[i] = 0;
            for (i16 i = (i16)min_segments; i < allocator->m_segment_count; i++)
                allocator->m_segment_counters[i] = -1; // mark as uncommitted

            return allocator;
        }

        void destroy(allocator_t* allocator)
        {
            // release underlying arena
            // - we don't have to do anything special here, as all memory is part of the arena
            arena_t* arena = allocator->m_arena;
            narena::destroy(arena);
        }

        void* allocate(allocator_t* a, u32 size, u32 alignment)
        {
            if (a == nullptr || size == 0)
                return nullptr;

            ASSERT(alignment != 0 && math::ispo2(alignment));       // requirement: alignment must be a power of two
            ASSERT(size <= ((1u << a->m_segment_size_shift) >> 6)); // requirement: allocation size <= (segment size / 64)

            // Verify alignment to (segment size / 64) (cannot align beyond this in a segment, see requirement)
            ASSERT(alignment <= ((1u << a->m_segment_size_shift) >> 6)); // requirement: alignment <= (segment size / 64)

            // Check current segment, can it satisfy the request?
            u64 aligned = (a->m_segment_alloc_cursor + ((u64)alignment - 1u)) & ~((u64)alignment - 1u);
            if ((aligned + size) <= ((u64)(a->m_segment + 1) << a->m_segment_size_shift))
            {
                // Yes it can, bump the write cursor and allocation counter
                a->m_segment_alloc_cursor = aligned + (u64)size;
                a->m_segment_counters[a->m_segment] += 1;

                // return the absolute address: base + segment_offset + aligned
                return (u8*)narena::base(a->m_arena) + ((int_t)1 << a->m_arena->m_page_size_shift) + aligned;
            }

            // segment cannot satisfy request, search for a new segment and make that the active one
            // linear search through array of segments
            // todo: this can be optimized with bit array ?
            for (u32 i = 0; i < a->m_segment_count; i++)
            {
                const i16 count = a->m_segment_counters[i];
                if (count > 0) // segment is not empty
                    continue;

                if (count < 0)
                { // Extend the committed region to include this segment
                    const int_t committed_size_in_bytes = ((i + 1) << a->m_segment_size_shift) + ((int_t)1 << a->m_arena->m_page_size_shift);
                    DVERIFY(narena::commit(a->m_arena, committed_size_in_bytes), true);
                }

                aligned = (i == 0) ? ((u64)1 << a->m_arena->m_page_size_shift) : (u64)i << a->m_segment_size_shift;
                aligned = (aligned + ((u64)alignment - 1u)) & ~((u64)alignment - 1u);

                // Verify: aligned allocation must fit (should always be true)
                ASSERT(aligned + (u64)size <= ((i + 1) << a->m_segment_size_shift));

                a->m_segment = i;

                // Bump cursor and live allocation counter
                a->m_segment_alloc_cursor           = aligned + (u64)size;
                a->m_segment_counters[a->m_segment] = 1;

                // return the absolute address: base + aligned cursor
                return (u8*)narena::base(a->m_arena) + ((int_t)1 << a->m_arena->m_page_size_shift) + aligned;
            }

            // No new segment found, out of memory
            return nullptr;
        }

        void deallocate(allocator_t* a, void* ptr)
        {
            // Do we have a valid allocator and pointer?
            ASSERT(a != nullptr && ptr != nullptr);

            // Check if pointer is outside of the arena
            ASSERT(((const u8*)ptr >= ((const u8*)narena::base(a->m_arena) + ((int_t)1 << a->m_arena->m_page_size_shift))) && narena::within_committed(a->m_arena, ptr));

            // Which segment is this coming from and is it valid?
            const u32 segment = (u32)(((const u8*)ptr - ((const u8*)narena::base(a->m_arena) + ((int_t)1 << a->m_arena->m_page_size_shift))) >> a->m_segment_size_shift);
            ASSERT(segment < a->m_segment_count); // invalid segment index

            // Decrement the segment counter
            i16& count = a->m_segment_counters[segment];
            if (count > 0)
            {
                count -= 1;
                if (count == 0)
                {
                    // TODO optional
                    // If this is the current segment, we can move back the allocation cursor?

                    // TODO optional
                    // A segment can be decommitted only when it at the end of the committed region
                    // This is an optimization to reduce committed memory usage, but may lead to
                    // more commit/decommit operations if the segment is reused often.
                    // Also we need to be aware of the ping-pong effect of commit/decommit if the
                    // segment is on the boundary of number of segments that are being actively
                    // used.
                }
            }
            else
            {
                ASSERT(count != 0); // Double free detected, could be any one before us, not specifically this one
            }
        }

    } // namespace nsegward

}; // namespace ncore
