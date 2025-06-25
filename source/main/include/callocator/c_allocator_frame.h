#ifndef __C_FRAME_ALLOCATOR_H__
#define __C_FRAME_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    // Frame allocator (frame scope allocator)
    // The frame allocator is a specialized allocator. You can use it when you are requesting specific
    // allocations that only have to live for N frames, like a single frame of a game, or when passing
    // an allocator to a JSON parser that will allocate a lot of small blocks and then deallocate them
    // all at once. So you do not (really) have to worry about deallocation of allocated memory.
    // This allocator is very very fast in allocating O(1) and deallocating O(1).
    // NOTE: We could make virtual memory version of this allocator, so that each frame does not have
    //       to know beforehand how much memory would need to be reserved.
    class frame_alloc_t : public alloc_t
    {
    public:
        frame_alloc_t();
        virtual ~frame_alloc_t();

        struct frame_t
        {
            u8* m_buffer_begin;
            u8* m_buffer_cursor;
            u8* m_buffer_end;
            s32 m_num_allocations;
            s32 m_num_deallocations;

            void setup(u8* buffer, u32 size);
        };

        // Note: maximum 256 frames
        void setup(frame_t* frame_array, s16 num_frames);
        void reset();

        void begin_frame(s16 frame);
        bool end_frame(); // true if 'number of allocations' == 'number of deallocations'
        bool reset_frame(s16 frame);

        DCORE_CLASS_PLACEMENT_NEW_DELETE

        s16      m_capacity;
        s16      m_current_frame; // Current frame index
        s16      m_active_size;
        u32      m_active_mark[8];
        frame_t* m_frame_array;

    private:
        virtual void* v_allocate(u32 size, u32 alignment);
        virtual void  v_deallocate(void* ptr);

        frame_alloc_t(const frame_alloc_t&);
        frame_alloc_t& operator=(const frame_alloc_t&);
    };

}; // namespace ncore

#endif
