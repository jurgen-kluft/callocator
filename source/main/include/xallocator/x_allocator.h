//==============================================================================
//  x_allocator.h
//==============================================================================
#ifndef __X_ALLOCATOR_H__
#define __X_ALLOCATOR_H__
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

	/// The custom allocators, 'Doug Lea malloc' and 'Two-Level Segregate Fit' allocator
	extern x_iallocator*		gCreateDlAllocator(void* mem, s32 memsize);
	extern x_iallocator*		gCreateTlsfAllocator(void* mem, s32 memsize);

	/// External block allocator, manages external memory like Texture Memory, Sound Memory
	/// by putting bookkeeping data in separate memory.
	typedef void (*xextmem_copy)(void const* src, u32 src_size, void* dst, u32 dst_size);
	extern x_iallocator*		gCreateEbAllocator(void* mem, s32 memsize, x_iallocator *allocator, xextmem_copy extmem_copy);
	
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

		void	set_elem_size(u32 size)									{ mElemSize = size; }
		void	set_elem_alignment(u32 alignment)						{ mElemAlignment = alignment; }
		void	set_block_size(u32 num_elements)						{ mBlockElemCount = num_elements; }
		void	set_block_initial_count(u32 initial_num_blocks)			{ mBlockInitialCount = initial_num_blocks; }
		void	set_block_growth_count(u32 growth_num_blocks)			{ mBlockGrowthCount = growth_num_blocks; }
		void	set_block_max_count(u32 max_num_blocks)					{ mBlockMaxCount = max_num_blocks; }

		u32		get_elem_size() const									{ return mElemSize; }
		u32		get_elem_alignment() const								{ return mElemAlignment; }
		u32		get_block_size() const									{ return mBlockElemCount; }
		u32		get_block_initial_count() const							{ return mBlockInitialCount; }
		u32		get_block_growth_count() const							{ return mBlockGrowthCount; }
		u32		get_block_max_count() const								{ return mBlockMaxCount; }

	private:
		u32		mElemSize;
		u32		mElemAlignment;
		u32		mBlockElemCount;
		u32		mBlockInitialCount;
		u32		mBlockGrowthCount;
		u32		mBlockMaxCount;
	};

	extern x_iallocator*		gCreatePoolAllocator(x_iallocator* main_allocator, xpool_params const& params);
	


	//==============================================================================
	// END xCore namespace
	//==============================================================================
};

#endif	/// __X_ALLOCATOR_H__

