//==============================================================================
//  x_allocator_freelist.h
//==============================================================================
#ifndef __X_ALLOCATOR_FREELIST_H__
#define __X_ALLOCATOR_FREELIST_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	/// Forward declares
	class xalloc;
	class x_iidx_allocator;

	/// Free list allocator
	extern xalloc*		gCreateFreeListAllocator(xalloc* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements);
	extern xalloc*		gCreateFreeListAllocator(xalloc* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inMaxNumElements);

	extern x_iidx_allocator*	gCreateFreeListIdxAllocator(xalloc* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements);
	extern x_iidx_allocator*	gCreateFreeListIdxAllocator(xalloc* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inMaxNumElements);
};


#endif	/// __X_ALLOCATOR_FREELIST_H__

