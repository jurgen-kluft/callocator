//==============================================================================
//  x_allocator_small_ext.h
//==============================================================================
#ifndef __X_ALLOCATOR_SMALL_EXT_H__
#define __X_ALLOCATOR_SMALL_EXT_H__
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
	class x_iallocator;

	namespace xexternal_memory
	{
		struct xsnode;	

		// An external memory allocator with book keeping data outside of that memory.
		// This allocator is suitable for cases where chunk_size, the allocation size,
		// is fixed, power-of-2 and small like 8/16/32/../4096.
		// It will not grow or shrink and allocates 16 byte sized structures from the
		// 'node_allocator' for book-keeping purposes.
		// The overhead of every allocation when approaching 'full' is ~0.2 bytes.
		struct xsmall_allocator
		{
			void			init		(void* base_address, u32 bin_size, u16 chunk_size);
			void			release		(x_iallocator* node_allocator);
		
			//@note: 'node_allocator' is used to allocate fixed size (16 bytes) structures
			void*			allocate	(u32 size, u32 alignment, x_iallocator* node_allocator);
			void			deallocate	(void* ptr);
		
		private:
			void*			mBaseAddress;				// Base address of the memory we are managing
			xsnode*			mNode;						// First node of our internal tree
			u32				mBinSize;					// 65536 is a good size
			u16				mChunkSize;					// 8/16/32/64/128/256/512/1024/2048/4096
			u16				mLevels;					// Number of levels
		};
	}
};

#endif	/// __X_ALLOCATOR_SMALL_EXT_H__

