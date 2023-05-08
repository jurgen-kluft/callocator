#include "ccore/c_target.h"
#include "ccore/c_debug.h"
#include "cbase/c_memory.h"
#include "cbase/c_integer.h"
#include "cbase/c_allocator.h"

#include "callocator/private/c_freelist.h"

namespace ncore
{
    /**
    @brief      Fixed size type, element
    @desc       It implements linked list behavior for free elements in the block.
                Works on 32-bit systems since we use indexing here instead of pointers.
    **/
    struct freelist_t::item_t
    {
        freelist_t::item_t* getNext(freelist_t const* info) { return info->ptr_of(mIndex); }
        void                setNext(freelist_t const* info, freelist_t::item_t* next) { mIndex = info->idx_of(next); }
        void*               getObject() { return (void*)this; }

    private:
        u32 mIndex;
    };

    freelist_t::freelist_t() : mAllocator(nullptr), mElemSize(0), mElemAlignment(0), mUsed(0), mSize(0), mElementArray(0), mFreeList(nullptr) {}

    void freelist_t::init_with_array(u8* array, u32 array_size, u32 elem_size, u32 elem_alignment)
    {
        // Check parameters
        ASSERT(mElemSize >= sizeof(void*));

        mAllocator    = nullptr;
        mElementArray = array;

        // Take over the parameters
        mElemSize      = elem_size;
        mElemAlignment = elem_alignment;
        mUsed          = 0;

        // Clamp/Guard parameters
        mElemAlignment = mElemAlignment == 0 ? D_ALIGNMENT_DEFAULT : mElemAlignment;
        mElemAlignment = math::alignUp(mElemAlignment, (u32)sizeof(void*)); // Align element alignment to the size of a pointer
        mElemSize      = math::alignUp(mElemSize, (u32)sizeof(void*));      // Align element size to the size of a pointer
        mElemSize      = math::alignUp(mElemSize, mElemAlignment);          // Align element size to a multiple of element alignment
        mSize          = (array_size / mElemSize);

        init_list();
    }

    void freelist_t::init_with_alloc(alloc_t* allocator, u32 elem_size, u32 elem_alignment, s32 count)
    {
        // Check parameters
        ASSERT(mElemSize >= sizeof(void*));
        ASSERT(count != 0);

        mAllocator = allocator;
        mUsed      = 0;
        mSize      = count;

        // Take over the parameters
        mElemSize      = elem_size;
        mElemAlignment = elem_alignment;

        // Clamp/Guard parameters
        mElemAlignment = mElemAlignment == 0 ? D_ALIGNMENT_DEFAULT : mElemAlignment;
        mElemAlignment = math::alignUp(mElemAlignment, (u32)sizeof(void*)); // Align element alignment to the size of a pointer
        mElemSize      = math::alignUp(mElemSize, (u32)sizeof(void*));      // Align element size to the size of a pointer
        mElemSize      = math::alignUp(mElemSize, mElemAlignment);          // Align element size to a multiple of element alignment

        // Initialize the element array
        mElementArray = (u8*)allocator->allocate(mElemSize * mSize, mElemAlignment);

        init_list();
    }

    void freelist_t::release()
    {
        if (mAllocator != nullptr)
        {
            mAllocator->deallocate(mElementArray);
            mElementArray = nullptr;
            mAllocator    = nullptr;
        }
    }

    void freelist_t::init_list()
    {
        mUsed     = 0;
        mFreeList = ptr_of(0);
        for (s32 i = 1; i < size(); ++i)
        {
            item_t* e = ptr_of(i - 1);
            item_t* n = ptr_of(i);
            e->setNext(this, n);
        }
        item_t* last = ptr_of(size() - 1);
        last->setNext(this, nullptr);
    }

    freelist_t::item_t* freelist_t::alloc()
    {
        item_t* current = mFreeList;
        if (current != nullptr)
        {
            mFreeList = current->getNext(this);
            mUsed++;
        }
        return current;
    }

    void freelist_t::free(item_t* item)
    {
        item->setNext(this, mFreeList);
        mFreeList = item;
        --mUsed;
    }

}; // namespace ncore
