#ifndef __C_HEAP_ALLOCATOR_H__
#define __C_HEAP_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

namespace ncore
{
    struct heap_t;

    namespace nheap
    {
        typedef void* (*resize_fn)(heap_t* h, int_t new_size);

        struct context_t;
    } // namespace nheap

    struct heap_t
    {
        arena_t*          m_arena;
        void*             m_save_point;
        nheap::context_t* m_context;
        nheap::resize_fn  m_resize_fn;
    };

    heap_t* g_create_heap(int_t initial_size, int_t reserved_size);
    void    g_release_heap(heap_t* allocator);

}; // namespace ncore

#endif /// __C_HEAP_ALLOCATOR_H__
