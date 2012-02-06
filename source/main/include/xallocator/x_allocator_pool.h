//==============================================================================
//  x_pool_allocator.h
//==============================================================================
#ifndef __X_POOL_ALLOCATOR_H__
#define __X_POOL_ALLOCATOR_H__
#include "xbase\x_target.h"
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

	/// The pool (fixed sized type) allocator
	struct xpool_params
	{
		xpool_params()
			: mElemSize(4)												///< The size of the element
			, mElemAlignment(4)											///< Alignment of the element
			, mBlockElemCount(16)										///< Number of elements per block
			, mBlockInitialCount(1)										///< Number of initial blocks
			, mBlockGrowthCount(1)										///< How many blocks to add to fullfill and alloc when all blocks are full
			, mBlockMaxCount(4096) { }									///< Maximum number of blocks

		void	set_elem_size(u32 size);
		void	set_elem_alignment(u32 alignment);
		void	set_block_size(u32 num_elements);
		void	set_block_initial_count(u32 initial_num_blocks);
		void	set_block_growth_count(u32 growth_num_blocks);
		void	set_block_max_count(u32 max_num_blocks);

		u32		get_elem_size() const;
		u32		get_elem_alignment() const;
		u32		get_block_size() const;
		u32		get_block_initial_count() const;
		u32		get_block_growth_count() const;
		u32		get_block_max_count() const;

	private:
		u32		mElemSize;
		u32		mElemAlignment;
		u32		mBlockElemCount;
		u32		mBlockInitialCount;
		u32		mBlockGrowthCount;
		u32		mBlockMaxCount;
	};

	extern x_iallocator*		gCreatePoolAllocator(x_iallocator* main_allocator, xpool_params const& params);
	
};

#endif	/// __X_POOL_ALLOCATOR_H__

