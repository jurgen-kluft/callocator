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
	extern x_iallocator*		gCreateDlAllocator(void* mem, u32 memsize);
	extern x_iallocator*		gCreateTlsfAllocator(void* mem, u32 memsize);

	/// Allocators that manage external memory like video or sound card memory are
	/// required to implement the following interface.
	class x_imemory
	{
	public:
		virtual const char*		name() const = 0;								///< The name of the external memory (e.g. 'PS3 Graphics Memory')
		virtual u32				mem_range(void*& low, void*& high) const = 0;	///< The external memory address starts at @low and ends at @high. Returns the size (@high - @low)			
		virtual void			mem_copy(void* dst, void const* src, u32 size) const = 0;
		virtual u32				page_size() const = 0;							///< The page size of the external memory
		virtual u32				align_size() const = 0;							///< The minimum alignment of allocated memory
	};

	/// External block allocator, manages external memory like Texture Memory, Sound Memory
	/// by putting bookkeeping data in separate memory.
	extern x_iallocator*		gCreateExtBlockAllocator(x_iallocator *allocator, x_imemory* memory);
	
	/// The forward ring allocator is a specialized allocator. You can use it when you are allocating different size blocks that
	/// all have a life-time that doesn't differ much. This allocator is very fast in allocation O(1), deallocations effectively
	/// also shows O(1) behavior but due to its coalesce mechanism can sometimes take a bit more time. If you mostly allocate
	/// and deallocate in a non-random order than deallocation is surely O(1).
	extern x_iallocator*		gCreateForwardRingAllocator(x_iallocator* allocator, u32 memsize);

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

