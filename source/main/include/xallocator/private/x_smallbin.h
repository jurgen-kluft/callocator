#ifndef __X_ALLOCATOR_SMALL_EXT_H__
#define __X_ALLOCATOR_SMALL_EXT_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	// Forward declares
	class xalloc;

	namespace xexternal
	{
		// An external memory allocator with book keeping data outside of that memory.
		// This allocator is suitable for cases where chunk_size, the allocation size,
		// is fixed, power-of-2 and small like 8/16/32/../4096.
		// The overhead of every allocation when approaching 'full' is 2 bits.
		struct xsmallbin
		{
			void			init		(void* base_address, u32 bin_size, u32 alloc_size, xalloc* alloc);
			void			release		(xalloc* alloc);
		
			void*			allocate	();
			void			deallocate	(void* ptr);
		
		private:
			void*			mBaseAddress;				// Base address of the memory we are managing
			xbitlist		mBitList;					// Our hierarchical bitmap
			u32				mBinSize;					// 65536 is a good size
			u32				mAllocSize;					// 8/12/16/20/24/28/32/../4096
		};
	}
};

#endif	/// __X_ALLOCATOR_SMALL_EXT_H__

