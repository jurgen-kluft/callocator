#ifndef __X_BTREE_H__
#define __X_BTREE_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	namespace btree
	{	
		struct index
		{
			enum { Null = 0, Type_Mask = 0x8000000, Type_Leaf = 0x0, Type_Node = 0x80000000 };

			inline		index() : m_index(Null) { }

			void		reset() { m_index = Null; }

			bool		is_null() const   { return m_index == Null; }
			bool		is_node() const   { return m_index & Type_Mask == Type_Node; }
			bool		is_leaf() const   { return m_index & Type_Mask == Type_Leaf; }

			void		set_node(u32 index)	{ m_index = index | Type_Node; }
			void		set_leaf(u32 index)	{ m_index = index | Type_Leaf; }

			u32			get_node() const	{ return m_index & ~Type_Mask; }
			u32			get_leaf() const	{ return m_index & ~Type_Mask; }

			u32			m_index;
		};

		class alloc
		{
		public:
			virtual void*	allocate(u32& index) = 0;
			virtual void	deallocate(u32 index) = 0;
			virtual void*	idx2ptr(u32 index) const = 0;
			virtual u32		ptr2idx(void* ptr) const = 0;
			virtual void	release() = 0;
		};
		alloc*		gCreateVMemBasedAllocator(xalloc* a, xvirtual_memory* vmem, u32 alloc_size, u64 addr_range, u32 page_size);

		// alloc creation examples:
		// gCreateVMemBasedAllocator(a, vmem, sizeof(myleaf) (32?), 1*xGB, 64*xKB);
		// Maximum number of myleaf allocations = (2048 - 1) * 16384 = 33.538.048 = ~32 Million nodes

		// Note: sizeof(BLeaf) == sizeof(BNode) !

		struct leaf
		{
			u64			m_value;
		};

		struct node
		{
			bool		is_empty() const 
			{
				return m_nodes[0].is_null() && m_nodes[1].is_null() && m_nodes[2].is_null() && m_nodes[3].is_null();
			}

			index		m_nodes[4];
		};

		// e.g.:
		// 'value' spans the lower 26 bits
		// masks  = { 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3 };
		// shifts = { 24,   22,  20,  18,  16,  14,  12,  10,   8,   6,   4,   2,   0 };

		// 65536 number of items
		//  16 K nodes
		//   4 K
		//   1 K
		// 256
		//  64
		//	16
		//   4
		// 21*1024 + 256 + 64 + 16 + 4 + 1 = 21845 nodes

		// Note: Make sure you keep the mask/shift in order otherwise you will break the
		//       presumption that all values are ordered.
		//       Also all masks should connect and have no bit gaps, you may skip a range of
		//       upper bits as long as you are sure that they do not change for the complete
		//       set of values.
		// Example:
		// Let's say you have an address range of 32 GB with a alignment of 1024, this means
		// that you can mask these bits: 0x#######7FFFFFC00. Since a virtual memory address
		// can have bits set in ####### part you know that this doesn't matter.
		
		// -------------- 1st brute-force approach to compute the mask:
		// uptr base_address = 0x07F0500800000000;
		// u32 const num_pages = 8192;
		// u32 const page_size = 65536;
		// u32 const granularity = 1024;
		// uptr end_address = base_address + num_pages * page_size;
		// uptr address = base_address;
		// uptr mask = 0;
		// uptr fixed = 0xffffffffffffffff;
		// while (address < end_address)
		// {
		//     mask = mask | address;
		//     fixed = fixed & mask;
		//     address += alignment;
		// }
		// 
		// -------------- 2nd approach to getting the mask
		// 
		// uptr address_range = page_size * num_pages;
		// u64 mask = address_range;
		// mask = mask | (mask >> 1);
		// mask = mask | (mask >> 2);
		// mask = mask | (mask >> 4);
		// mask = mask | (mask >> 8);
		// mask = mask | (mask >> 16);
		// mask = mask | (mask >> 32);
		// mask = mask & ~(granularity - 1);
		// 
		// u32 maskbitcnt = xcountBits(mask);
		// u32 numlevels = (maskbitcnt + 1) / 2;
		// if ((maskbitcnt & 1) == 1)
		// {
		//     // Which way should we extend the mask to make the count 'even'
		//     if ((mask & (1<<63)) == 0)
		//     {
		//         mask = mask | (mask << 1);
		//     }
		//     else
		//     {
		//         mask = mask | (mask >> 1);
		//     }		
		//     maskbitcnt += 1;
		// }

		class indexer
		{
			u8*			m_masks;
			u8*			m_shifts;
			u64			m_mask;
			s8			m_levels;
		public:
			void		initialize(s32 max_levels, u64 mask, u8* masks, u8* shifts)
			{
				m_masks = masks;
				m_shifts = shifts;
				m_mask = mask;
				m_levels = max_levels;
			}

			s8			max_levels() const { return m_levels; }

			// The indexer knows which bits are being 'indexed' so it can
			// tell us if two values are equal from its perspective.
			bool		equal(u64 lhs, u64 rhs) const
			{
				return (lhs & m_mask) == (rhs & m_mask);
			}
			
			s32			compare(u64 lhs, u64 rhs) const
			{
				lhs = lhs & m_mask;
				rhs = rhs & m_mask;
				if (lhs == rhs)
					return 0;
				return (lhs < rhs) ? -1 : 1;
			}

			s8			get_index(s32 level) const
			{
				return (s8)m_masks[level];
			}

			s8			get_index(u64 value, s32 level) const
			{
				s8 const msk = (s8)m_masks[level];
				s32 const shr = (s32)m_shifts[level];
				return (s8)(value >> shr) & msk;
			}
		};
	}

	// A btree_t is a BST that is unbalanced and where branches grow/shrink when adding
	// or removing items. This particular implementation uses indices instead of pointers
	// and thus uses the same amount of memory on a 32-bit system compared to a 64-bit
	// system. It is possible to make an implementation where the indices are 16-bit when
	// the number of leafs are less then 16384 (we already use the highest bit to identify
	// between node and leaf plus a tree uses (N*2 - 1) nodes + leafs).
	// 
	// We use an `indexer` here which is responsible for computing the index at every level
	// of the tree for a given 'value'.
	//
	// Since this data-structure is 'sorted' we also provide a lower and upper bound find
	// function.
	struct btree_t
	{
		void			init(btree::indexer* indexer, btree::allocator* node_allocator, btree::allocator* leaf_allocator);

		bool			add(u64 value, btree::index& leaf_index);
		bool			rem(u64 value);

		bool			find(u64 value, btree::index& leaf_index) const;
		bool			lower_bound(u64 value, btree::index& leaf_index) const;
		bool			upper_bound(u64 value, btree::index& leaf_index) const;

		btree::node		m_root;
		btree::indexer*	m_idxr;
		btree::alloc*	m_node_allocator;
		btree::alloc*	m_leaf_allocator;
	};

};


#endif	/// __X_BTREE_H__

