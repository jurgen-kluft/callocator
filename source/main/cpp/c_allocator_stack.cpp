#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "ccore/c_math.h"

#include "callocator/c_allocator_stack.h"

namespace ncore
{
    stack_alloc_t::stack_alloc_t() : m_buffer_begin(nullptr), m_buffer_cursor(nullptr), m_buffer_end(nullptr), m_allocation_count(0) {}

    void stack_alloc_t::setup(void* beginAddress, u32 size)
    {
        m_buffer_begin     = (u8*)beginAddress;
        m_buffer_cursor    = (u8*)beginAddress;
        m_buffer_end       = (u8*)beginAddress + size;
        m_allocation_count = 0;
    }

    void stack_alloc_t::reset()
    {
        m_buffer_cursor    = m_buffer_begin;
        m_allocation_count = 0;
    }

    void* stack_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        if (m_buffer_begin == nullptr)
            return nullptr;
        u8* const ptr   = (u8*)math::alignUp((u64)m_buffer_cursor, alignment);
        m_buffer_cursor = ptr + size;
        m_allocation_count++;
        return ptr;
    }

    void stack_alloc_t::v_deallocate(void* ptr)
    {
        if (ptr == nullptr)
            return;
        ASSERT(m_allocation_count > 0);
        m_allocation_count--;
    }

    void stack_alloc_t::v_restore_cursor(u8* cursor, int_t allocation_count)
    {
        // Has the user forgot to deallocate one or more allocations?
        ASSERT(m_allocation_count == allocation_count);
        m_buffer_cursor    = cursor;
        m_allocation_count = allocation_count;
    }

}; // namespace ncore
