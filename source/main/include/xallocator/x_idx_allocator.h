//==============================================================================
//  x_idx_allocator.h
//==============================================================================
#ifndef __X_IDX_ALLOCATOR_H__
#define __X_IDX_ALLOCATOR_H__
#include "xbase\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase\x_types.h"

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	/// Forward declares
	class x_iallocator;
	class x_iidx_allocator;

	extern x_iidx_allocator*	gCreateArrayIdxAllocator(x_iallocator* allocator, x_iallocator* object_array_allocator, u32 size_of_object, u32 object_alignment, u32 size);
	extern x_iidx_allocator*	gCreateArrayIdxAllocator(x_iallocator* allocator, void* object_array, u32 size_of_object, u32 object_alignment, u32 size);
	extern x_iidx_allocator*	gCreatePoolIdxAllocator(x_iallocator* allocator, x_iallocator* block_array_allocator, x_iallocator* object_array_allocator, u32 size_of_object, u32 object_alignment=4, u32 num_objects_per_block=8, u32 num_initial_blocks=1, u32 num_grow_blocks=1, u32 num_shrink_blocks=1);
	extern x_iidx_allocator*	gCreatePoolIdxAllocator(x_iallocator* allocator, u32 size_of_object, u32 object_alignment=4, u32 num_objects_per_block=16, u32 num_initial_blocks=1, u32 num_grow_blocks=1, u32 num_shrink_blocks=1);

	//==============================================================================
	// END xCore namespace
	//==============================================================================
};

#endif	/// __X_IDX_ALLOCATOR_H__
