#include "ccore/c_target.h"
#include "ccore/c_math.h"
#include "ccore/c_memory.h"
#include "ccore/c_vmem.h"

#include "callocator/c_allocator_segward.h"

namespace ncore
{
    namespace nsegward
    {
        // Requirements for creating the segmented forward allocator:
        // - segment constraints
        //   - size must be a power of two
        //   - 4KB <= size <= 1GB
        //   - minimum segments <= number of segments < 32768
        // - 0 <= number of allocations per segment < 65536
        // - total size must be at least 3 times the segment size
        // - allocation alignment must be a power of two, at least 8 and less than (segment-size / 256)
        // - configure this allocator so that a segment can hold N allocations (N >= 256 at a minimum)

        struct allocator_t
        {
            arena_t* m_arena;                // underlying virtual memory arena
            u16*     m_segment_counters;     // allocation counter per segment (< 65536 allocations per segment)
            u64      m_segment_alloc_cursor; // current segment allocation cursor
            u16      m_segment;              // current segment index being allocated from
            u16      m_committed;            // Continues number of committed segments from the start (for optimization)
            u16      m_segment_count;        // total number of segments
            u16      m_segment0_headersize;  // base address of where segments start
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

            const u32 max_segments = (u32)(total_size / segment_size);
            const u32 min_segments = 3; // Need at least 3 segments to work properly
            if (max_segments < min_segments || max_segments > 65535)
                return nullptr;

            arena_t* arena = narena::create(total_size, (min_segments * segment_size));

            allocator_t* allocator          = narena::allocate_and_clear<allocator_t>(arena);
            allocator->m_arena              = arena;
            allocator->m_segment_counters   = narena::allocate_array_and_clear<u16>(arena, max_segments);
            allocator->m_segment_count      = (i32)max_segments;
            allocator->m_segment_size_shift = (s8)math::ilog2(segment_size);

            // segment 0 is special, it needs to start after our header
            const byte* save_address         = (const byte*)narena::current_address(arena);
            allocator->m_segment0_headersize = (u32)(save_address - (const byte*)arena);

            // start with first segment, and mark first segment as active
            allocator->m_segment              = 0;
            allocator->m_committed            = (u16)min_segments;
            allocator->m_segment_alloc_cursor = allocator->m_segment0_headersize;

            return allocator;
        }

        void destroy(allocator_t* allocator)
        {
            // release underlying arena
            // - we don't have to do anything special here, as all memory is part of the arena
            narena::release(allocator->m_arena);
        }

        void* allocate(allocator_t* a, u32 size, u32 alignment)
        {
            // 1. does this still fit in the current segment
            // 2. if not, find a segment
            //    - find an EMPTY (COMMITTED or UNCOMMITTED) segment
            //    - if none found, fail the allocation (out of memory)
            // 3. allocate:
            //    - allocate from current segment
            //    - increment segment counter
            //    - move alloc cursor forward
            //    - done and return pointer

            if (a == nullptr || size == 0)
                return nullptr;

            // align size to at least 8 bytes
            size = (size + (8u - 1u)) & ~(8u - 1u);

            // Ensure alignment is a power of two and at least 8 (typical minimum)
            // Cap alignment to (segment size / 64) (cannot align beyond this in a segment, see requirement)
            ASSERT(alignment <= ((1u << a->m_segment_size_shift) >> 6));
            alignment = (alignment + (8u - 1u)) & ~(8u - 1u);

            // Check current segment
            u64 aligned     = (a->m_segment_alloc_cursor + ((u64)alignment - 1u)) & ~((u64)alignment - 1u);
            u64 segment_end = (a->m_segment + 1) << a->m_segment_size_shift;
            if ((aligned + size) > segment_end)
            {
                // mark current segment as DRAINING since it is full and the only events that should
                // affect it from this moment are deallocations.
                // u32 segment = (u32)(a->m_segment_alloc_cursor >> a->m_segment_size_shift);

                // linear search for a new segment
                // todo: this can be optimized with a empty-committed- and empty-uncommitted-segment list
                for (u32 i = 0; i < a->m_segment_count; i++)
                {
                    const u16 count = a->m_segment_counters[i];
                    if (count > 0) // segment is not empty
                        continue;

                    if (i >= a->m_committed)
                    {
                        // Extend the committed region to include this segment
                        const int_t committed_size_in_bytes = (i + 1) << a->m_segment_size_shift;
                        narena::commit(a->m_arena, committed_size_in_bytes);
                    }

                    a->m_segment_alloc_cursor = (i == 0) ? (u64)a->m_segment0_headersize : (u64)i << a->m_segment_size_shift;
                    a->m_segment              = i;
                    aligned                   = (a->m_segment_alloc_cursor + ((u64)alignment - 1u)) & ~((u64)alignment - 1u);
                    segment_end               = (a->m_segment + 1) << a->m_segment_size_shift;

                    // Verify: aligned allocation must fit (should be true by previous checks)
                    ASSERT(aligned + (u64)size <= segment_end);
                    goto label_allocate;
                }

                // No new segment found, out of memory
                return nullptr;
            }

        label_allocate:

            // Bump cursor and live allocation counter
            a->m_segment_alloc_cursor = aligned + (u64)size;
            a->m_segment_counters[a->m_segment] += 1;

            // return the absolute address: base + segment_offset + aligned
            return (u8*)narena::base(a->m_arena) + aligned;
        }

        void deallocate(allocator_t* a, void* ptr)
        {
            // Do we have a valid allocator and pointer?
            ASSERT(a != nullptr && ptr != nullptr);

            // Check if pointer is outside of the arena
            ASSERT(((const u8*)ptr >= ((const u8*)narena::base(a->m_arena) + a->m_segment0_headersize)) && narena::within_committed(a->m_arena, ptr));

            // Which segment is this coming from and is it valid?
            const u32 segment = (u32)(((const u8*)ptr - (const u8*)narena::base(a->m_arena)) >> a->m_segment_size_shift);
            ASSERT(segment < a->m_segment_count); // invalid segment index

            // Decrement the segment counter
            u16& count = a->m_segment_counters[segment];
            if (count > 0)
            {
                count -= 1;
                if (count == 0)
                {
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
