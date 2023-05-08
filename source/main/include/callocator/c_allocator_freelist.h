#ifndef __C_ALLOCATOR_FREELIST_H__
#define __C_ALLOCATOR_FREELIST_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace ncore
{
	/// Forward declares
	class alloc_t;
	class fsadexed_t;

    fsadexed_t* gCreateFreeListIdxAllocator(alloc_t* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements);
};

#endif	/// __C_ALLOCATOR_FREELIST_H__

