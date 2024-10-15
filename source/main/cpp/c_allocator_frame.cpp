#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"

#include "callocator/c_allocator_frame.h"

namespace ncore
{
    frame_alloc_t::frame_alloc_t() : m_buffer_begin(nullptr), m_buffer_cursor(nullptr), m_buffer_end(nullptr), m_num_allocations(0) {}
    frame_alloc_t::frame_alloc_t(void* beginAddress, u32 size) : m_buffer_begin((u8*)beginAddress), m_buffer_cursor((u8*)beginAddress), m_buffer_end((u8*)beginAddress + size), m_num_allocations(0) {}
    frame_alloc_t::~frame_alloc_t() {}

    void frame_alloc_t::reset()
    {
        m_buffer_cursor   = (u8*)m_buffer_begin;
        m_num_allocations = 0;
    }

    void* frame_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        u8* p = ncore::g_ptr_align(m_buffer_cursor, alignment);
        if ((p + size) > m_buffer_end)
            return nullptr;
#ifdef TARGET_DEBUG
        nmem::memset(p, 0xcd, size);
#endif
        m_buffer_cursor = p + size;
        m_num_allocations++;
        return p;
    }

    void frame_alloc_t::v_deallocate(void* ptr)
    {
        ASSERT(m_num_allocations > 0);
        --m_num_allocations;
    }

}; // namespace ncore
