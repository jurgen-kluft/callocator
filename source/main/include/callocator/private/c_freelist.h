#ifndef __C_FREELIST_H__
#define __C_FREELIST_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

namespace ncore
{
    struct freelist_t
    {
        freelist_t();

        const u32 NULL_INDEX = 0xffffffff;

        void init(u8* array, u32 array_size, u32 elem_size);
        void reset();

        inline bool valid() const { return (mElementArray != nullptr); }
        inline s32  size() const { return mSize; }
        inline s32  max() const { return mSizeMax; }

        struct item_t;

        inline item_t* ptr_of(s32 index) const
        {
            if (index == NULL_INDEX)
                return nullptr;
            return (item_t*)(mElementArray + (index * mElemSize));
        }

        inline u32 idx_of(item_t const* element) const
        {
            if (element == nullptr)
                return NULL_INDEX;
            s32 idx = ((s32)((u8 const*)element - (u8 const*)mElementArray)) / (s32)mElemSize;
            if (idx >= 0 && idx < mSize)
                return idx;
            return NULL_INDEX;
        }

        item_t* alloc();
        void    free(item_t*);

    private:
        u8*      mElementArray;
        u32      mFreeIndex;
        u32      mFreeList;
        u32      mElemSize;
        u32      mSize;
        u32      mSizeMax;
    };

}; // namespace ncore

#endif /// __C_FREELIST_H__
