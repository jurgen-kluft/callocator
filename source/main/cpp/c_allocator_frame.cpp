#include "ccore/c_target.h"
#include "cbase/c_memory.h"
#include "cbase/c_limits.h"
#include "cbase/c_integer.h"
#include "cbase/c_allocator.h"
#include "cbase/c_integer.h"

#include "callocator/c_allocator_frame.h"

namespace ncore
{
    frame_alloc_t::frame_alloc_t() : buffer_begin_(nullptr), buffer_cursor_(nullptr), buffer_end_(nullptr), checkout_(0), num_allocations_(0) {}
    frame_alloc_t::frame_alloc_t(void* beginAddress, u32 size) : buffer_begin_((u8*)beginAddress), buffer_cursor_((u8*)beginAddress), buffer_end_((u8*)beginAddress + size), checkout_(0), num_allocations_(0) {}
    frame_alloc_t::~frame_alloc_t() {}

    void frame_alloc_t::reset()
    {
        buffer_cursor_   = (u8*)buffer_begin_;
        checkout_        = 0;
        num_allocations_ = 0;
    }

    frame_alloc_t frame_alloc_t::v_checkout(u32 size, u32 alignment)
    {
        ASSERT(checkout_ == 0);
        u8* p     = ncore::g_align_ptr(buffer_cursor_, alignment);
        if ((p + size) > buffer_end_)
        {
            return frame_alloc_t(nullptr, 0);
        }
        checkout_ = math::alignUp(size, alignment);
#ifdef TARGET_DEBUG
        nmem::memset(buffer_cursor_, 0xCDCDCDCD, checkout_);
#endif
        num_allocations_++;
        return frame_alloc_t(p, checkout_);
    }

    void frame_alloc_t::v_commit(frame_alloc_t const& f)
    {
        u64 const commit_size = (u8*)f.buffer_cursor_ - (u8*)(buffer_cursor_);
        buffer_cursor_        = f.buffer_cursor_;
        ASSERT(commit_size <= checkout_);
        checkout_ = 0;
    }

    void* frame_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        ASSERT(checkout_ == 0); // We are in the middle of a checkout
        u8* p = ncore::g_align_ptr(buffer_cursor_, alignment);
        if ((p + size) > buffer_end_)
            return nullptr;
        buffer_cursor_ = p + size;
        num_allocations_++;
        return p;
    }

    void frame_alloc_t::v_deallocate(void* ptr)
    {
        ASSERT(num_allocations_ > 0);
        --num_allocations_;
    }

}; // namespace ncore
