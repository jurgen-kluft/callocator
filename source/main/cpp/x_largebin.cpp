#include "xbase\x_target.h"
#include "xbase\x_debug.h"
#include "xbase\x_integer.h"
#include "xbase\x_tree.h"
#include "xallocator\private\x_largebin.h"

namespace xcore
{
	namespace xexternal
	{
		/*
		Large block allocator

		This allocator uses external book keeping data to manage the 'managed memory'. This means that additional memory
		is used for data that contains meta-data about the allocated and free blocks of the 'managed memory'.

		A red-black tree is used to quickly identify a suitable (best-fit) block for allocation, while the deallocation
		code path is using an additional red-black tree for quickly finding the allocated block so as to release it back
		into the red-black tree of free blocks.

		*/

		static void			insert_size(xlnode* root, xlnode* node);
		static bool			find_bestfit(xlnode* root, u32 size, u32 alignment, xlnode*& outNode, u32& outNodeSize);

		static void			insert_addr(xlnode* root, xlnode* node);
		static bool			find_addr(xlnode* root, memptr ptr, xlnode*& outNode);

		static xlnode*		insert_in_list(xlnode* nodep, u32 size, x_iallocator* a);
		static void			remove_from_list(xlnode* node);

		static void			remove_from_tree(xlnode* root, xlnode* node);

		void				xlargebin::init(void* mem_begin, u32 mem_size, u32 size_alignment, u32 address_alignment, x_iallocator* node_allocator)
		{
			x_iallocator* a = node_allocator;

			// Allocate the root nodes for the @size and @address tree
			mNodeAllocator		= a;
			mRootSizeTree		= allocate_node(a);
			mRootAddrTree		= allocate_node(a);
			init_node(mRootSizeTree, 0, xlnode::STATE_USED, mRootSizeTree, mRootSizeTree);
			init_node(mRootAddrTree, 0, xlnode::STATE_USED, mRootAddrTree, mRootAddrTree);

			mBaseAddress		= mem_begin;
			mNodeListHead		= NULL;
			mSizeAlignment		= size_alignment;
			mAddressAlignment	= address_alignment;

			// The initial tree will contain 3 nodes, 2 nodes that define
			// the span of the managed memory (begin - end). These 2 nodes
			// are always marked USED, so the allocation code will never
			// touch them. They are there to make the logic simpler. When
			// the tree is empty only these 2 nodes are left and the logic
			// will figure out that there is no free memory.

			xlnode* beginNode = allocate_node(a);
			xlnode* sizeNode  = allocate_node(a);
			xlnode* endNode   = allocate_node(a);

			// Link these 3 nodes into a double linked list. This list will always be ordered by the
			// address of the managed memory that is identified with the node.
			// So after n allocations this list will contain n+3 nodes where every node will be
			// pointing in managed memory increasingly higher.
			init_node(beginNode,        0, xlnode::STATE_USED, sizeNode , endNode  );
			init_node(sizeNode ,        0, xlnode::STATE_FREE, endNode  , beginNode);
			init_node(endNode  , mem_size, xlnode::STATE_USED, beginNode, sizeNode );

			mNodeListHead = beginNode;

			// Add the 'initial' size node to the size tree
			insert_size(mRootSizeTree, sizeNode);
		}

		void				xlargebin::release	()
		{
			x_iallocator* a = mNodeAllocator;

			// There are always at least 3 nodes so we can safely
			// start the loop assuming mNodeListHead is not nill
			xlnode* i = mNodeListHead;
			do {
				xlnode* n = i;
				i = n->next;
				a->deallocate(n);
			} while (i != mNodeListHead);

			a->deallocate(mRootSizeTree);
			a->deallocate(mRootAddrTree);

			mRootSizeTree = NULL;
			mRootAddrTree = NULL;
			mNodeListHead = NULL;

			mSizeAlignment    = 0xffffffff;
			mAddressAlignment = 0xffffffff;
		}
		
		void*				xlargebin::allocate	(u32 size, u32 alignment)
		{
			// Align the size up with 'mSizeAlignment'
			// Align the alignment up with 'mAddressAlignment'
			size      = x_intu::alignUp(size, mSizeAlignment);
			alignment = x_intu::max(alignment, mAddressAlignment);

			// Find the first node in the size tree that has the same or larger size
			// Start to iterate through the tree until we find a node that can hold
			// our size+alignment.
			xlnode* node;
			u32 nodeSize;
			if (find_bestfit(mRootSizeTree, size, alignment, node, nodeSize) == false)
				return NULL;

			// @TODO: We still need to check that if the alignment is very big to
			//        have a new node take that wasted-space.
			node->ptr    = align_ptr(node->ptr, alignment);

			// Check if we have to split this node, if so do it, create a new node
			// to hold the left-over size, insert it into the double linked list and
			// insert it into the size tree.
			if ((nodeSize - size) >= mSizeAlignment)
			{
				// Add to the linear list after 'node'
				xlnode* newNodep = insert_in_list(node, size, mNodeAllocator);
				if (newNodep == NULL)
					return NULL;

				// Insert this 'free' new node into the size tree
				newNodep->set_free();
				insert_size(mRootSizeTree, newNodep);
			}

			// Mark our node as used
			node->set_used();

			// Remove 'node' from the size tree since it is not available/free
			// anymore
			remove_from_tree(mRootSizeTree, node);

			// Insert our alloc node into the address tree so that we can find it when
			// deallocate is called.
			insert_addr(mRootAddrTree, node);

			// Done...

			return (void*)((uptr)mBaseAddress + node->ptr);
		}


		void				xlargebin::deallocate	(void* ptr)
		{
			x_iallocator* a = mNodeAllocator;

			memptr p = (xbyte*)ptr - (xbyte*)mBaseAddress;

			// Find this address in the address-tree and remove it from there.
			xlnode* node;
			if (find_addr(mRootAddrTree, p, node)==false)
				return;

			// Remove the node from the tree, it is deallocated so it has no
			// reason to be in this tree anymore.
			remove_from_tree(mRootAddrTree, node);

			xlnode* next = node->next;
			xlnode* prev = node->prev;

			// Check if we can merge with our previous and next nodes, handle
			// any merge and remove any obsolete node from the size-tree.
			bool prev_used = prev->is_used();
			bool next_used = next->is_used();

			// mark our node to indicate it is free now
			node->set_free();

			if (!prev_used)
				remove_from_list(node);

			if (!next_used)
			{
				remove_from_list(next);
				remove_from_tree(mRootSizeTree, next);

				// next has been merged so we can deallocate it
				a->deallocate(next);
			}

			// The size of 'prev' has changed if we have merged, so we need to pull
			// out the 'prev' node from the size tree and re-insert it.
			if (!prev_used)
			{
				remove_from_tree(mRootSizeTree, prev);
				insert_size(mRootSizeTree, prev);

				// node has been merged with prev so it is not needed anymore
				a->deallocate(node);
			}
			else
			{
				// Insert node into the size tree
				insert_size(mRootSizeTree, node);
			}

			// Done...
		}

		// Size tree function implementations
		static void			insert_size(xlnode* root, xlnode* node)
		{
			ASSERT(node->is_free());
		
			xlnode* nill     = root;
			xlnode* nextNode = node->next;
			xsize_t const nodeSize  = diff_ptr(node->ptr, nextNode->ptr);

			xlnode* lastNode  = root;
			xlnode* curNode   = (xlnode*)root->get_child(xlnode::LEFT);

			s32 s = xlnode::LEFT;
			while (curNode != nill)
			{
				lastNode  = curNode;

				xlnode* nextNode  = curNode->next;
				xsize_t const curSize = diff_ptr(curNode->ptr, nextNode->ptr);

				if (nodeSize < curSize)
				{ s = xlnode::LEFT; }
				else if (nodeSize > curSize)
				{ s = xlnode::RIGHT; }
				else
				{	// Identical size, sort them by address
					if (node->ptr < curNode->ptr)
					{ s = xlnode::LEFT; }
					else
					{ s = xlnode::RIGHT; }
				}
				curNode  = (xlnode*)curNode->get_child(s);
			}

			rb_attach_to(node, lastNode, s);
			rb_insert_fixup(*root, node);

	#ifdef DEBUG_RBTREE
			rb_check(root);
	#endif
		}

		static bool	can_handle_size(xlnode* node, u32 size, u32 alignment, u32& outSize)
		{
			// Convert index to node ptr
			xlnode* next = node->next;
			
			// Compute the 'size' that 'node' can allocate. This is done by taking
			// the 'external mem ptr' of the current node and the 'external mem ptr'
			// of the next node and substracting them.
			xsize_t const node_size = diff_ptr(node->ptr, next->ptr);

			ASSERT(x_intu::isPowerOf2(alignment));
			u32 align_mask = alignment - 1;

			// See if we can use this block
			if (size <= node_size)
			{
				// Verify the alignment
				u32 align_shift = (u32)((uptr)node->ptr & align_mask);
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

		static bool			find_bestfit(xlnode* root, u32 size, u32 alignment, xlnode*& outNode, u32& outNodeSize)
		{
			xlnode*		nill      = root;
			xlnode*		lastNode  = root;
			xlnode*		curNode   = (xlnode*)lastNode->get_child(xlnode::LEFT);

			s32 s = xlnode::LEFT;
			while (curNode != nill)
			{
				lastNode  = curNode;

				xlnode* nextNode = curNode->next;
				xsize_t const curSize = diff_ptr(curNode->ptr, nextNode->ptr);

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
				curNode  = (xlnode*)curNode->get_child(s);
			}

			curNode  = lastNode;
			if (s == xlnode::RIGHT)
			{
				// Get the successor since our size is bigger than the size
				// that the lastNode is holding
				curNode  = (xlnode*)rb_inorder(1, curNode);
			}

			// Now traverse to the right until we find a block that satisfies our
			// size and alignment.
			while (curNode != nill)
			{
				u32 curNodeSize;
				if (can_handle_size(curNode, size, alignment, curNodeSize))
				{
					outNode     = curNode;
					outNodeSize = curNodeSize;
					return true;
				}
				curNode  = (xlnode*)rb_inorder(0, curNode);
			}
			return false;
		}

		void			insert_addr(xlnode* root, xlnode* node)
		{
			// @TODO
			// Since we can get the prev node from our linear list
			// we could shortcut the search here since we are on the right
			// of our prev node.
			ASSERT(node->is_used());

			xlnode* nill = root;
			xlnode*	lastNode  = root;
			xlnode*	curNode   = (xlnode*)root->get_child(xlnode::LEFT);
			s32 s = xlnode::LEFT;
			while (curNode != nill)
			{
				lastNode  = curNode;

				if (node->ptr < curNode->ptr)
				{ s = xlnode::LEFT; }
				else if (node->ptr > curNode->ptr)
				{ s = xlnode::RIGHT; }

				curNode  = (xlnode*)curNode->get_child(s);
			}

			rb_attach_to(node, lastNode, s);
			rb_insert_fixup(*root, node);

#ifdef DEBUG_RBTREE
			rb_check(root, a);
#endif
		}

		bool			find_addr(xlnode* root, memptr ptr, xlnode*& outNode)
		{
			xlnode*	nill = root;
			xlnode*	curNode = (xlnode*)root->get_child(xlnode::LEFT);
			s32 s = xlnode::LEFT;
			while (curNode != nill)
			{
				memptr cptr = curNode->ptr;

				if (ptr < cptr)
				{ s = xlnode::LEFT; }
				else if (ptr > cptr)
				{ s = xlnode::RIGHT; }
				else
				{
					outNode = curNode;
					return true;
				}
				curNode  = (xlnode*)curNode->get_child(s);
			}
			return false;
		}

		xlnode*		insert_in_list(xlnode* node, u32 size, x_iallocator* a)
		{
			xlnode* newNode = NULL;
			{
				newNode = (xlnode*)a->allocate(sizeof(xlnode), sizeof(void*));
				if (newNode == NULL)
					return NULL;
			}

			newNode->clear(newNode);
			newNode->next = node->next;
			newNode->prev = node;
			newNode->ptr  = advance_ptr(node->ptr, size);

			xlnode* next = node->next;
			next->prev = newNode;
			node->next = newNode;

			return newNode;
		}

		void			remove_from_list(xlnode* node)
		{
			// The node to remove from the linear list
			xlnode* next = node->next;
			xlnode* prev = node->prev;
			prev->next = node->next;
			next->prev = node->prev;
		}

		static void			remove_from_tree(xlnode* root, xlnode* node)
		{
			// The node to remove
			ASSERT(node != root);
			xlnode* nill = root;
			{
				xlnode* repl = node;

				s32 s = xlnode::LEFT;
				if (node->get_right() != nill)
				{
					if (node->get_left() != nill)
					{
						repl = (xlnode*)node->get_right();
						while (repl->get_left() != nill)
						{
							repl = (xlnode*)repl->get_left();
						}
					}
					s = xlnode::RIGHT;
				}
				ASSERT(repl->get_child(1-s) == nill);
				bool const red = repl->is_red();
				xlnode* replChild  = (xlnode*)repl->get_child(s);

				rb_substitute_with(repl, replChild);
				ASSERT(root->is_black());

				if (repl != node)
					rb_switch_with(repl, node);

				ASSERT(root->is_black());

				if (!red) 
					rb_erase_fixup(root, replChild);

#ifdef DEBUG_RBTREE
				rb_check(root, a);
#endif
			}
		}
	}
}