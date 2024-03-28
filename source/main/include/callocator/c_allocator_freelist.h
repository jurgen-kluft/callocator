#ifndef __C_ALLOCATOR_FREELIST_H__
#define __C_ALLOCATOR_FREELIST_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "cbase/c_allocator.h"

namespace ncore
{
    class freelist_t : public fsadexed_t
    {
    public:
        freelist_t();
        virtual ~freelist_t() {}

        void init(void* mem, u32 mem_size, u32 elem_size);
        void reset();

        virtual u32   v_size() const { return mElementSize; }
        virtual void* v_allocate();
        virtual void  v_deallocate(void* p);
        virtual u32   v_ptr2idx(void* p) const;
        virtual void* v_idx2ptr(u32 idx) const;

        DCORE_CLASS_PLACEMENT_NEW_DELETE

    protected:
        const u32 NULL_INDEX = 0xffffffff;
        u8*       mElementArray;
        u32       mElementSize;
        u32       mElementMaxIndex;
        u32       mElementIndex;
        u32       mElementFreeList;

    private:
        // Copy construction and assignment are forbidden
        freelist_t(const freelist_t&);
        freelist_t& operator=(const freelist_t&);
    };

}; // namespace ncore

#endif /// __C_ALLOCATOR_FREELIST_H__
