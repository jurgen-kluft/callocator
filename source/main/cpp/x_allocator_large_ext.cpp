#include "xbase\x_target.h"
#include "xbase\x_debug.h"
#include "xbase\x_idx_allocator.h"
#include "xbase\x_integer.h"
#include "xbase\private\x_rbtree15.h"
#include "xallocator\private\x_allocator_large_ext.h"

namespace xcore
{
	namespace xexternal_memory
	{
		// 16 bytes
		struct xlnode : public xrbnode15
		{
			enum EState { USED=1, FREE=0 };

			void*			ptr;				// (4) pointer to external mem
			u16				next;				// (2) linear list ordered by physical address
			u16				prev;				// (2) 
		};

		static inline void*	advance_ptr(void* ptr, u32 size)		{ return (void*)((xbyte*)ptr + size); }
		static inline void*	align_ptr(void* ptr, u32 alignment)		{ return (void*)(((u32)ptr + (alignment-1)) & ~(alignment-1)); }
		static inline void*	mark_ptr_0(void* ptr, u8 bit)			{ return (void*)((u32)ptr & ~(1<<bit)); }
		static inline void*	mark_ptr_1(void* ptr, u8 bit)			{ return (void*)((u32)ptr | (1<<bit)); }
		static inline u32	get_ptr_mark(void* ptr, u8 bit)			{ u32 b = (1<<bit); return ((u32)ptr&b); }
		static inline void* get_ptr(void* ptr, u8 used_bits)		{ return (void*)((u32)ptr & ~((1<<used_bits)-1)); }

		static u32			diff_ptr(void* ptr, void* next_ptr)		{ return (u32)((xbyte*)next_ptr - (xbyte*)ptr); }
		static s32			cmp_range_ptr(void* low, void* high, void* p)
		{ 
			if ((xbyte*)p < (xbyte*)low)
				return -1; 
			else if ((xbyte*)p >= (xbyte*)high)
				return 1;
			return 0;
		}
	
		static void			insert_size(u16 root, u16 node, x_iidx_allocator* node_allocator);
		static bool			find_size(u16 root, u32 size, u32 alignment, x_iidx_allocator* a, u16& outNode, u32& outNodeSize);

		static void			insert_addr(u16 root, u16 node, x_iidx_allocator* node_allocator);
		static bool			find_addr(u16 root, void* ptr, x_iidx_allocator* a, u16& outNode);

		static void			remove_from_list(u16 node, x_iidx_allocator* a);
		static void			remove_from_tree(u16 root, u16 node, x_iidx_allocator* a);

		static xlnode*		allocate_node(x_iidx_allocator* allocator, u16& outNodeIdx)
		{
			void* nodePtr;
			outNodeIdx = allocator->iallocate(nodePtr);
			xlnode* node = (xlnode*)nodePtr;
			return node;
		}

		static void			init_node(xlnode* node, void* ptr, xlnode::EState state, u16 next, u16 prev, u16 nill)
		{
			node->ptr  = (state==xlnode::USED) ? mark_ptr_1(ptr, 0) : ptr;
			node->next = next;
			node->prev = prev;
			node->clear(nill);
		}

		void				xlarge_allocator::init(void* mem_begin, u32 mem_size, u32 size_alignment, u32 address_alignment, x_iidx_allocator* node_allocator)
		{
			x_iidx_allocator* a = node_allocator;

			mNodeAllocator		= a;
			mBaseAddress		= mem_begin;
			allocate_node(a, mRootSizeTree);
			allocate_node(a, mRootAddrTree);
			mNodeListHead		= 0xffff;
			mSizeAlignment		= size_alignment;
			mAddressAlignment	= address_alignment;

			u16 const size_nill = mRootSizeTree;
			u16 const addr_nill = mRootAddrTree;

			u16 beginNodeIdx;
			xlnode* beginNode = allocate_node(a, beginNodeIdx);
			u16 endNodeIdx;
			xlnode* endNode   = allocate_node(a, endNodeIdx);
			u16 sizeNodeIdx;
			xlnode* sizeNode  = allocate_node(a, sizeNodeIdx);

			mNodeListHead = beginNodeIdx;
			void* mem_end = advance_ptr(mem_begin, mem_size);

			init_node(sizeNode , mem_begin, xlnode::FREE, endNodeIdx  , beginNodeIdx, size_nill);
			init_node(beginNode, mem_begin, xlnode::USED, sizeNodeIdx , endNodeIdx  , size_nill);
			init_node(endNode  , mem_end  , xlnode::USED, beginNodeIdx, sizeNodeIdx , size_nill);

			xlnode* sizeRoot = (xlnode*)a->to_ptr(mRootSizeTree);
			init_node(sizeRoot, NULL, xlnode::USED, size_nill, size_nill, size_nill);
			sizeRoot->set_left(sizeNodeIdx);

			xlnode* addrRoot = (xlnode*)a->to_ptr(mRootAddrTree);
			init_node(addrRoot, NULL, xlnode::USED, addr_nill, addr_nill, addr_nill);
		}

		void				xlarge_allocator::release	()
		{
			x_iidx_allocator* a = mNodeAllocator;
			// There are always at least 3 nodes so we can safely
			// start the loop assuming mNodeListHead is not nill
			u16 i = mNodeListHead;
			do {
				xlnode* n = (xlnode*)a->to_ptr(i);
				i = n->next;
				a->deallocate(n);
			} while (i != mNodeListHead);

			a->ideallocate(mRootSizeTree);
			a->ideallocate(mRootAddrTree);
		}
		
		void*				xlarge_allocator::allocate	(u32 size, u32 alignment)
		{
			x_iidx_allocator* a = mNodeAllocator;

			// Align the size up with 'mSizeAlignment'
			// Align the alignment up with 'mAddressAlignment'
			size      = x_intu::alignUp(size     , mSizeAlignment   );
			alignment = x_intu::alignUp(alignment, mAddressAlignment);

			// Find the first node in the size tree that has the same or larger size
			// Start to iterate through the tree until we find a node that can hold
			// our size+alignment.
			u16 node;
			u32 nodeSize;
			if (find_size(mRootSizeTree, size, alignment, a, node, nodeSize) == false)
				return NULL;

			xlnode* nodep = (xlnode*)xlnode::to_ptr(a, node);

			// @TODO: We still need to check that if the alignment is very big to
			//        have a new node take that wasted-space.
			nodep->ptr    = align_ptr(nodep->ptr, alignment);

			// Check if we have to split this node, if so do it, create a new node
			// to hold the left-over size, insert it into the double linked list and
			// insert it into the size tree.
			if ((nodeSize - size) >= mSizeAlignment)
			{
				// Split
				void* p;
				u16 newNode = a->iallocate(p);
				
				// Insert it into the linear list after 'node'
				xlnode* newNodep = (xlnode*)xlnode::to_ptr(a, newNode);
				newNodep->next = nodep->next;
				newNodep->prev = node;
				newNodep->ptr  = advance_ptr(nodep->ptr, size);

				xlnode* nextp = (xlnode*)xlnode::to_ptr(a, nodep->next);
				nextp->prev = newNode;
				nodep->next = newNode;

				// Insert this new node into the size tree
				insert_size(mRootSizeTree, newNode, a);
			}

			// Remove 'node' from the size tree since it is not available/free
			// anymore
			remove_from_tree(mRootSizeTree, node, a);

			// Mark our node as used
			nodep->ptr = mark_ptr_1(nodep->ptr, 0);

			// Insert our alloc node into the address tree so that we can find it when
			// deallocate is called.
			insert_addr(mRootAddrTree, node, a);

			// Done...

			return get_ptr(nodep->ptr, 1);
		}


		void				xlarge_allocator::deallocate	(void* ptr)
		{
			x_iidx_allocator* a = mNodeAllocator;

			// Find this address in the address-tree and remove it from there.
			u16 node;
			if (find_addr(mRootAddrTree, ptr, a, node)==false)
				return;

			// Remove the node from the tree, it is deallocated so it has no
			// reason to be in this tree anymore.
			remove_from_tree(mRootAddrTree, node, a);

			xlnode* nodep = (xlnode*)xrbnode15::to_ptr(a, node);

			u16 const next = nodep->next;
			u16 const prev = nodep->prev;

			xlnode* nextp = (xlnode*)xrbnode15::to_ptr(a, next);
			xlnode* prevp = (xlnode*)xrbnode15::to_ptr(a, prev);

			// Check if we can merge with our previous and next nodes, handle
			// any merge and remove any obsolete node from the size-tree.
			bool prev_used = get_ptr_mark(prevp->ptr, 0) != 0;
			bool next_used = get_ptr_mark(nextp->ptr, 0) != 0;

			// mark our node to indicate it is free now
			nodep->ptr = mark_ptr_0(nodep->ptr, 0);

			if (!prev_used)
				remove_from_list(node, a);

			if (!next_used)
			{
				remove_from_list(next, a);
				remove_from_tree(mRootSizeTree, next, a);

				// next has been merged so we can deallocate it
				a->ideallocate(next);
			}

			// The size of 'prev' has changed if we have merged, so we need to pull
			// out the 'prev' node from the size tree and re-insert it.
			if (!prev_used)
			{
				remove_from_tree(mRootSizeTree, prev, a);
				insert_size(mRootSizeTree, prev, a);

				// node has been merged with prev so it is not needed anymore
				a->ideallocate(node);
			}
			else
			{
				// Insert node into the size tree
				insert_size(mRootSizeTree, node, a);
			}

			// Done...
		}

		// Size tree function implementations
		static void			insert_size(u16 root, u16 node, x_iidx_allocator* a)
		{
			xlnode* rootPtr = (xlnode*)a->to_ptr(root);
			xlnode* nodePtr = (xlnode*)a->to_ptr(node);
		
			xlnode* nextNodePtr = (xlnode*)a->to_ptr(nodePtr->next);
			u32 const nodeSize  = diff_ptr(nextNodePtr->ptr, nodePtr->ptr);

			u16     lastNode  = root;
			xlnode* lastNodep = rootPtr;
			u16     curNode   = rootPtr->get_child(xlnode::LEFT);
			xlnode* curNodep  = (xlnode*)(xrbnode15::to_ptr(a, curNode));
			s32 s = xlnode::LEFT;
			while (curNode != root)
			{
				lastNode  = curNode;
				lastNodep = curNodep;

				xlnode* nextNodep  = (xlnode*)xrbnode15::to_ptr(a, curNodep->next);
				u32 const curSize = diff_ptr(curNodep, nextNodep);

				if (nodeSize < curSize)
				{ s = xlnode::LEFT; }
				else if (nodeSize > curSize)
				{ s = xlnode::RIGHT; }
				else
				{	// Duplicate, insert it as a sibling
					curNodep->insert_sibling(curNode, node, root, a);
					return;	
				}
				curNode  = curNodep->get_child(s);
				curNodep = (xlnode*)xrbnode15::to_ptr(a, curNode);
			}

			rb15_attach_to(node, lastNode, s, a);
			rb15_insert_fixup(root, node, a);

	#ifdef DEBUG_RB15TREE
			rb15_check(root, a);
	#endif
		}

		static bool	can_handle_size(u16 node, u32 size, u32 alignment, x_iidx_allocator* a, u32& outSize)
		{
			xlnode* nodep    = (xlnode*)xrbnode15::to_ptr(a, node);
			xlnode* nextp    = (xlnode*)xrbnode15::to_ptr(a, nodep->next);
			u32 const node_size = diff_ptr(nodep, nextp);

			ASSERT(x_intu::isPowerOf2(alignment));
			u32 align_mask = alignment - 1;

			// See if we can use this block
			if (size >= node_size)
			{
				// Verify the alignment
				u32 align_shift = (u32)nodep->ptr & align_mask;
				if (align_shift!=0)
				{
					// How many bytes do we have to add to the pointer to reach
					// the required alignment?
					align_shift = alignment - align_shift;

					// The pointer of the block does not match our alignment, so
					// here we have to check what happens when we add this difference
					// to the pointer and if the size is still sufficient to hold our
					// required size.
					if ((size + align_shift) <= node_size)
					{
						// Ok, we found a block which satisfies our request
						outSize = node_size;
						return true;
					}
				}
				else
				{
					// The alignment of the pointer is already enough to satisfy
					// our request, so here we can say we have found a best-fit.
					outSize = node_size;
					return true;
				}
			}
			outSize = 0;
			return false;
		}

		static bool			find_size(u16 root, u32 size, u32 alignment, x_iidx_allocator* a, u16& outNode, u32& outNodeSize)
		{
			u16 const    nill      = root;
			u16          lastNode  = root;
			xlnode*      lastNodep = (xlnode*)xrbnode15::to_ptr(a, root);
			u16          curNode   = lastNodep->get_child(xlnode::LEFT);
			xlnode*      curNodep  = (xlnode*)xrbnode15::to_ptr(a, curNode);
			s32 s = xlnode::LEFT;
			while (curNode != nill)
			{
				lastNode  = curNode;
				lastNodep = curNodep;

				xlnode* nextNodep = (xlnode*)xrbnode15::to_ptr(a, curNodep->next);
				u32 const curSize = diff_ptr(curNodep->ptr, nextNodep->ptr);

				if (size < curSize)
				{ s = xlnode::LEFT; }
				else if (size > curSize)
				{ s = xlnode::RIGHT; }
				else
				{
					// We have found a node that holds the same size, we will
					// traverse the tree from here searching for the best-fit.
					break;
				}
				curNode  = curNodep->get_child(s);
				curNodep = (xlnode*)xrbnode15::to_ptr(a, curNode);
			}

			curNode  = lastNode;
			curNodep = lastNodep;
			if (s == xlnode::RIGHT)
			{
				// Get the successor since our size is bigger than the size
				// that the lastNode is holding
				curNode  = rb15_inorder(1, curNode, root, a);
				curNodep = (xlnode*)xrbnode15::to_ptr(a, curNode);
			}


			// Now traverse to the right until we find a block that satisfies our
			// size and alignment.
			while (curNode != nill)
			{
				u32 curNodeSize;
				if (can_handle_size(curNode, size, alignment, a, curNodeSize))
				{
					outNode     = curNode;
					outNodeSize = curNodeSize;
					return true;
				}

				// Iterate over all the blocks, we have to do this since every block
				// might have a different alignment to start with and there is a 
				// possibility that we find a best-fit.
				u16 curNodeSibling = curNodep->get_sibling();
				while (curNodeSibling != nill)
				{
					xlnode* curNodeSiblingp = (xlnode*)xrbnode15::to_ptr(a, curNodeSibling);
					if (can_handle_size(curNodeSibling, size, alignment, a, curNodeSize))
					{
						outNode     = curNodeSibling;
						outNodeSize = curNodeSize;
						return true;
					}
					curNodeSibling = curNodeSiblingp->get_right();
				}

				curNode  = rb15_inorder(0, curNode, root, a);
				curNodep = (curNode!=nill) ? ((xlnode*)xrbnode15::to_ptr(a, curNode)) : NULL;
			}
			return false;
		}

		void			insert_addr(u16 root, u16 node, x_iidx_allocator* a)
		{
			xlnode* rootPtr = (xlnode*)a->to_ptr(root);
			xlnode* nodePtr = (xlnode*)a->to_ptr(node);

			// @TODO
			// Since we can get the prev node from our linear list
			// we could shortcut the search here since we are on the right
			// of our prev node.

			u16     lastNode  = root;
			xlnode* lastNodep = rootPtr;

			u16     curNode   = rootPtr->get_child(xlnode::LEFT);
			xlnode* curNodep  = (xlnode*)(xrbnode15::to_ptr(a, curNode));
			s32 s = xlnode::LEFT;
			while (curNodep != rootPtr)
			{
				lastNode  = curNode;
				lastNodep = curNodep;

				if ((xbyte*)nodePtr->ptr < (xbyte*)curNodep->ptr)
				{ s = xlnode::LEFT; }
				else if ((xbyte*)nodePtr->ptr > (xbyte*)curNodep->ptr)
				{ s = xlnode::RIGHT; }

				curNode  = curNodep->get_child(s);
				curNodep = (xlnode*)xrbnode15::to_ptr(a, curNode);
			}

			rb15_attach_to(node, lastNode, s, a);
			rb15_insert_fixup(root, node, a);

#ifdef DEBUG_RB15TREE
			rb15_check(root, a);
#endif
		}

		bool			find_addr(u16 root, void* ptr, x_iidx_allocator* a, u16& outNode)
		{
			xlnode* rootPtr = (xlnode*)a->to_ptr(root);

			u16     curNode   = rootPtr->get_child(xlnode::LEFT);
			xlnode* curNodep  = (xlnode*)(xrbnode15::to_ptr(a, curNode));
			s32 s = xlnode::LEFT;
			while (curNodep != rootPtr)
			{
				if ((xbyte*)ptr < (xbyte*)curNodep->ptr)
				{ s = xlnode::LEFT; }
				else if ((xbyte*)ptr > (xbyte*)curNodep->ptr)
				{ s = xlnode::RIGHT; }
				else
				{
					outNode = curNode;
					return true;
				}
				curNode  = curNodep->get_child(s);
				curNodep = (xlnode*)xrbnode15::to_ptr(a, curNode);
			}
			return false;
		}

		static void			remove_from_list(u16 node, x_iidx_allocator* a)
		{
			// The node to remove from the linear list
			xlnode* nodep = (xlnode*)xrbnode15::to_ptr(a, node);

			xlnode* nextp = (xlnode*)xrbnode15::to_ptr(a, nodep->prev);
			xlnode* prevp = (xlnode*)xrbnode15::to_ptr(a, nodep->next);

			prevp->next = nodep->next;
			nextp->prev = nodep->prev;
		}


		static void			remove_from_tree(u16 root, u16 node, x_iidx_allocator* a)
		{
			u16 const nill = root;
			xlnode* rootp  = (xlnode*)xrbnode15::to_ptr(a, root);

			// The node to remove
			xlnode* nodep = (xlnode*)xrbnode15::to_ptr(a, node);
			ASSERT(node != root);

			// It's possible the node is a sibling, in that case we have to search
			// the tree for the 'parent'/'head' node and remove this node from there
			// as a sibling.
			if (nodep->is_sibling(nill))
			{
				// First determine the 'size' that this node represents
				xlnode*  nextNodep = (xlnode*)xlnode::to_ptr(a, nodep->next);
				u32 const nodeSize = diff_ptr(nodep->ptr, nextNodep->ptr);

				u16     curNode   = rootp->get_child(xlnode::LEFT);
				xlnode* curNodep  = (xlnode*)(xlnode::to_ptr(a, curNode));
				s32 s = xlnode::LEFT;
				while (curNodep != rootp)
				{
					nextNodep = (xlnode*)xlnode::to_ptr(a, curNodep->next);
					u32 const curNodeSize = diff_ptr(curNodep->ptr, nextNodep->ptr);

					if (nodeSize < curNodeSize)
					{ s = xlnode::LEFT; }
					else if (nodeSize > curNodeSize)
					{ s = xlnode::RIGHT; }
					else
					{
						// Remove the node as a sibling from this tree node
						ASSERT(curNodep->has_sibling(nill));
						curNodep->remove_sibling(node, nill, a);
						break;
					}
					curNode  = curNodep->get_child(s);
					curNodep = (xlnode*)xlnode::to_ptr(a, curNode);
				}
			}
			else
			{
				u16     repl  = node;
				xlnode* replp = nodep;

				s32 s = xlnode::LEFT;
				if (nodep->get_right() != root)
				{
					if (nodep->get_left() != root)
					{
						repl = nodep->get_right();
						replp = (xlnode*)xrbnode15::to_ptr(a, repl);
						while (replp->get_left() != root)
						{
							repl = replp->get_left();
							replp = (xlnode*)xrbnode15::to_ptr(a, repl);
						}
					}
					s = xlnode::RIGHT;
				}
				ASSERT(replp->get_child(1-s) == root);
				bool const red = replp->is_red();
				u16 replChild  = replp->get_child(s);
				xlnode* replChildp = (xlnode*)xrbnode15::to_ptr(a, replChild);

				rb15_substitute_with(repl, replChild, a);
				ASSERT(rootp->is_black());

				if (repl != node)
					rb15_switch_with(repl, node, a);

				ASSERT(rootp->is_black());

				if (!red) 
					rb15_erase_fixup(root, replChild, a);

#ifdef DEBUG_RB15TREE
				rb15_check(root, a);
#endif
			}
		}
	}
}