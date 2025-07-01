#ifndef __C_LINEAR_ALLOCATOR_H__
#define __C_LINEAR_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    // Linear allocator
    // The linear allocator is a specialized allocator. You can use it when you are allocating different size blocks that
    // all have a life-time that are all part of one group which later can be deallocated all at once.
    // This allocator is very fast in allocating O(1), deallocating does not do anything.
    class linear_alloc_t : public alloc_t
    {
    public:
        inline void reset() { v_reset(); }

    protected:
        virtual void v_reset() = 0;
    };

    linear_alloc_t* g_create_linear_allocator(int_t initial_size, int_t reserved_size);
    int_t           g_current_size(linear_alloc_t* allocator);
    void            g_destroy_allocator(linear_alloc_t*);

}; // namespace ncore

#endif /// __C_FORWARD_ALLOCATOR_H__
