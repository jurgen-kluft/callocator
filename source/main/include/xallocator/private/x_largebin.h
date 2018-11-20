//==============================================================================
//  x_largebin.h
//==============================================================================
#ifndef __X_ALLOCATOR_LARGE_BIN32_H__
#define __X_ALLOCATOR_LARGE_BIN32_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase/x_tree.h"
#include "xbase/x_idx_allocator.h"

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	namespace xexternal32
	{
		struct xlnode;

		// An allocator that manages 'external' memory with book keeping data outside of that memory.
		// Every allocation and every free chunk will occupy 1 28 bytes structure.
		// Maximum number of used/free-chunks (nodes) is 2 * 1024 * 1024 * 1024.
		// Maximum size of memory that can be managed is 8 GB
		// Minimum alignment is 4
		// The 'size_alignment' and 'address_alignment should be smartly initialized since:
		//  - you may increase the amount of wasted memory (size alignment and/or address alignment to large)
		//  - you may decrease the performance (size alignment to small)
		struct xlargebin
		{
			xlargebin();

			//@note: 'node_allocator' is used to allocate fixed size (16/32 bytes) structures
			void				init(void* mem_begin, u32 mem_size, u32 size_alignment, u32 address_alignment, xalloc* node_allocator);
			void				release();

			void*				allocate(u32 size, u32 alignment);
			void				deallocate(void* ptr);

		private:
			xalloc * mNodeAllocator;				// 
			void*				mBaseAddress;				// Base address of the memory we are managing
			xtree				mRootSizeTree;				// First node of our internal tree, key=size
			xtree				mRootAddrTree;				// First node of our internal tree, key=address
			xlnode*				mNodeListHead;
			u32					mSizeAlignment;
			u32					mAddressAlignment;
		};


	}
};

#endif	/// __X_ALLOCATOR_LARGE_BIN32_H__

