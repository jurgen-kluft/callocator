#include "ccore/c_target.h"
#include "ccore/c_math.h"
#include "ccore/c_memory.h"
#include "ccore/c_vmem.h"

#include "callocator/c_allocator_segward.h"

namespace ncore
{
    namespace nsegward
    {
        const u8 SEGMENT_STATE_EMPTY   = 0x0;
        const u8 SEGMENT_STATE_ACTIVE  = 0x1;
        const u8 SEGMENT_STATE_FULL    = 0x2;
        const u8 SEGMENT_STATE_RETIRED = 0x3;

        // Requirements for creating the segmented forward allocator:
        // - segment size
        //   - power of two
        //   - 4KB <= size <= 1GB
        // - total size must be at least 3 times the segment size
        // - allocation alignment must be a power of two, at least 8 and less than (segment-size / 256)
        // - configure this allocator so that a segment can hold N allocations (N >= 256 at a minimum)

        struct allocator_t
        {
            arena_t* m_arena;              // underlying virtual memory arena
            i32*     m_segment_counters;   // allocation counter per segment
            u32*     m_segment_cursors;    // current write cursor per segment
            u8*      m_segment_states;     // bitfield to track the state of each segment (EMPTY, RETIRED, ACTIVE)
            byte*    m_base_address;       // base address of where segments start
            i32      m_segment_count;      // total number of segments
            i32      m_segment;            // current segment for next allocation
            s8       m_segment_size_shift; // size (1 << m_segment_size_shift) of each segment (power of two)
        };

        allocator_t* create(int_t segment_size, int_t total_size)
        {
            // power-of-2 upper bound of segment size
            segment_size = math::g_ceilpo2(segment_size);
            if (segment_size < 4096 || segment_size > (1 << 30))
                return nullptr;

            // align up the total size to be able to fit full segments
            total_size = (total_size + (segment_size - 1)) & ~(segment_size - 1);

            const u32 max_segments = (u32)(total_size / segment_size);
            const u32 min_segments = 3; // Need at least 3 segments to work properly
            if (max_segments < min_segments)
                return nullptr;

            u32 size = sizeof(allocator_t);
            size += sizeof(i32) * max_segments; // segment counters
            size += sizeof(u32) * max_segments; // segment cursors
            size += sizeof(u8) * max_segments;  // segment states

            arena_t* arena = narena::create(total_size, size + (min_segments * segment_size));

            allocator_t* allocator          = narena::allocate_and_clear<allocator_t>(arena);
            allocator->m_arena              = arena;
            allocator->m_segment_counters   = narena::allocate_array_and_clear<i32>(arena, max_segments);
            allocator->m_segment_cursors    = narena::allocate_array_and_clear<u32>(arena, max_segments);
            allocator->m_segment_states     = narena::allocate_array_and_clear<u8>(arena, max_segments);
            allocator->m_segment_count      = (i32)max_segments;
            allocator->m_segment            = 0;
            allocator->m_segment_size_shift = (s8)math::g_ilog2(segment_size);

            const int_t current_pos   = narena::save_point(arena);
            allocator->m_base_address = narena::address_at_pos(arena, current_pos);

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
            //    - find an EMPTY segment
            //    - if none found, find a RETIRED segment
            //    - if none found, fail the allocation (out of memory)
            // 3. allocate:
            //    - allocate from current segment
            //    - increment segment counter
            //    - move cursor forward
            //    - done and return pointer

            if (a == nullptr || size == 0)
                return nullptr;

            // align size to at least 8 bytes
            size = (size + (8u - 1u)) & ~(8u - 1u);

            // Ensure alignment is a power of two and at least 8 (typical minimum)
            // Cap alignment to (segment size / 256) (cannot align beyond this in a segment, see requirement)
            ASSERT(alignment <= ((1u << a->m_segment_size_shift) >> 8));
            alignment = (alignment < 8u) ? 8u : alignment;
            alignment = (alignment + (8u - 1u)) & ~(8u - 1u);

            i32 seg = a->m_segment;

            // Check current segment
            const u32 end = ((a->m_segment_cursors[seg] + (alignment - 1u)) & ~(alignment - 1u)) + size;
            if (end > (1u << a->m_segment_size_shift))
            {
                // mark current segment as FULL since this allocation doesn't fit
                a->m_segment_states[seg] = SEGMENT_STATE_FULL;

                // linear search
                i32 found = -1;
                for (i32 i = 0; i < a->m_segment_count; i++)
                {
                    if (a->m_segment_states[i] == SEGMENT_STATE_EMPTY || a->m_segment_states[i] == SEGMENT_STATE_RETIRED)
                    {
                        found = i;
                        break;
                    }
                }

                // If no new segment found, out of memory
                if (found == -1)
                    return nullptr;

                a->m_segment_states[found] = SEGMENT_STATE_ACTIVE;
                seg                        = found;
                a->m_segment               = seg;
            }

            // 3) Allocate from selected segment
            u32 const cursor  = a->m_segment_cursors[seg];
            u32 const aligned = (cursor + (alignment - 1u)) & ~(alignment - 1u);

            // Verify: aligned allocation must fit (should be true by previous checks)
            ASSERT((u64)aligned + (u64)size <= (u64)(1u << a->m_segment_size_shift));

            // Compute the absolute address: base + segment_offset + aligned
            u8* const ptr = a->m_base_address + ((u32)seg << a->m_segment_size_shift) + aligned;

            // Bump cursor and live allocation counter
            a->m_segment_cursors[seg] = aligned + size;
            a->m_segment_counters[seg] += 1;

            return (void*)ptr;
        }

        void deallocate(allocator_t* a, void* ptr)
        {
            // Do we have a valid allocator and pointer?
            ASSERT(a != nullptr && ptr != nullptr);

            // Check if pointer is outside of the arena
            ASSERT((const u8*)ptr >= a->m_base_address);

            // Which segment is this coming from and is it valid?
            const i32 seg = (i32)(((const u8*)ptr - a->m_base_address) >> a->m_segment_size_shift);
            ASSERT(seg >= 0 && seg < a->m_segment_count); // invalid segment index

            // A free should be on an ACTIVE segment
            ASSERT(a->m_segment_states[seg] == SEGMENT_STATE_ACTIVE);

            // Decrement the segment counter
            i32& count = a->m_segment_counters[seg];
            ASSERT(count > 0);
            count -= 1;
            if (count > 0)
            {
                // Segment has no more live allocations, mark as RETIRED
                a->m_segment_states[seg]  = SEGMENT_STATE_RETIRED;
                a->m_segment_cursors[seg] = 0;
            }
        }

    } // namespace nsegward

}; // namespace ncore
