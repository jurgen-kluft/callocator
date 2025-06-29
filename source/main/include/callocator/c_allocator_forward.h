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
    class forward_alloc_t : public alloc_t
    {
    public:
        forward_alloc_t();
        virtual ~forward_alloc_t();

        void setup(void* beginAddress, u32 size);

        bool is_empty() const;
        void reset();

        DCORE_CLASS_PLACEMENT_NEW_DELETE

        struct node_t;
        node_t* m_buffer_begin;
        node_t* m_buffer_cursor;
        node_t* m_buffer_end;

    private:
        virtual void* v_allocate(u32 size, u32 alignment) final;
        virtual void  v_deallocate(void* ptr) final;
    };

}; // namespace ncore

#endif /// __C_FORWARD_ALLOCATOR_H__
