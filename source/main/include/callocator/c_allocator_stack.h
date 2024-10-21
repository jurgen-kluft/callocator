#ifndef __C_STACK_ALLOCATOR_H__
#define __C_STACK_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    // Stack allocator
    // The frame allocator is a specialized allocator. You can use it when you are allocating different size blocks that
    // all have a life-time that doesn't differ much, like one single frame of a game, or when passing an allocator to
    // a JSON parser that will allocate a lot of small blocks and then deallocate them all at once.
    // This allocator is very very fast in allocations O(1) and deallocations O(1).
    class stack_alloc_scope_t;
    class stack_alloc_t : protected alloc_t
    {
    public:
        stack_alloc_t();
        virtual ~stack_alloc_t() {}

        void          setup(void* beginAddress, u32 size);
        D_INLINE bool is_valid() const { return m_buffer_begin != nullptr; }
        D_INLINE bool is_empty() const { return m_allocation_count == 0; }
        void          reset();

        DCORE_CLASS_PLACEMENT_NEW_DELETE

    protected:
        virtual void* v_allocate(u32 size, u32 alignment);
        virtual void  v_deallocate(void* ptr);
        virtual void  v_restore_cursor(u8* cursor, int_t allocation_count);

        friend class stack_alloc_scope_t;

        u8*   m_buffer_begin;
        u8*   m_buffer_cursor;
        u8*   m_buffer_end;
        int_t m_allocation_count;
    };

    class stack_alloc_scope_t
    {
        stack_alloc_t* m_allocator;
        u8*            m_buffer_cursor;
        int_t          m_allocation_count;

    public:
        stack_alloc_scope_t(stack_alloc_t* allocator) : m_allocator(allocator), m_buffer_cursor(allocator->m_buffer_cursor), m_allocation_count(allocator->m_allocation_count) {}
        ~stack_alloc_scope_t() { m_allocator->v_restore_cursor(m_buffer_cursor, m_allocation_count); }

        D_INLINE void* allocate(u32 size, u32 alignment) { return m_allocator->v_allocate(size, alignment); }
        D_INLINE void  deallocate(void* ptr) { m_allocator->v_deallocate(ptr); }
        D_INLINE bool  is_empty() const { return m_allocator->m_allocation_count == 0; }

        template <typename T, typename... Args> T* construct(Args... args)
        {
            void* mem    = allocate(sizeof(T), sizeof(void*));
            T*    object = new (mem) T(args...);
            return object;
        }

        template <typename T> void destruct(T* p)
        {
            p->~T();
            deallocate(p);
        }
    };

}; // namespace ncore

#endif
