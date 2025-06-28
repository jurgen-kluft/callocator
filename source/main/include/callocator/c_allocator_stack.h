#ifndef __C_STACK_ALLOCATOR_H__
#define __C_STACK_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    struct vmem_arena_t;
    class stack_alloc_scope_t;

    class stack_alloc_t : public alloc_t
    {
    public:
        inline void  restore_point(void* point) { v_restore_point(point); }
        inline void* save_point() { return v_save_point(); }

    protected:
        virtual void  v_restore_point(void* point) = 0;
        virtual void* v_save_point()               = 0;
    };

    // Stack allocator
    //
    // The stack allocator is a specialized allocator. You can use it when you are allocating temporary
    // objects that are only used in a limited scope, for example, in a function. Furthermore, it is
    // not thread safe, so there should be one instance of the stack allocator per thread.
    //
    class stack_allocator_t : protected stack_alloc_t
    {
    public:
        stack_allocator_t();
        virtual ~stack_allocator_t() {}

        void          setup(vmem_arena_t* arena);
        D_INLINE bool is_valid() const { return m_arena != nullptr; }
        D_INLINE bool is_empty() const { return m_allocation_count == 0; }
        void          reset();

        DCORE_CLASS_PLACEMENT_NEW_DELETE

        vmem_arena_t* m_arena;
        int_t         m_allocation_count;

    protected:
        virtual void* v_allocate(u32 size, u32 alignment) final;
        virtual void  v_deallocate(void* ptr) final;
        virtual void  v_restore_point(void* point) final;
        virtual void* v_save_point() final;

        friend class stack_alloc_scope_t;
    };

    class stack_alloc_scope_t : public alloc_t
    {
        stack_alloc_t* m_allocator;
        void*          m_point;

    public:
        stack_alloc_scope_t();
        stack_alloc_scope_t(stack_allocator_t* allocator) : m_allocator(allocator) { m_point = m_allocator->save_point(); }
        ~stack_alloc_scope_t() { m_allocator->restore_point(m_point); }

    protected:
        virtual void* v_allocate(u32 size, u32 alignment) final { return m_allocator->allocate(size, alignment); }
        virtual void  v_deallocate(void* ptr) final { m_allocator->deallocate(ptr); }
    };

}; // namespace ncore

#endif
