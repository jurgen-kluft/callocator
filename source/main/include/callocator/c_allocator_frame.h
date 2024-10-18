#ifndef __C_FRAME_ALLOCATOR_H__
#define __C_FRAME_ALLOCATOR_H__
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

        bool is_valid() const { return m_buffer_begin != nullptr; }
        void reset();

        DCORE_CLASS_PLACEMENT_NEW_DELETE

    private:
        virtual void* v_allocate(u32 size, u32 alignment);
        virtual void  v_deallocate(void* ptr);

        u8* m_buffer_begin;
        u8* m_buffer_cursor;
        u8* m_buffer_end;
        s32 m_num_allocations;

        frame_alloc_t(const frame_alloc_t&);
        frame_alloc_t& operator=(const frame_alloc_t&);
    };

}; // namespace ncore

#endif
