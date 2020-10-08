#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_memory.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"

#include "xallocator/private/x_freelist.h"

namespace xcore
{
    xfreelist_t::xfreelist_t() : mAllocator(NULL), mElemSize(0), mElemAlignment(0), mUsed(0), mSize(0), mElementArray(0), mFreeList(NULL) {}

    void xfreelist_t::init_with_array(xbyte* array, u32 array_size, u32 elem_size, u32 elem_alignment)
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
        mElemAlignment = mElemAlignment == 0 ? X_ALIGNMENT_DEFAULT : mElemAlignment;
        mElemAlignment = xalignUp(mElemAlignment, sizeof(void*)); // Align element alignment to the size of a pointer
        mElemSize      = xalignUp(mElemSize, sizeof(void*));      // Align element size to the size of a pointer
        mElemSize      = xalignUp(mElemSize, mElemAlignment);     // Align element size to a multiple of element alignment
        mSize          = (array_size / mElemSize);

        init_list();
    }

    void xfreelist_t::init_with_alloc(xalloc* allocator, u32 elem_size, u32 elem_alignment, s32 count)
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
        mElemAlignment = mElemAlignment == 0 ? X_ALIGNMENT_DEFAULT : mElemAlignment;
        mElemAlignment = xalignUp(mElemAlignment, sizeof(void*)); // Align element alignment to the size of a pointer
        mElemSize      = xalignUp(mElemSize, sizeof(void*));      // Align element size to the size of a pointer
        mElemSize      = xalignUp(mElemSize, mElemAlignment);     // Align element size to a multiple of element alignment

        // Initialize the element array
        mElementArray = (xbyte*)allocator->allocate(mElemSize * mSize, mElemAlignment);

        init_list();
    }

    void xfreelist_t::release()
    {
        if (mAllocator != nullptr)
        {
            mAllocator->deallocate(mElementArray);
            mElementArray = nullptr;
            mAllocator    = nullptr;
        }
    }

    void xfreelist_t::init_list()
    {
        mUsed     = 0;
        mFreeList = ptr_of(0);
        for (s32 i = 1; i < size(); ++i)
        {
            xitem_t* e = ptr_of(i - 1);
            xitem_t* n = ptr_of(i);
            e->setNext(this, n);
        }
        xitem_t* last = ptr_of(size() - 1);
        last->setNext(this, NULL);
    }

    xfreelist_t::xitem_t* xfreelist_t::alloc()
    {
        xitem_t* current = mFreeList;
        if (current != NULL)
        {
            mFreeList = current->getNext(this);
            mUsed++;
        }
        return current;
    }

    void xfreelist_t::free(xitem_t* item)
    {
        item->setNext(this, mFreeList);
        mFreeList = item;
        --mUsed;
    }

    /**
    @brief      Fixed size type, element
    @desc       It implements linked list behavior for free elements in the block.
                Works on 32-bit systems since we use indexing here instead of pointers.
    **/
    class xfreelist_t::xitem_t
    {
    public:
        xfreelist_t::xitem_t* getNext(xfreelist_t const* info) { return info->ptr_of(mIndex); }
        void                  setNext(xfreelist_t const* info, xfreelist_t::xitem_t* next) { mIndex = info->idx_of(next); }
        void*                 getObject() { return (void*)this; }

    private:
        u32 mIndex;
    };

}; // namespace xcore
