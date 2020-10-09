#ifndef __X_ALLOCATOR_FREELIST_H__
#define __X_ALLOCATOR_FREELIST_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	/// Forward declares
	class xalloc;
    class xfsadexed;

	/// Free list allocator
    extern xfsadexed* gCreateFreeListAllocator(xalloc* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements);
    extern xfsadexed* gCreateFreeListAllocator(xalloc* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inMaxNumElements);

	extern xfsadexed* gCreateFreeListIdxAllocator(xalloc* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements);
    extern xfsadexed* gCreateFreeListIdxAllocator(xalloc* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inMaxNumElements);
};


#endif	/// __X_ALLOCATOR_FREELIST_H__

