#ifndef __C_LINEAR_ALLOCATOR_H__
#define __C_LINEAR_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    // Linear allocator (life cycle limited allocator)
    // The linear allocator is a specialized allocator. You can use it when you are allocating different size blocks that
    // all have a life-time that doesn't differ much, like one or more frames of a game, or when passing an allocator to
    // a JSON parser that will allocate a lot of small blocks and then deallocate them all at once.
    // This allocator is fast in allocations O(1) and deallocations O(1).
    class linear_alloc_t : public alloc_t
    {
    public:
        linear_alloc_t();
        virtual ~linear_alloc_t();

        void setup(void* beginAddress, u32 size);

        bool is_valid() const;
        bool is_empty() const;
        void reset();

        DCORE_CLASS_PLACEMENT_NEW_DELETE

        struct node_t;

    private:
        virtual void* v_allocate(u32 size, u32 alignment) final;
        virtual void  v_deallocate(void* ptr) final;

        node_t*         m_buffer_begin;
        node_t*         m_buffer_cursor;
        node_t*         m_buffer_end;

        linear_alloc_t(const linear_alloc_t&)            = delete;
        linear_alloc_t& operator=(const linear_alloc_t&) = delete;
    };

}; // namespace ncore

#endif /// __C_FORWARD_ALLOCATOR_H__
