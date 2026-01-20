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
        nheap::context_t* m_context;
        nheap::resize_fn  m_resize_fn;
        arena_t*          m_arena;
        void*             m_save_point;
    };

    heap_t* g_heap_create(int_t initial_size, int_t reserved_size);
    void*   g_heap_alloc(heap_t* allocator, u32 size);
    void*   g_heap_alloc_fill(heap_t* allocator, u32 size, u32 fill);
    void    g_heap_dealloc(heap_t* allocator, void* ptr);
    void    g_heap_release(heap_t* allocator);

    // Some C++ style helper functions
    template <typename T> inline T*   g_allocate(heap_t* heap) { return (T*)g_heap_alloc(heap, sizeof(T)); }
    template <typename T> inline void g_deallocate(heap_t* heap, T* ptr) { g_heap_dealloc(heap, (void*)ptr); }
    template <typename T> inline T*   g_allocate_and_clear(heap_t* heap) { return (T*)g_heap_alloc_fill(heap, sizeof(T), 0); }
    template <typename T> inline T*   g_allocate_array(heap_t* heap, u32 maxsize) { return (T*)g_heap_alloc(heap, maxsize * sizeof(T)); }
    template <typename T> inline T*   g_allocate_array_and_clear(heap_t* heap, u32 maxsize) { return (T*)g_heap_alloc_fill(heap, maxsize * sizeof(T), 0); }
    template <typename T> inline T*   g_allocate_array_and_fill(heap_t* heap, u32 maxsize, u32 fill) { return (T*)g_heap_alloc_fill(heap, maxsize * sizeof(T), fill); }

}; // namespace ncore

#endif /// __C_HEAP_ALLOCATOR_H__
