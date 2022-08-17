#ifndef __X_ALLOCATOR_H__
#define __X_ALLOCATOR_H__
#include "cbase/c_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace ncore
{
	/// Forward declares
	class alloc_t;

	/// Heap allocator (dlmalloc allocator)
	extern alloc_t*	gCreateHeapAllocator(void* mem_begin, u32 mem_size);
};

#endif	/// __X_ALLOCATOR_H__

