#ifndef __X_IDX_ALLOCATOR_H__
#define __X_IDX_ALLOCATOR_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	/// Forward declares
	class xalloc;
	class xfsadexed;

	extern xfsadexed*	gCreateArrayIdxAllocator(xalloc* allocator, xalloc* object_array_allocator, u32 size_of_object, u32 object_alignment, u32 size);
	extern xfsadexed*	gCreateArrayIdxAllocator(xalloc* allocator, void* object_array, u32 size_of_object, u32 object_alignment, u32 size);
	extern xfsadexed*	gCreatePoolIdxAllocator(xalloc* allocator, xalloc* block_array_allocator, xalloc* object_array_allocator, u32 size_of_object, u32 object_alignment=4, u32 num_objects_per_block=8, u32 num_initial_blocks=1, u32 num_grow_blocks=1, u32 num_shrink_blocks=1);
	extern xfsadexed*	gCreatePoolIdxAllocator(xalloc* allocator, u32 size_of_object, u32 object_alignment=4, u32 num_objects_per_block=16, u32 num_initial_blocks=1, u32 num_grow_blocks=1, u32 num_shrink_blocks=1);

};

#endif	///< __X_IDX_ALLOCATOR_H__
