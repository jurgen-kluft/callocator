#ifndef __C_ALLOCATOR_H__
#define __C_ALLOCATOR_H__
#include "ccore/c_target.h"
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

#endif	/// __C_ALLOCATOR_H__

