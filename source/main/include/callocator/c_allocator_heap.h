#ifndef __C_HEAP_ALLOCATOR_H__
#define __C_HEAP_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif


namespace ncore
{
    class alloc_t;

    alloc_t* g_create_heap(int_t initial_size, int_t reserved_size);
    void     g_release_heap(alloc_t* allocator);

}; // namespace ncore

#endif /// __C_HEAP_ALLOCATOR_H__
