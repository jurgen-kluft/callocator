#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "ccore/c_math.h"
#include "ccore/c_vmem.h"

#include "callocator/c_allocator_frame.h"

namespace ncore
{
    // Frame allocator (frame scope allocator)
    //
    // We have two memory arenas, the first one is used until we reach the maximum number of active frames.
    // Now we are switching to the second arena, which is used for creating new frames, when we here reach
    // the maximum number of active frames, the first arena 'should' be unoccupied, so we can reuse it.
    // An ASSERT will be triggered if we switch lanes and the target lane is still occupied.

    frame_allocator_t::frame_allocator_t() : m_active_lane(0), m_max_active_frames(0), m_current_frame(nullptr)
    {
        for (s32 i = 0; i < 2; i++)
        {
            m_active_frames[i] = 0;
            m_ended_frames[i]  = 0;
            m_frames[i]        = nullptr;
            m_arena[i]         = nullptr;
        }
    }

    frame_allocator_t::~frame_allocator_t()
    {
        m_arena[0]->release();
        m_arena[1]->release();
    }

    void frame_allocator_t::reset()
    {
        for (s32 i = 0; i < 2; i++)
        {
            m_active_frames[i] = 0;
            m_ended_frames[i]  = 0;
            m_arena[i]->restore_point(m_save_points[i]);
        }
        m_active_lane = 0;
        // m_max_active_frames = ?;
        m_current_frame = nullptr;
    }

    void frame_allocator_t::setup(s32 max_active_frames, int_t average_frame_size, int_t max_reserved_size)
    {
        m_max_active_frames = max_active_frames;
        for (s32 i = 0; i < 2; i++)
        {
            arena_t* arena   = gCreateArena((sizeof(frame_t) * max_active_frames) + sizeof(arena_t) + max_reserved_size, (sizeof(frame_t) * max_active_frames) + average_frame_size * max_active_frames);
            m_frames[i]      = (frame_t*)arena->alloc_and_zero(sizeof(frame_t) * max_active_frames);
            m_save_points[i] = arena->save_point();
            m_arena[i]       = arena;
        }
        reset();
    }

    s32 frame_allocator_t::v_new_frame()
    {
        if (m_current_frame != nullptr)
            end_frame();

        if (m_active_frames[m_active_lane] >= m_max_active_frames)
        {
            // We have reached the maximum number of active frames, switch lanes.
            // Before we switch we need to ensure that the first arena is not occupied anymore.
            const s32 new_lane = 1 - m_active_lane;

            // Did all the frames in this new lane end
            for (s32 i = 0; i < m_active_frames[new_lane]; i++)
            {
                frame_t* frame_array = (frame_t*)m_frames[new_lane];
                if (frame_array[i].m_frame_number != -1)
                {
                    // If we find a frame that is not ended, we cannot switch lanes
                    ASSERT(false);
                    return -1; // Indicate an error
                }
            }

            if (m_active_frames[new_lane] == m_ended_frames[new_lane])
            {
                m_active_frames[new_lane] = 0;
                m_ended_frames[new_lane]  = 0;

                m_arena[new_lane]->reset(); // Reset the arena for the new lane
                m_arena[new_lane]->alloc(sizeof(arena_t));
                m_frames[new_lane] = (frame_t*)m_arena[new_lane]->alloc_and_zero(sizeof(frame_t) * m_max_active_frames);
            }
            else
            {
                // We are trying to switch lanes, but the other lane is still occupied.
                // This should not happen, as we are supposed to end all frames before switching lanes.
                ASSERT(false);
            }

            m_active_lane = new_lane;
        }

        frame_t* frame_array           = (frame_t*)m_frames[m_active_lane];
        frame_t* new_frame             = &frame_array[m_active_frames[m_active_lane]];
        new_frame->m_frame_number      = m_active_frames[m_active_lane];
        new_frame->m_num_allocations   = 0;
        new_frame->m_num_deallocations = 0;
        new_frame->m_ended             = 0;

        m_current_frame = new_frame;
        m_active_frames[m_active_lane]++;

        return new_frame->m_frame_number | (m_active_lane << 24); // Return frame number with lane information
    }

    bool frame_allocator_t::v_end_frame()
    {
        ASSERT(m_current_frame != nullptr && m_current_frame->m_ended == 0);
        m_current_frame->m_ended = 1;
        ASSERT(m_ended_frames[m_active_lane] < m_max_active_frames);
        m_ended_frames[m_active_lane]++;
        m_current_frame = nullptr;
        return true;
    }

    bool frame_allocator_t::v_reset_frame(s32 frame_number)
    {
        const s32 lane = frame_number >> 24;      // Extract lane from frame number
        frame_number   = frame_number & 0xffffff; // Extract frame number
        ASSERT(lane >= 0 && lane < 2);
        ASSERT(frame_number >= 0 && frame_number < m_max_active_frames);

        frame_t* frame_array = (frame_t*)m_frames[lane];
        frame_t* frame       = &frame_array[frame_number];

        if (frame->m_frame_number != frame_number)
        {
            ASSERT(false);
            return false;
        }

        frame->m_frame_number      = -1;
        frame->m_num_allocations   = 0;
        frame->m_num_deallocations = 0;
        frame->m_ended             = 0;

        return true;
    }

    void* frame_allocator_t::v_allocate(u32 size, u32 alignment)
    {
        ASSERT(m_current_frame != nullptr);

        byte* p = (byte*)m_arena[m_active_lane]->alloc(size, alignment);

#ifdef TARGET_DEBUG
        nmem::memset(p, 0xcd, size);
#endif

        m_current_frame->m_num_allocations++;

        return p;
    }

    void frame_allocator_t::v_deallocate(void* ptr)
    {
        ASSERT(m_current_frame != nullptr);
        ASSERT(m_current_frame->m_num_allocations > m_current_frame->m_num_deallocations);
        m_current_frame->m_num_deallocations++;
    }

}; // namespace ncore
