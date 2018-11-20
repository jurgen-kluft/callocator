//==============================================================================
//  x_allocator.h
//==============================================================================
#ifndef __X_ALLOCATOR_H__
#define __X_ALLOCATOR_H__
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

	/// Heap allocator (dlmalloc allocator)
	extern xalloc*	gCreateHeapAllocator(void* mem_begin, u32 mem_size);
};

#endif	/// __X_ALLOCATOR_H__

