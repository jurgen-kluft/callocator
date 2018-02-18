#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_integer.h"
#include "xbase/x_tree.h"
#include "xallocator/private/x_largebin32.h"

namespace xcore
{
	namespace xexternal32
	{
		/*
		Large block allocator

		This allocator uses external book keeping data to manage the 'managed memory'. This means that additional memory
		is used for data that contains meta-data about the allocated and free blocks of the 'managed memory'.

		A red-black tree is used to quickly identify a suitable (best-fit) block for allocation, while the deallocation
		code path is using an additional red-black tree for quickly finding the allocated block so as to release it back
		into the red-black tree of free blocks.

		*/

		static void			insert_size(u32 root, u32 node, x_iidx_allocator* node_allocator);
		static bool			find_bestfit(u32 root, u32 size, u32 alignment, x_iidx_allocator* a, u32& outNode, u32& outNodeSize);

		static void			insert_addr(u32 root, u32 node, x_iidx_allocator* node_allocator);
		static bool			find_addr(u32 root, memptr ptr, x_iidx_allocator* a, u32& outNode);

		static xlnode*		insert_in_list(xlnode* nodep, u32 nill, u32 size, x_iidx_allocator* a, u32& newNode);
		static void			remove_from_list(u32 node, x_iidx_allocator* a);
		static void			remove_from_tree(u32 root, u32 node, x_iidx_allocator* a);

		void				xlargebin::init(void* mem_begin, u32 mem_size, u32 size_alignment, u32 address_alignment, x_iidx_allocator* node_allocator)
		{
			x_iidx_allocator* a = node_allocator;

			// Allocate the root nodes for the @size and @address tree
			mNodeAllocator		= a;
			xlnode* sizeRoot    = allocate_node(a, mRootSizeTree);
			xlnode* addrRoot    = allocate_node(a, mRootAddrTree);

			init_node(sizeRoot, 0, xlnode::STATE_USED, mRootSizeTree, mRootSizeTree, mRootSizeTree);
			init_node(addrRoot, 0, xlnode::STATE_USED, mRootAddrTree, mRootAddrTree, mRootAddrTree);
			mBaseAddress		= mem_begin;
			mNodeListHead		= 0xffffffff;
			mSizeAlignment		= size_alignment;
			mAddressAlignment	= address_alignment;

			// The initial tree will contain 3 nodes, 2 nodes that define
			// the span of the managed memory (begin - end). These 2 nodes
			// are always marked USED, so the allocation code will never
			// touch them. They are there to make the logic simpler. When
			// the tree is empty only these 2 nodes are left and the logic
			// will figure out that there is no free memory.

			u32 beginNodeIdx;
			xlnode* beginNode = allocate_node(a, beginNodeIdx);
			u32 sizeNodeIdx;
			xlnode* sizeNode  = allocate_node(a, sizeNodeIdx);
			u32 endNodeIdx;
			xlnode* endNode   = allocate_node(a, endNodeIdx);

			// Link these 3 nodes into a double linked list. This list will always be ordered by the
			// address of the managed memory that is identified with the node.
			// So after n allocations this list will contain n+3 nodes where every node will be
			// pointing in managed memory increasingly higher.
			init_node(beginNode,        0, xlnode::STATE_USED, sizeNodeIdx , endNodeIdx  , mRootSizeTree);
			init_node(sizeNode ,        0, xlnode::STATE_FREE, endNodeIdx  , beginNodeIdx, mRootSizeTree);
			init_node(endNode  , mem_size, xlnode::STATE_USED, beginNodeIdx, sizeNodeIdx , mRootSizeTree);

			mNodeListHead = beginNodeIdx;

			// Add the 'initial' size node to the size tree
			insert_size(mRootSizeTree, sizeNodeIdx, a);

		}

		void				xlargebin::release	()
		{
			x_iidx_allocator* a = mNodeAllocator;

			// There are always at least 3 nodes so we can safely
			// start the loop assuming mNodeListHead is not nill
			u32 i = mNodeListHead;
			do {
				xlnode* n = (xlnode*)a->to_ptr(i);
				i = n->next;
				a->deallocate(n);
			} while (i != mNodeListHead);

			a->ideallocate(mRootSizeTree);
			a->ideallocate(mRootAddrTree);

			mRootSizeTree = 0xffff;
			mRootAddrTree = 0xffff;
			mNodeListHead = 0xffff;

			mSizeAlignment    = 0xffffffff;
			mAddressAlignment = 0xffffffff;
		}
		
		void*				xlargebin::allocate	(u32 size, u32 alignment)
		{
			x_iidx_allocator* a = mNodeAllocator;

			// Align the size up with 'mSizeAlignment'
			// Align the alignment up with 'mAddressAlignment'
			size      = xalignUp(size, mSizeAlignment);
			alignment = xmax(alignment, mAddressAlignment);

			// Find the first node in the size tree that has the same or larger size
			// Start to iterate through the tree until we find a node that can hold
			// our size+alignment.
			u32 node;
			u32 nodeSize;
			if (find_bestfit(mRootSizeTree, size, alignment, a, node, nodeSize) == false)
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
				// Add to the linear list after 'node'
				u32 newNode;
				xlnode* newNodep = insert_in_list(nodep, mRootSizeTree, size, a, newNode);
				if (newNodep == NULL)
					return NULL;

				// Insert this new node into the size tree
				newNodep->set_free();
				insert_size(mRootSizeTree, newNode, a);
			}

			// Mark our node as used
			nodep->set_used();

			// Remove 'node' from the size tree since it is not available/free
			// anymore
			remove_from_tree(mRootSizeTree, node, a);

			// Insert our alloc node into the address tree so that we can find it when
			// deallocate is called.
			insert_addr(mRootAddrTree, node, a);

			// Done...

			return (void*)((uptr)mBaseAddress + nodep->ptr);
		}


		void				xlargebin::deallocate	(void* ptr)
		{
			x_iidx_allocator* a = mNodeAllocator;

			memptr p = (xbyte*)ptr - (xbyte*)mBaseAddress;

			// Find this address in the address-tree and remove it from there.
			u32 node;
			if (find_addr(mRootAddrTree, p, a, node)==false)
				return;

			// Remove the node from the tree, it is deallocated so it has no
			// reason to be in this tree anymore.
			remove_from_tree(mRootAddrTree, node, a);

			xlnode* nodep = (xlnode*)xrbnode31::to_ptr(a, node);

			u32 const next = nodep->next;
			u32 const prev = nodep->prev;

			xlnode* nextp = (xlnode*)xrbnode31::to_ptr(a, next);
			xlnode* prevp = (xlnode*)xrbnode31::to_ptr(a, prev);

			// Check if we can merge with our previous and next nodes, handle
			// any merge and remove any obsolete node from the size-tree.
			bool prev_used = prevp->is_used();
			bool next_used = nextp->is_used();

			// mark our node to indicate it is free now
			nodep->set_free();

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
		static void			insert_size(u32 root, u32 node, x_iidx_allocator* a)
		{
			xlnode* rootPtr = (xlnode*)a->to_ptr(root);
			xlnode* nodePtr = (xlnode*)a->to_ptr(node);
			ASSERT(nodePtr->is_free());
		
			xlnode* nextNodePtr = (xlnode*)a->to_ptr(nodePtr->next);
			xsize_t const nodeSize  = diff_ptr(nodePtr->ptr, nextNodePtr->ptr);

			u32     lastNode  = root;
			u32     curNode   = rootPtr->get_child(xlnode::LEFT);

			s32 s = xlnode::LEFT;
			while (curNode != root)
			{
				lastNode  = curNode;
				xlnode* curNodep = (xlnode*)(xrbnode31::to_ptr(a, curNode));
				xlnode* nextNodep  = (xlnode*)xrbnode31::to_ptr(a, curNodep->next);
				xsize_t const curSize = diff_ptr(curNodep->ptr, nextNodep->ptr);

				if (nodeSize < curSize)
				{ s = xlnode::LEFT; }
				else if (nodeSize > curSize)
				{ s = xlnode::RIGHT; }
				else
				{	// Identical size, sort them by address
					if (nodePtr->ptr < curNodep->ptr)
					{ s = xlnode::LEFT; }
					else
					{ s = xlnode::RIGHT; }
				}
				curNode  = curNodep->get_child(s);
			}

			rb31_attach_to(node, lastNode, s, a);
			rb31_insert_fixup(root, node, a);

	#ifdef DEBUG_RB31TREE
			rb31_check(root, a);
	#endif
		}

		static bool	can_handle_size(u32 node, u32 size, u32 alignment, x_iidx_allocator* a, u32& outSize)
		{
			// Convert index to node ptr
			xlnode* nodep    = (xlnode*)xrbnode31::to_ptr(a, node);
			xlnode* nextp    = (xlnode*)xrbnode31::to_ptr(a, nodep->next);
			
			// Compute the 'size' that 'node' can allocate. This is done by taking
			// the 'external mem ptr' of the current node and the 'external mem ptr'
			// of the next node and substracting them.
			xsize_t const node_size = diff_ptr(nodep->ptr, nextp->ptr);

			ASSERT(xispo2(alignment));
			u32 align_mask = alignment - 1;

			// See if we can use this block
			if (size <= node_size)
			{
				// Verify the alignment
				u32 align_shift = (u32)((uptr)nodep->ptr & align_mask);
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
						outSize = (u32)node_size;
						return true;
					}
				}
				else
				{
					// The alignment of the pointer is already enough to satisfy
					// our request, so here we can say we have found a best-fit.
					outSize = (u32)node_size;
					return true;
				}
			}
			outSize = 0;
			return false;
		}

		static bool			find_bestfit(u32 root, u32 size, u32 alignment, x_iidx_allocator* a, u32& outNode, u32& outNodeSize)
		{
			u32 const    nill      = root;
			u32          lastNode  = root;
			xlnode*      rootp     = (xlnode*)xrbnode31::to_ptr(a, root);
			u32          curNode   = rootp->get_child(xlnode::LEFT);
			s32 s = xlnode::LEFT;
			while (curNode != nill)
			{
				lastNode  = curNode;

				xlnode* curNodep = (xlnode*)xrbnode31::to_ptr(a, curNode);
				xlnode* nextNodep = (xlnode*)xrbnode31::to_ptr(a, curNodep->next);
				xsize_t const curSize = diff_ptr(curNodep->ptr, nextNodep->ptr);

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
			}

			curNode  = lastNode;
			if (s == xlnode::RIGHT)
			{
				// Get the successor since our size is bigger than the size
				// that the lastNode is holding
				curNode  = rb31_inorder(1, curNode, root, a);
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

				curNode  = rb31_inorder(0, curNode, root, a);
			}
			return false;
		}

		void			insert_addr(u32 root, u32 node, x_iidx_allocator* a)
		{
			xlnode* rootPtr = (xlnode*)a->to_ptr(root);
			xlnode* nodePtr = (xlnode*)a->to_ptr(node);

			// @TODO
			// Since we can get the prev node from our linear list
			// we could shortcut the search here since we are on the right
			// of our prev node.
			ASSERT(nodePtr->is_used());

			u32     lastNode  = root;
			xlnode* lastNodep = rootPtr;

			u32     curNode   = rootPtr->get_child(xlnode::LEFT);
			s32		dir       = xlnode::LEFT;
			while (curNode != root)
			{

				lastNode  = curNode;

				xlnode* curNodep = (xlnode*)(xrbnode31::to_ptr(a, curNode));
				if (nodePtr->ptr < curNodep->ptr)
				{ dir = xlnode::LEFT; }
				else if (nodePtr->ptr > curNodep->ptr)
				{ dir = xlnode::RIGHT; }

				curNode  = curNodep->get_child(dir);
			}

			rb31_attach_to(node, lastNode, dir, a);
			rb31_insert_fixup(root, node, a);

#ifdef DEBUG_RB31TREE
			rb31_check(root, a);
#endif
		}

		bool			find_addr(u32 root, memptr ptr, x_iidx_allocator* a, u32& outNode)
		{
			xlnode* rootPtr = (xlnode*)a->to_ptr(root);
			u32     curNode   = rootPtr->get_child(xlnode::LEFT);
			s32 s = xlnode::LEFT;
			while (curNode != root)
			{
				xlnode* curNodep = (xlnode*)xrbnode31::to_ptr(a, curNode);
				memptr cptr = curNodep->ptr;

				if (ptr < cptr)
				{ s = xlnode::LEFT; }
				else if (ptr > cptr)
				{ s = xlnode::RIGHT; }
				else
				{
					outNode = curNode;
					return true;
				}
				curNode  = curNodep->get_child(s);
			}
			return false;
		}

		inline u32			to_idx(x_iidx_allocator* a, xlnode* node)	{ return a->to_idx(node); }

		xlnode*		insert_in_list(xlnode* nodep, u32 nill, u32 size, x_iidx_allocator* a, u32& newNode)
		{
			newNode = 0xffffffff;
			{
				void* p;
				newNode = a->iallocate(p);
				if (p == NULL)
					return NULL;
			}

			xlnode* newNodep = (xlnode*)xlnode::to_ptr(a, newNode);
			newNodep->clear(newNode);
			newNodep->next = nodep->next;
			newNodep->prev = to_idx(a, nodep);
			newNodep->ptr  = advance_ptr(nodep->ptr, size);

			xlnode* nextp = (xlnode*)xlnode::to_ptr(a, nodep->next);
			nextp->prev = newNode;
			nodep->next = newNode;

			return newNodep;
		}

		void			remove_from_list(u32 node, x_iidx_allocator* a)
		{
			// The node to remove from the linear list
			xlnode* nodep = (xlnode*)xrbnode31::to_ptr(a, node);

			xlnode* nextp = (xlnode*)xrbnode31::to_ptr(a, nodep->next);
			xlnode* prevp = (xlnode*)xrbnode31::to_ptr(a, nodep->prev);

			prevp->next = nodep->next;
			nextp->prev = nodep->prev;
		}


		static void			remove_from_tree(u32 root, u32 node, x_iidx_allocator* a)
		{
			u32 const nill = root;
			xlnode* rootp  = (xlnode*)xrbnode31::to_ptr(a, root);

			// The node to remove
			xlnode* nodep = (xlnode*)xrbnode31::to_ptr(a, node);
			ASSERT(node != root);

			{
				u32     repl  = node;
				xlnode* replp = nodep;

				s32 s = xlnode::LEFT;
				if (nodep->get_right() != nill)
				{
					if (nodep->get_left() != nill)
					{
						repl = nodep->get_right();
						replp = (xlnode*)xrbnode31::to_ptr(a, repl);
						while (replp->get_left() != nill)
						{
							repl = replp->get_left();
							replp = (xlnode*)xrbnode31::to_ptr(a, repl);
						}
					}
					s = xlnode::RIGHT;
				}
				ASSERT(replp->get_child(1-s) == nill);
				bool const red = replp->is_red();
				u32 replChild  = replp->get_child(s);
				xlnode* replChildp = (xlnode*)xrbnode31::to_ptr(a, replChild);

				rb31_substitute_with(repl, replChild, a);
				ASSERT(rootp->is_black());

				if (repl != node)
					rb31_switch_with(repl, node, a);

				ASSERT(rootp->is_black());

				if (!red) 
					rb31_erase_fixup(root, replChild, a);

#ifdef DEBUG_RB31TREE
				rb31_check(root, a);
#endif
			}
		}
	}
}