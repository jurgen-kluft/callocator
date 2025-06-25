#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "ccore/c_math.h"

#include "callocator/c_allocator_frame.h"

namespace ncore
{
    void frame_alloc_t::frame_t::setup(u8* buffer, u32 size)
    {
        m_buffer_begin      = buffer;
        m_buffer_cursor     = buffer;
        m_buffer_end        = buffer + size;
        m_num_allocations   = 0;
        m_num_deallocations = 0;
    }

    frame_alloc_t::frame_alloc_t() : m_current_frame(0), m_capacity(0), m_frame_array(nullptr) {}
    frame_alloc_t::~frame_alloc_t() {}

    void frame_alloc_t::reset()
    {
        m_current_frame = 0;
        m_active_size   = 0;

        m_active_mark[0] = 0;
        m_active_mark[1] = 0;
        m_active_mark[2] = 0;
        m_active_mark[3] = 0;

        for (u8 i = 0; i < m_capacity; ++i)
        {
            frame_t& cf            = m_frame_array[i];
            cf.m_buffer_cursor     = cf.m_buffer_begin;
            cf.m_num_allocations   = 0;
            cf.m_num_deallocations = 0;
        }
    }

    void frame_alloc_t::setup(frame_t* frame_array, s16 num_frames)
    {
        ASSERT(frame_array != nullptr && num_frames > 0 && num_frames <= 256);
        m_frame_array = frame_array;
        m_capacity    = num_frames;
        reset();
    }

    void frame_alloc_t::begin_frame(s16 frame)
    {
        if (m_active_size >= m_capacity)
        {
            ASSERT(false); // No more frames available, so we cannot allocate a new frame
            m_current_frame = -1;
            return;
        }

        for (u8 i = 0; i < 8; ++i)
        {
            s8 const slot = math::g_findFirstBit(~m_active_mark[i]);
            if (slot >= 0 && slot < 8)
            {
                m_current_frame = (i << 3) + slot;
                m_active_mark[i] |= ((u64)1 << slot);
                m_active_size++;
                break;
            }
        }
    }

    bool frame_alloc_t::end_frame()
    {
        ASSERT(m_current_frame >= 0 && m_current_frame < m_capacity);
        bool const valid = m_frame_array[m_current_frame].m_num_allocations == m_frame_array[m_current_frame].m_num_deallocations;
        m_current_frame  = -1;
        return valid;
    }

    bool frame_alloc_t::reset_frame(s16 frame)
    {
        ASSERT(frame < m_capacity);

        m_active_mark[frame >> 3] &= ~(1 << (frame & 7));
        m_active_size--;

        bool const valid = m_frame_array[frame].m_num_allocations == m_frame_array[frame].m_num_deallocations;

        m_frame_array[frame].m_buffer_cursor     = m_frame_array[frame].m_buffer_begin;
        m_frame_array[frame].m_num_allocations   = 0;
        m_frame_array[frame].m_num_deallocations = 0;

        return valid;
    }

    void* frame_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        ASSERT(m_current_frame >= 0 && m_current_frame < m_capacity);
        frame_t& cf = m_frame_array[m_current_frame];

        u8* p = ncore::g_ptr_align(cf.m_buffer_cursor, alignment);
        if ((p + size) > cf.m_buffer_end)
            return nullptr;
#ifdef TARGET_DEBUG
        nmem::memset(p, 0xcd, size);
#endif
        cf.m_buffer_cursor = p + size;
        cf.m_num_allocations++;
        return p;
    }

    void frame_alloc_t::v_deallocate(void* ptr)
    {
        ASSERT(m_current_frame >= 0 && m_current_frame < m_capacity);
        frame_t& cf = m_frame_array[m_current_frame];

        ASSERT(cf.m_num_allocations > cf.m_num_deallocations);
        cf.m_num_deallocations++;
    }

}; // namespace ncore
