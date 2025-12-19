#ifndef __C_STACK_ALLOCATOR_H__
#define __C_STACK_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"
#include "ccore/c_defer.h"

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

    template <> class defer_action_t<stack_alloc_t*>
    {
    public:
        stack_alloc_t* m_object;
        void*          m_point;
        defer_action_t(stack_alloc_t* object) : m_object(object) {}
        void open() { m_point = m_object->save_point(); }
        void close() { m_object->restore_point(m_point); }
    };

    stack_alloc_t* g_create_stack_allocator(int_t initial_size, int_t reserved_size);
    void           g_destroy_stack_allocator(stack_alloc_t* allocator);
}; // namespace ncore

#endif
