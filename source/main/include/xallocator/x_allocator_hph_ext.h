//==============================================================================
//  x_hpeha_allocator.h
//==============================================================================
#ifndef __X_HPEHA_ALLOCATOR_H__
#define __X_HPEHA_ALLOCATOR_H__
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

	struct x_hpeha_params
	{
		x_hpeha_params(const char* allocator_name)
			: mAllocatorName(allocator_name) 
			, mPageSize(65536)
			, mSmallBinCnt(6)
			, mSmallBinSizes(NULL)
			, mLargeBinMinimumSize(1024)
			, mLargeBinMinimumAlignment(512)
		{

		}

		const char*		allocator_name() const;
		u32				page_size() const;

		u16				small_bin_cnt() const;
		u16*			small_bin_sizes() const;

		u32				large_bin_minimum_size() const;
		u32				large_bin_minimum_alignment() const;

	private:
		const char*		mAllocatorName;
		u32				mPageSize;

		u16				mSmallBinCnt;
		u16*			mSmallBinSizes;

		u32				mLargeBinMinimumSize;
		u32				mLargeBinMinimumAlignment;
	};

	// WORK IN PROGRESS, NOT FINISHED!
#if 0
	/// High performance external heap allocator, manages external memory like Texture Memory, Sound Memory
	/// by putting bookkeeping data in separate ('main') memory.
	extern x_iallocator*		gCreateHpehaAllocator(x_iallocator *allocator, const char* memory_name, u32 page_size, u16 num_small_bins, u16* small_bin_sizes, u32 large_bin_minimum_size, u32 large_bin_minimum_alignment);
#endif
};

#endif	/// __X_HPEHA_ALLOCATOR_H__

