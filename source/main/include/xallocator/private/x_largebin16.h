//==============================================================================
//  x_largebin16.h
//==============================================================================
#ifndef __X_ALLOCATOR_LARGE_BIN16_H__
#define __X_ALLOCATOR_LARGE_BIN16_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase/private/x_rbtree15.h"
#include "xbase/x_idx_allocator.h"

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	namespace xexternal16
	{
		// An allocator that manages 'external' memory with book keeping data outside of that memory.
		// Every allocation and every free chunk will occupy 1 28 bytes structure.
		// Maximum number of used/free-chunks (nodes) is 65536.
		// Maximum size of memory that can be managed is 2 GB
		// The 'size_alignment' and 'address_alignment should be smartly initialized since:
		//  - you may increase the amount of wasted memory (size alignment and/or address alignment to large)
		//  - you may decrease the performance (size alignment to small)
		struct xlargebin
		{
			//@note: 'node_allocator' is used to allocate fixed size (16/32 bytes) structures
			void				init(void* mem_begin, u32 mem_size, u32 size_alignment, u32 address_alignment, x_iidx_allocator* node_allocator);
			void				release();

			void*				allocate(u32 size, u32 alignment);
			void				deallocate(void* ptr);

		private:
			x_iidx_allocator*	mNodeAllocator;				// Max. 65535 nodes (xrbnode15)
			void*				mBaseAddress;				// Base address of the memory we are managing
			u16					mRootSizeTree;				// First node of our internal tree, key=size
			u16					mRootAddrTree;				// First node of our internal tree, key=address
			u16					mNodeListHead;
			u32					mSizeAlignment;
			u32					mAddressAlignment;
		};

		typedef	u32				memptr;

		static inline memptr	advance_ptr(memptr ptr, u32 size) { return (memptr)(ptr + size); }
		static inline memptr	align_ptr(memptr ptr, u32 alignment) { return (memptr)((ptr + (alignment - 1)) & ~((memptr)alignment - 1)); }
		static xsize_t			diff_ptr(memptr ptr, memptr next_ptr) { return (xsize_t)(next_ptr - ptr); }

		struct xlnode : public xrbnode15
		{
			enum EState { STATE_USED=4, STATE_FREE=0 };

			void			set_state(EState state)		{ flags = (flags & ~STATE_USED) | state; }

			void			set_free()					{ flags = (flags & ~STATE_USED); }
			void			set_used()					{ flags = (flags & ~STATE_USED) | STATE_USED; }

			bool			is_free() const				{ return (flags & STATE_USED) == STATE_FREE; }
			bool			is_used() const				{ return (flags & STATE_USED) == STATE_USED; }

			u16				next;				// (2) linear list ordered by physical address
			u16				prev;				// (2)  
			memptr			ptr;				// (4) pointer/offset in external memory
		};

		static inline xlnode*	allocate_node(x_iidx_allocator* allocator, u16& outNodeIdx)
		{
			void* nodePtr;
			outNodeIdx = allocator->iallocate(nodePtr);
			xlnode* node = (xlnode*)nodePtr;
			return node;
		}

		static inline void		init_node(xlnode* node, memptr offset, xlnode::EState state, u16 next, u16 prev, u16 nill)
		{
			node->clear(nill);
			node->set_state(state);
			node->next = next;
			node->prev = prev;
			node->ptr = offset;
		}

		static inline void		deallocate_node(xlnode* nodePtr, x_iidx_allocator* allocator)
		{
			allocator->deallocate(nodePtr);
		}

		static inline xsize_t	get_size(xlnode* nodePtr, x_iidx_allocator* a)
		{
			xlnode* nextNodePtr = (xlnode*)a->to_ptr(nodePtr->next);
			xsize_t const nodeSize = (nextNodePtr->ptr) - (nodePtr->ptr);
			return nodeSize;
		}
	}
};

#endif	/// __X_ALLOCATOR_LARGE_BIN16_H__

