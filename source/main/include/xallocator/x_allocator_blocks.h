#ifndef __X_BLOCK_ALLOCATOR_H__
#define __X_BLOCK_ALLOCATOR_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	/// Forward declares
	class xfsalloc;

	/// The block (fixed sized) allocator
	struct xballoc_params
	{
		xballoc_params()
			: mBlockSize(65536)											///< The size of a block
			, mBlockAlignment(sizeof(void*))							///< Alignment of the block
			, mBlockMinCount(1)											///< Minimum number of blocks
			, mBlockIncCount(2)											///< How many blocks to add when more are needed
			, mBlockMaxCount(4096) { }									///< Maximum number of blocks

		void	set_block_size(u32 size);
		void	set_block_alignment(u32 alignment);
		void	set_block_min_count(u32 min_num_blocks);
		void	set_block_inc_count(u32 growth_num_blocks);
		void	set_block_max_count(u32 max_num_blocks);

		u32		get_block_size() const;
		u32		get_block_alignment() const;
		u32		get_block_min_count() const;
		u32		get_block_inc_count() const;
		u32		get_block_max_count() const;

	private:
		u32		mBlockSize;
		u32		mBlockAlignment;
		u32		mBlockMinCount;
		u32		mBlockIncCount;
		u32		mBlockMaxCount;
	};

	extern xfsalloc*	gCreateBlockAllocator(xalloc* backend, xballoc_params const& params);
};

#endif	/// __X_BLOCK_ALLOCATOR_H__

