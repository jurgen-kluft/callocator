//==============================================================================
//  x_largebin.h
//==============================================================================
#ifndef __X_ALLOCATOR_LARGE_BIN_H__
#define __X_ALLOCATOR_LARGE_BIN_H__
#include "xbase\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase\private\x_rbtree31.h"
#include "xbase\x_idx_allocator.h"

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	namespace xexternal
	{
		// An allocator that manages 'external' memory with book keeping data outside of that memory.
		// Every allocation and every free chunk will occupy 1 28 bytes structure.
		// Maximum number of used/free-chunks (nodes) is 2 * 1024 * 1024 * 1024.
		// Maximum size of memory that can be managed is 2 GB
		// The 'size_alignment' and 'address_alignment should be smartly initialized since:
		//  - you may increase the amount of wasted memory (size alignment and/or address alignment to large)
		//  - you may decrease the performance (size alignment to small)
		struct xlargebin
		{
			//@note: 'node_allocator' is used to allocate fixed size (16/32 bytes) structures
			void				init		(void* mem_begin, u32 mem_size, u32 size_alignment, u32 address_alignment, x_iidx_allocator* node_allocator);
			void				release		();
		
			void*				allocate	(u32 size, u32 alignment);
			void				deallocate	(void* ptr);
		
		private:
			x_iidx_allocator*	mNodeAllocator;				// Max. 32768 nodes (xrbnode15)
			void*				mBaseAddress;				// Base address of the memory we are managing
			u32					mRootSizeTree;				// First node of our internal tree, key=size
			u32					mRootAddrTree;				// First node of our internal tree, key=address
			u32					mNodeListHead;
			u32					mSizeAlignment;
			u32					mAddressAlignment;
		};

		typedef	u32				memptr;

		static inline memptr	advance_ptr(memptr ptr, u32 size)		{ return (memptr)(ptr + size); }
		static inline memptr	align_ptr(memptr ptr, u32 alignment)	{ return (memptr)((ptr + (alignment-1)) & ~((memptr)alignment-1)); }
		static inline memptr	mark_ptr_0(memptr ptr, u8 bit)			{ return (memptr)(ptr & ~((memptr)1<<bit)); }
		static inline memptr	mark_ptr_1(memptr ptr, u8 bit)			{ return (memptr)(ptr | ((memptr)1<<bit)); }
		static inline bool		get_ptr_mark(memptr ptr, u8 bit)		{ u32 const field = (1<<bit); return ((memptr)ptr&field) != 0; }
		static inline memptr	get_ptr(memptr ptr, u8 used_bits)		{ return (memptr)(ptr & ~(((memptr)1<<used_bits)-1)); }
		static xsize_t			diff_ptr(memptr ptr, memptr next_ptr)	{ return (xsize_t)(next_ptr - ptr); }

		struct xlnode : public xrbnode31
		{
			enum EState { STATE_USED=1, STATE_FREE=0, USED_BIT=0, USED_BITS=1 };

			memptr			ptr;				// (4) pointer in external memory
			u32				next;				// (2) linear list ordered by physical address
			u32				prev;				// (2)  
		};

		static inline xlnode*	allocate_node(x_iidx_allocator* allocator, u32& outNodeIdx)
		{
			void* nodePtr;
			outNodeIdx = allocator->iallocate(nodePtr);
			xlnode* node = (xlnode*)nodePtr;
			return node;
		}

		static inline void		init_node(xlnode* node, u32 offset, xlnode::EState state, u32 next, u32 prev, u32 nill)
		{
			node->ptr = (state==xlnode::STATE_USED) ? (offset | xlnode::USED_BIT) : (offset | xlnode::USED_BIT);
			node->next = next;
			node->prev = prev;
			node->clear(nill);
		}

		static inline void		deallocate_node(xlnode* nodePtr, x_iidx_allocator* allocator)
		{
			allocator->deallocate(nodePtr);
		}

		static inline s32		cmp_range_ptr(void* low, void* high, void* p)
		{ 
			if ((xbyte*)p < (xbyte*)low)
				return -1; 
			else if ((xbyte*)p >= (xbyte*)high)
				return 1;
			return 0;
		}

		static inline xsize_t	get_size(xlnode* nodePtr, x_iidx_allocator* a)
		{
			xlnode* nextNodePtr = (xlnode*)a->to_ptr(nodePtr->next);
			xsize_t const nodeSize  = (nextNodePtr->ptr & ~xlnode::USED_BITS) - (nodePtr->ptr & ~xlnode::USED_BITS);
			return nodeSize;
		}
	}
};

#endif	/// __X_ALLOCATOR_LARGE_BIN_H__

