//==============================================================================
//  x_allocator_large_ext.h
//==============================================================================
#ifndef __X_ALLOCATOR_LARGE_EXT_H__
#define __X_ALLOCATOR_LARGE_EXT_H__
#include "xbase\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	// Forward declares
	class x_iidx_allocator;

	namespace xexternal_memory
	{
		// An allocator that manages 'external' memory with book keeping data outside of that memory.
		// Every allocation and every free chunk will occupy 1 16 bytes structure.
		// Maximum number of used/free-chunks (nodes) is 32768.
		// The 'size_alignment' and 'address_alignment should be smartly initialized since:
		//  - you may increase the amount of wasted memory (size alignment and/or address alignment to large)
		//  - you may decrease the performance (size alignment to small)
		struct xlarge_allocator
		{
			//@note: 'node_allocator' is used to allocate fixed size (16 bytes) structures
			void				init		(void* mem_begin, u32 mem_size, u32 size_alignment, u32 address_alignment, x_iidx_allocator* node_allocator);
			void				release		();
		
			void*				allocate	(u32 size, u32 alignment);
			void				deallocate	(void* ptr);
		
		private:
			x_iidx_allocator*	mNodeAllocator;				// Max. 32768 nodes (xrbnode15)
			void*				mBaseAddress;				// Base address of the memory we are managing
			u16					mRootSizeTree;				// First node of our internal tree, key=size
			u16					mRootAddrTree;				// First node of our internal tree, key=address
			u16					mNodeListHead;
			u32					mSizeAlignment;
			u32					mAddressAlignment;
		};
	}
};

#endif	/// __X_ALLOCATOR_LARGE_EXT_H__

