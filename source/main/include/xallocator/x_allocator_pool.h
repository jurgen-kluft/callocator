#ifndef __X_POOL_ALLOCATOR_H__
#define __X_POOL_ALLOCATOR_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xallocator/x_fsalloc.h"

namespace xcore
{
	/// Forward declares
	class xalloc;

	struct xfsa_params
	{
		xfsa_params()
			: mElemSize(4)												// The size of the element
			, mElemAlignment(4)											// Alignment of the element
			, mMinNumberOfBlocks(1)										// Minimum number of blocks
			, mMaxNumberOfBlocks(256)									// Maximum number of blocks
			, mBlockAllocator(nullptr)									// Allocator to obtain blocks from

		void		set_elem_size(u32 size);
		void		set_elem_alignment(u32 alignment);

		void		set_block_min_count(u32 min_num_blocks);
		void		set_block_max_count(u32 max_num_blocks);
		void		set_block_allocator(xfsalloc* block_allocator);

		u32			get_elem_size() const;
		u32			get_elem_alignment() const;
		u32			get_block_min_count() const;
		u32			get_block_max_count() const;
		xfsalloc*	get_block_allocator() const;

	private:
		u32			mElemSize;
		u32			mElemAlignment;
		u32			mMinNumberOfBlocks;		
		u32			mMaxNumberOfBlocks;
		xfsalloc*	mBlockAllocator;
	};

	extern xfsalloc*	gCreateFixedSizeAllocator(xalloc* main_allocator, xfsa_params const& params);
	
};

#endif	/// __X_POOL_ALLOCATOR_H__

