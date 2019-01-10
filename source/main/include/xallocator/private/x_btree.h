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

		class allocator
		{
		public:
			virtual void*	allocate(index& index) = 0;
			virtual void	deallocate(index index) = 0;
			virtual void*	idx2ptr(index index) const = 0;
			virtual index	ptr2idx(void* ptr) const = 0;
		};


		// Note: sizeof(BLeaf) == sizeof(BNode) !

		struct leaf
		{
			u32			m_value;
			index		m_prev;
			index		m_next;
			u32			m_flags;
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

		bool			add(u32 value, btree::index& leaf_index);
		bool			rem(u32 value);

		bool			find(u32 value, btree::index& leaf_index) const;

		btree::node		m_root;
		btree::indexer*	m_idxr;
		btree::alloc*	m_node_allocator;
		btree::alloc*	m_leaf_allocator;
	};

};


#endif	/// __X_BTREE_H__

