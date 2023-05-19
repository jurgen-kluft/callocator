#include "ccore/c_target.h"
#include "ccore/c_debug.h"
#include "cbase/c_memory.h"
#include "cbase/c_integer.h"
#include "cbase/c_allocator.h"

#include "callocator/private/c_freelist.h"

namespace ncore
{
    struct freelist_t::item_t
    {
        inline u32  getNext() const { mIndex; }
        inline void setNext(u32 next) { mIndex = next; }

    private:
        u32 mIndex;
    };

    freelist_t::freelist_t() : mElementArray(0), mFreeIndex(0), mFreeList(NULL_INDEX), mElemSize(0), mSize(0), mSizeMax(0) {}

    void freelist_t::init(u8* array, u32 array_size, u32 elem_size)
    {
        // Check parameters
        ASSERT(mElemSize >= sizeof(u32));

        mElementArray = array;

        mElemSize = elem_size;
        mSize     = 0;
        mSizeMax  = array_size / mElemSize;

        mFreeIndex = 0;
        mFreeList  = NULL_INDEX;
    }

    void freelist_t::reset()
    {
        mFreeIndex = 0;
        mFreeList  = NULL_INDEX;
        mSize      = 0;
    }

    freelist_t::item_t* freelist_t::alloc()
    {
        if (mFreeList == NULL_INDEX)
        {
            if (mFreeIndex < mSizeMax)
            {
                // Allocate new element
                item_t* current = ptr_of(mFreeIndex);
                ++mFreeIndex;
                ++mSize;
                return current;
            }
            else
            {
                // No more free elements
                return nullptr;
            }
        }
        item_t* item = ptr_of(mFreeList);
        if (item != nullptr)
        {
            mFreeList = item->getNext();
            ++mSize;
        }
        return item;
    }

    void freelist_t::free(item_t* item)
    {
        item->setNext(mFreeList);
        mFreeList = idx_of(item);
        --mSize;
    }

}; // namespace ncore
