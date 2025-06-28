#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "ccore/c_math.h"
#include "ccore/c_vmem.h"
#include "cbase/c_context.h"

#include "callocator/c_allocator_stack.h"

namespace ncore
{
    stack_allocator_t::stack_allocator_t() : m_arena(nullptr), m_allocation_count(0) {}

    void stack_allocator_t::setup(vmem_arena_t* arena)
    {
        m_arena            = arena;
        m_allocation_count = 0;
    }

    void stack_allocator_t::reset()
    {
        m_arena->reset();
        m_allocation_count = 0;
    }

    void* stack_allocator_t::v_allocate(u32 size, u32 alignment) { return m_arena->commit(size, alignment); }

    void stack_allocator_t::v_deallocate(void* ptr)
    {
        if (ptr == nullptr)
            return;
        ASSERT(m_allocation_count > 0);
        m_allocation_count--;
    }

    void* stack_allocator_t::v_save_point()
    {
        int_t* ptr = (int_t*)m_arena->commit(sizeof(int_t));
        *ptr       = m_allocation_count;
        return ptr;
    }

    void stack_allocator_t::v_restore_point(void* point)
    {
        // Has the user forgotten to deallocate one or more allocations?
        int_t* allocation_count = (int_t*)point;
        ASSERT(m_allocation_count == *allocation_count);
        uint_t pos         = g_ptr_diff_in_bytes(point, m_arena->m_base);
        m_allocation_count = *allocation_count;
        m_arena->restore(pos);
    }

    stack_alloc_scope_t::stack_alloc_scope_t()
    {
        context_t context = g_current_context();
        m_allocator       = context.stack_alloc();
    }

}; // namespace ncore
