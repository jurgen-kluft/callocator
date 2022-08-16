#ifndef __X_ALLOCATOR_FREELIST_H__
#define __X_ALLOCATOR_FREELIST_H__
#include "cbase/c_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace ncore
{
	/// Forward declares
	class alloc_t;
    class fsadexed_t;

	/// Free list allocator
    extern fsadexed_t* gCreateFreeListAllocator(alloc_t* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements);
    extern fsadexed_t* gCreateFreeListAllocator(alloc_t* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inMaxNumElements);

	extern fsadexed_t* gCreateFreeListIdxAllocator(alloc_t* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements);
    extern fsadexed_t* gCreateFreeListIdxAllocator(alloc_t* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inMaxNumElements);
};


#endif	/// __X_ALLOCATOR_FREELIST_H__

