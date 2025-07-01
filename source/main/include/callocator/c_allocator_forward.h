#ifndef __C_LINEAR_ALLOCATOR_H__
#define __C_LINEAR_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    // Forward allocator (life cycle limited allocator)
    //
    // The forward allocator is a specialized allocator. You can use it when you are allocating different size blocks that
    // all have a life-time that doesn't differ much, like one or more frames of a game, or when passing an allocator to
    // a JSON parser that will allocate a lot of small blocks and then deallocate them all at once. This means that if you
    // are tracking all allocations and deallocations on a linear time-line, you would see all active allocations moving
    // forward in time. With this you can see that at some point in time the cursor will wrap around to the beginning of the
    // memory, where it will continue allocating from.
    //
    // This allocator is pretty optimal in allocating O(1) and deallocating O(1).
    //

    class forward_alloc_t;
    forward_alloc_t* g_create_forward_allocator(void* beginAddress, u32 size);
    void             g_destroy_forward_allocator(forward_alloc_t* allocator);

}; // namespace ncore

#endif /// __C_FORWARD_ALLOCATOR_H__
