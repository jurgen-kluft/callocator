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
	class x_iallocator;
	class x_iidx_allocator;

	/// Free list allocator
	extern x_iallocator*		gCreateFreeListAllocator(x_iallocator* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements);
	extern x_iallocator*		gCreateFreeListAllocator(x_iallocator* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inMaxNumElements);

	extern x_iidx_allocator*	gCreateFreeListIdxAllocator(x_iallocator* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements);
	extern x_iidx_allocator*	gCreateFreeListIdxAllocator(x_iallocator* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inMaxNumElements);
};


#endif	/// __X_ALLOCATOR_FREELIST_H__

