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

    frame_alloc_t::frame_alloc_t() : m_index(0), m_capacity(0), m_used(0), m_frame_array(nullptr) {}
    frame_alloc_t::~frame_alloc_t() {}

    void frame_alloc_t::reset()
    {
        m_index = 0;
        m_used  = ~((1 << m_capacity) - 1);

        for (u8 i = 0; i < m_capacity; ++i)
        {
            frame_t& cf            = m_frame_array[i];
            cf.m_buffer_cursor     = cf.m_buffer_begin;
            cf.m_num_allocations   = 0;
            cf.m_num_deallocations = 0;
        }
    }

    void frame_alloc_t::setup(frame_t* frame_array, u8 num_frames)
    {
        ASSERT(frame_array != nullptr && num_frames > 0 && num_frames <= (sizeof(m_used) * 8));
        m_frame_array = frame_array;
        m_capacity    = num_frames;
        reset();
    }

    void frame_alloc_t::begin_frame(u8 frame)
    {
        m_index = math::g_findFirstBit(~m_used);
        ASSERT(m_index < m_capacity);
        m_used |= (1 << m_index);
    }

    bool frame_alloc_t::end_frame()
    {
        ASSERT(m_index >= 0 && m_index < m_capacity);
        bool const valid = m_frame_array[m_index].m_num_allocations == m_frame_array[m_index].m_num_deallocations;
        m_index          = -1;
        return valid;
    }

    bool frame_alloc_t::reset_frame(u8 frame)
    {
        ASSERT(frame < m_capacity);

        m_used &= ~(1 << frame);

        bool const valid = m_frame_array[frame].m_num_allocations == m_frame_array[frame].m_num_deallocations;

        m_frame_array[frame].m_buffer_cursor     = m_frame_array[frame].m_buffer_begin;
        m_frame_array[frame].m_num_allocations   = 0;
        m_frame_array[frame].m_num_deallocations = 0;

        return valid;
    }

    void* frame_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        ASSERT(m_index >= 0 && m_index < m_capacity);
        frame_t& cf = m_frame_array[m_index];

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
        ASSERT(m_index >= 0 && m_index < m_capacity);
        frame_t& cf = m_frame_array[m_index];

        ASSERT(cf.m_num_allocations > cf.m_num_deallocations);
        cf.m_num_deallocations++;
    }

}; // namespace ncore
