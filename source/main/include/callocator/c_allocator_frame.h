#ifndef __C_FRAME_ALLOCATOR_H__
#define __C_FRAME_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    class frame_alloc_t : public alloc_t
    {
    public:
        inline s32  new_frame() { return v_new_frame(); }
        inline bool end_frame() { return v_end_frame(); }
        inline bool reset_frame(s32 frame_id) { return v_reset_frame(frame_id); }

    protected:
        virtual s32  v_new_frame()               = 0; // ends the previous frame and starts a new one, returns new frame id
        virtual bool v_end_frame()               = 0; // true if 'number of allocations' == 'number of deallocations'
        virtual bool v_reset_frame(s32 frame_id) = 0; // if a frame is obsolete, reset it so that it can be reused
    };

    // Frame allocator (frame scope allocator)
    // The frame allocator is a specialized allocator. You can use it when you are requesting specific
    // allocations that only have to live for N frames, like a single frame of a game, or when passing
    // an allocator to a JSON parser that will allocate a lot of small blocks and then deallocate them
    // all at once. So you do not (really) have to worry about deallocation of allocated memory.
    // This allocator is very very fast in allocating O(1) and deallocating O(1).
    // NOTE: We could make virtual memory version of this allocator, so that each frame does not have
    //       to know beforehand how much memory would need to be reserved.
    class frame_allocator_t : public frame_alloc_t
    {
    public:
        frame_allocator_t();
        virtual ~frame_allocator_t();

        struct frame_t
        {
            s32 m_frame_number;
            s32 m_num_allocations;
            s32 m_num_deallocations;
            s32 m_ended;
        };

        void setup(s32 max_active_frames, int_t average_frame_size, int_t max_reserved_size);
        void reset();

        DCORE_CLASS_PLACEMENT_NEW_DELETE

        s32           m_active_frames[2];  // keeping track of the number of active frames in each lane
        s32           m_ended_frames[2];   // keeping track of the number of ended frames in each lane
        s32           m_active_lane;       // Current lane, either 0 or 1, used for switching between two arenas
        s32           m_max_active_frames; // Maximum number of active frames, used for switching between two arenas
        frame_t*      m_current_frame;     // Current frame, used for allocating memory
        vmem_arena_t* m_arena[2];          // Virtual memory arenas

    private:
        virtual void* v_allocate(u32 size, u32 alignment) final;
        virtual void  v_deallocate(void* ptr) final;

        virtual s32  v_new_frame() final;
        virtual bool v_end_frame() final;
        virtual bool v_reset_frame(s32 frame_id) final;

        frame_allocator_t(const frame_allocator_t&)            = delete; // Disable copy constructor
        frame_allocator_t& operator=(const frame_allocator_t&) = delete; // Disable assignment operator
    };

}; // namespace ncore

#endif
