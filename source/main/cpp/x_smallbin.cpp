#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_allocator.h"
#include "xbase/x_integer.h"

#include "xallocator/private/x_smallbin.h"

namespace xcore
{
	namespace xexternal
	{
		// Page Size Bin that manages a fixed number of slots of fixed size allocations with
		// external book-keeping.

		void		xsmallbin::init(void* base_address, u32 bin_size, u32 alloc_size, xalloc* alloc)
		{
			mBaseAddress = base_address;
			mBinSize     = bin_size;
			mAllocSize   = alloc_size;

			xheap heap(alloc);
			u32 count = mBinSize / mAllocSize;
			mBitList.init(heap, count, false, false);
		}

		void		xsmallbin::release(xalloc* allocator)
		{
			xheap heap(allocator);
			mBitList.release(heap);
		}

		void*		xsmallbin::allocate()
		{
			u32 i;
			if (mBitList.find(i))
			{
				mBitList.set(i);
				void* ptr = (void*)((uptr)mBaseAddress + (i*mAllocSize));
				return ptr;
			}
			return nullptr;
		}

		void		xsmallbin::deallocate(void* ptr)
		{
			u32 const i = ((uptr)ptr - (uptr)mBaseAddress) / (u32)mAllocSize;
			ASSERT(mBitList.is_set(i));
			mBitList.clr(i);
		}
	}
}