#ifndef __X_BTREE_H__
#define __X_BTREE_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	namespace btrie
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

		class indexer
		{
			u8*			m_masks;
			u8*			m_shifts;
			s32			m_levels;
		public:
			void		initialize(s32 max_levels, u8* masks, u8* shifts)
			{
				m_levels = max_levels;
				m_masks = masks;
				m_shifts = shifts;
			}

			s32			max_levels() const { return m_levels; }
			s32			get_index(u64 value, s32 level) const
			{
				u64 const msk = (u64)m_masks[level];
				s32 const shr = (s32)m_shifts[level];
				return (value >> shr) & msk;
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
	struct btree_t
	{
		void			init(btree::indexer* indexer, btree::allocator* node_allocator, btree::allocator* leaf_allocator);

		bool			add(u64 value, btree::index& leaf_index);
		bool			rem(u64 value);

		bool			find(u64 value, btree::index& leaf_index) const;

		btree::node		m_root;
		btree::indexer*	m_idxr;
		btree::alloc*	m_node_allocator;
		btree::alloc*	m_leaf_allocator;
	};

};


#endif	/// __X_BTREE_H__

