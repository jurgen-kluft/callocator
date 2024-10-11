#ifndef __C_FORWARD_ALLOCATOR_H__
#define __C_FORWARD_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    // Frame allocator (life cycle limited allocator)
    // The frame allocator is a specialized allocator. You can use it when you are allocating different size blocks that
    // all have a life-time that doesn't differ much, like one single frame of a game, or when passing an allocator to
    // a JSON parser that will allocate a lot of small blocks and then deallocate them all at once.
    // This allocator is very very fast in allocations O(1) and deallocations O(1).
    class frame_alloc_t : public alloc_t
    {
    public:
        frame_alloc_t();
        frame_alloc_t(void* beginAddress, u32 size);
        virtual ~frame_alloc_t();

        bool is_valid() const { return buffer_begin_ != nullptr; }
        void reset();

        // Each frame allocator can handle one checkout, however you can checkout on the returned frame allocator
        frame_alloc_t checkout(u32 within_size, u32 alignment) { return v_checkout(within_size, alignment); }
        void          commit(frame_alloc_t const& f) { v_commit(f); }

        DCORE_CLASS_PLACEMENT_NEW_DELETE

    private:
        virtual frame_alloc_t v_checkout(u32 size, u32 alignment);
        virtual void          v_commit(frame_alloc_t const&);
        virtual void*         v_allocate(u32 size, u32 alignment);
        virtual void          v_deallocate(void* ptr);

        u8* buffer_begin_;
        u8* buffer_cursor_;
        u8* buffer_end_;
        s32 checkout_;
        s32 num_allocations_;

        frame_alloc_t(const frame_alloc_t&);
        frame_alloc_t& operator=(const frame_alloc_t&);
    };

}; // namespace ncore

#endif /// __C_FORWARD_ALLOCATOR_H__
