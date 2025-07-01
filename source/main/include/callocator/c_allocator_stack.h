#ifndef __C_STACK_ALLOCATOR_H__
#define __C_STACK_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    class stack_alloc_t : public alloc_t
    {
    public:
        inline void  restore_point(void* point) { v_restore_point(point); }
        inline void* save_point() { return v_save_point(); }

    protected:
        virtual void  v_restore_point(void* point) = 0;
        virtual void* v_save_point()               = 0;
    };

    class stack_alloc_scope_t
    {
        stack_alloc_t* m_allocator;
        void*          m_point;

    public:
        stack_alloc_scope_t(stack_alloc_t* allocator) : m_allocator(allocator) { m_point = m_allocator->save_point(); }
        ~stack_alloc_scope_t() { m_allocator->restore_point(m_point); }
    };

    stack_alloc_t* g_create_stack_allocator(int_t initial_size, int_t reserved_size);
    void           g_destroy_stack_allocator(stack_alloc_t* allocator);
}; // namespace ncore

#endif
