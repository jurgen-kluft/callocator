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
    // An ASSERT will be triggered if we try to allocate a new frame when the first arena is still occupied.

    frame_allocator_t::frame_allocator_t() : m_active_lane(0), m_max_active_frames(0), m_current_frame(nullptr)
    {
        for (s32 i = 0; i < 2; i++)
        {
            m_active_frames[i] = 0;
            m_ended_frames[i]  = 0;
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
            m_arena[i]->reset();
            m_arena[i]->commit(sizeof(frame_t) * m_max_active_frames, alignof(frame_t));
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
            vmem_arena_t a;
            a.reserved(max_reserved_size);
            a.committed((sizeof(frame_t) * max_active_frames) + average_frame_size * max_active_frames);

            vmem_arena_t* arena = (vmem_arena_t*)a.commit(sizeof(vmem_arena_t));
            *arena = a;

            m_arena[i] = arena;
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
            ASSERT(m_active_lane == 0 || m_arena[0]->m_pos == 0);
            m_active_lane = 1 - m_active_lane;

            if (m_active_frames[m_active_lane] == m_ended_frames[m_active_lane])
            {
                m_active_frames[m_active_lane] = 0;
                m_ended_frames[m_active_lane]  = 0;
                m_arena[m_active_lane]->reset(); // Reset the arena for the new lane
            }
            else
            {
                // We are trying to switch lanes, but the other lane is still occupied.
                // This should not happen, as we are supposed to end all frames before switching lanes.
                ASSERT(false);
            }
        }

        frame_t* frame_array           = (frame_t*)m_arena[m_active_lane]->m_base;
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

        frame_t* frame_array = (frame_t*)m_arena[lane]->m_base;
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

        byte* p = (byte*)m_arena[m_active_lane]->commit(size, alignment);

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
