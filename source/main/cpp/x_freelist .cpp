#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_memory_std.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"

#include "xallocator/private/x_freelist.h"

namespace xcore
{
	xfreelist_t::xfreelist_t()
		: mElemSize(0)
		, mElemAlignment(0)
		, mSize(0)
		, mElementArray(0)
	{
	}

	void                xfreelist_t::init(xbyte* array, u32 array_size, u32 elem_size, u32 elem_alignment)
	{
		// Check parameters
		ASSERT(mElemSize >= sizeof(void*));

		mAllocator = nullptr;
		mElementArray = array;

		// Take over the parameters
		mElemSize = elem_size;
		mElemAlignment = elem_alignment;
		mSize = 0;

		// Clamp/Guard parameters
		mElemAlignment     = mElemAlignment == 0 ? X_ALIGNMENT_DEFAULT : mElemAlignment;
		mElemAlignment     = xalignUp(mElemAlignment, sizeof(void*));                   // Align element alignment to the size of a pointer
		mElemSize          = xalignUp(mElemSize, sizeof(void*));                        // Align element size to the size of a pointer
		mElemSize          = xalignUp(mElemSize, mElemAlignment);                       // Align element size to a multiple of element alignment
		mSize              = (array_size / mElemSize);
	}

	void                xfreelist_t::alloc(x_iallocator* allocator, u32 elem_size, u32 elem_alignment, s32 count)
	{
		// Check parameters
		ASSERT(mElemSize >= sizeof(void*));
		ASSERT(count != 0);

		mAllocator = allocator;
		mSize = count;

		// Take over the parameters
		mElemSize = elem_size;
		mElemAlignment = elem_alignment;

		// Clamp/Guard parameters
		mElemAlignment = mElemAlignment == 0 ? X_ALIGNMENT_DEFAULT : mElemAlignment;
		mElemAlignment = xalignUp(mElemAlignment, sizeof(void*));                       // Align element alignment to the size of a pointer
		mElemSize = xalignUp(mElemSize, sizeof(void*));                                 // Align element size to the size of a pointer
		mElemSize = xalignUp(mElemSize, mElemAlignment);                                // Align element size to a multiple of element alignment

		// Initialize the element array
		mElementArray = (xbyte*)allocator->allocate(mElemSize * mSize, mElemAlignment);
	}

	void                xfreelist_t::release()
	{
		if (mAllocator != nullptr)
		{
			mAllocator->deallocate(mElementArray);
			mElementArray = nullptr;
			mAllocator = nullptr;
		}
	}


	/**
	@brief      Fixed size type, element
	@desc       It implements linked list behavior for free elements in the block.
	            Works on 64-bit systems since we use indexing here instead of pointers.
	**/
	class xelement
	{
	public:
		xelement*           getNext(xfreelist_t const* info)                        { return info->ptr_of(mIndex); }
		void                setNext(xfreelist_t const* info, xelement* next)        { mIndex = info->idx_of(next); }
		void*               getObject()                                             { return (void*)&mIndex; }
	private:
		u32                 mIndex;
	};


};
