#ifndef __X_ALLOCATOR_HEXT_H__
#define __X_ALLOCATOR_HEXT_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	/// Forward declares
	class xalloc;

	// WORK IN PROGRESS, NOT FINISHED!

	/// High performance external heap allocator, manages external memory like Texture Memory, Sound Memory
	/// by putting bookkeeping data in separate ('main') memory.
	extern xalloc*		gCreateHextAllocator(xalloc *allocator, void* mem_base, u32 mem_size, u32 min_alloc_size, u32 max_alloc_size);

};

#endif	/// __X_ALLOCATOR_HEXT_H__