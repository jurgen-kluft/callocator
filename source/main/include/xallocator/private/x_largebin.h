//==============================================================================
//  x_largebin.h
//==============================================================================
#ifndef __X_ALLOCATOR_LARGE_BIN_H__
#define __X_ALLOCATOR_LARGE_BIN_H__
#include "xbase\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase\private\x_rbtree15.h"
#include "xbase\x_idx_allocator.h"

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	namespace xexternal
	{
		// An allocator that manages 'external' memory with book keeping data outside of that memory.
		// Every allocation and every free chunk will occupy 1 16 bytes structure.
		// Maximum number of used/free-chunks (nodes) is 32768.
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
			u16					mRootSizeTree;				// First node of our internal tree, key=size
			u16					mRootAddrTree;				// First node of our internal tree, key=address
			u16					mNodeListHead;
			u32					mSizeAlignment;
			u32					mAddressAlignment;
		};

		static inline void*		advance_ptr(void* ptr, u32 size)		{ return (void*)((xbyte*)ptr + size); }
		static inline void*		align_ptr(void* ptr, u32 alignment)		{ return (void*)(((X_PTR_SIZED_INT)ptr + (alignment-1)) & ~(alignment-1)); }
		static inline void*		mark_ptr_0(void* ptr, u8 bit)			{ return (void*)((X_PTR_SIZED_INT)ptr & ~((X_PTR_SIZED_INT)1<<bit)); }
		static inline void*		mark_ptr_1(void* ptr, u8 bit)			{ return (void*)((X_PTR_SIZED_INT)ptr | ((X_PTR_SIZED_INT)1<<bit)); }
		static inline bool		get_ptr_mark(void* ptr, u8 bit)			{ u32 const field = (1<<bit); return ((X_PTR_SIZED_INT)ptr&field) != 0; }
		static inline void*		get_ptr(void* ptr, u8 used_bits)		{ return (void*)((X_PTR_SIZED_INT)ptr & ~(((X_PTR_SIZED_INT)1<<used_bits)-1)); }
		static u32				diff_ptr(void* ptr, void* next_ptr)		{ return (u32)((xbyte*)next_ptr - (xbyte*)ptr); }

		struct xlnode : public xrbnode15
		{
			enum EState { STATE_USED=1, STATE_FREE=0, USED_BIT=0, USED_BITS=1 };

			void*			ptr;				// (4) pointer to external memory
			u16				next;				// (2) linear list ordered by physical address
			u16				prev;				// (2)  
		};

		static inline xlnode*	allocate_node(x_iidx_allocator* allocator, u16& outNodeIdx)
		{
			void* nodePtr;
			outNodeIdx = allocator->iallocate(nodePtr);
			xlnode* node = (xlnode*)nodePtr;
			return node;
		}

		static inline void		init_node(xlnode* node, void* ptr, xlnode::EState state, u16 next, u16 prev, u16 nill)
		{
			node->ptr  = (state==xlnode::STATE_USED) ? mark_ptr_1(ptr, xlnode::USED_BIT) : mark_ptr_0(ptr, xlnode::USED_BIT);
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

		static inline u32		get_size(xlnode* nodePtr, x_iidx_allocator* a)
		{
			xlnode* nextNodePtr = (xlnode*)a->to_ptr(nodePtr->next);
			u32 const nodeSize  = diff_ptr(get_ptr(nextNodePtr->ptr, xlnode::USED_BITS), get_ptr(nodePtr->ptr, xlnode::USED_BITS));
			return nodeSize;
		}
	}
};

#endif	/// __X_ALLOCATOR_LARGE_BIN_H__

