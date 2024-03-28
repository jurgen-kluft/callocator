#include "ccore/c_target.h"
#include "ccore/c_debug.h"
#include "cbase/c_memory.h"
#include "cbase/c_integer.h"
#include "cbase/c_allocator.h"

#include "callocator/c_allocator_freelist.h"

namespace ncore
{
    freelist_t::freelist_t()
        : mElementArray(nullptr)
        , mElementSize(0)
        , mElementMaxIndex(0)
        , mElementIndex(0)
        , mElementFreeList(NULL_INDEX)
    {
    }

    void* freelist_t::v_idx2ptr(u32 index) const
    {
        if (index == NULL_INDEX)
            return nullptr;
        return (mElementArray + (index * mElementSize));
    }

    u32 freelist_t::v_ptr2idx(void* element) const
    {
        if (element == nullptr)
            return NULL_INDEX;
        uint_t idx = g_ptr_diff_bytes(mElementArray, element) / mElementSize;
        return (idx >= 0 && idx < mElementMaxIndex) ? idx : NULL_INDEX;
    }

    void freelist_t::init(void* mem, u32 mem_size, u32 elem_size)
    {
        mElementArray = (u8*)mem;
        mElementSize = elem_size;
        mElementIndex = 0;
        mElementFreeList = NULL_INDEX;
        mElementMaxIndex = mem_size / elem_size;
    }

    void freelist_t::reset()
    {
        mElementIndex    = 0;
        mElementFreeList = NULL_INDEX;
    }

    void* freelist_t::v_allocate()
    {
        if (mElementFreeList != NULL_INDEX)
        {
            u32  index       = mElementFreeList;
            u32* elem        = (u32*)v_idx2ptr(index);
            mElementFreeList = *elem;
            return elem;
        }
        else if (mElementIndex < mElementMaxIndex)
        {
            return v_idx2ptr(mElementIndex++);
        }
        return nullptr;
    }

    void freelist_t::v_deallocate(void* p)
    {
        if (p == nullptr)
            return;
        u32 idx = (u32)(g_ptr_diff_bytes(mElementArray, p) / mElementSize);
        ASSERT (idx >= 0 && idx < mElementMaxIndex);
        u32* elem = (u32*)p;
        *elem = mElementFreeList;
        mElementFreeList = idx;
    }

}; // namespace ncore
