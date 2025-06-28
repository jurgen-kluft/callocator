#ifndef __C_HEAP_ALLOCATOR_H__
#define __C_HEAP_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    struct vmem_arena_t;

    namespace nheap
    {
        struct context_t;

        class allocator_t : public alloc_t
        {
        public:
            allocator_t(context_t* context);
            virtual ~allocator_t();

            virtual void* v_allocate(u32 size, u32 alignment) final;
            virtual void  v_deallocate(void* ptr) final;

            virtual bool  v_check(const char*& error_description) const;
            virtual void* v_resize(u64 size) = 0;

            DCORE_CLASS_PLACEMENT_NEW_DELETE

            context_t* m_context;
        };

    } // namespace nheap

    alloc_t* g_create_heap(int_t initial_size, int_t reserved_size);
    void     g_release_heap(alloc_t* allocator);

}; // namespace ncore

#endif /// __C_HEAP_ALLOCATOR_H__
