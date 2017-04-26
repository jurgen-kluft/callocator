#include "xbase\x_target.h"
#include "xbase\x_allocator.h"
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
		into the 'size' red-black tree.

		*/
		typedef	u32				memptr;

		static inline memptr	advance_ptr(memptr ptr, u32 size)		{ return (memptr)(ptr + size); }
		static inline memptr	align_ptr(memptr ptr, u32 alignment)	{ return (memptr)((ptr + (alignment - 1)) & ~((memptr)alignment - 1)); }
		static xsize_t			diff_ptr(memptr ptr, memptr next_ptr)	{ return (xsize_t)(next_ptr - ptr); }

		struct xrbnodep : public xrbnode
		{
			xlnode*		node;
		};

		struct xlnode
		{
			enum EState { STATE_USED = 4, STATE_FREE = 0 };

			void			set_state(EState state)		{ flags = (flags & ~STATE_USED) | state; }
			EState			get_state() const			{ return EState(flags & STATE_USED); }

			void			set_free()					{ flags = (flags & ~STATE_USED); }
			void			set_used()					{ flags = (flags & ~STATE_USED) | STATE_USED; }

			bool			is_free() const				{ return (flags & STATE_USED) == STATE_FREE; }
			bool			is_used() const				{ return (flags & STATE_USED) == STATE_USED; }

			u32				flags;						// Used/Not-Used
			memptr			ptr;						// (4) pointer/offset in external memory
			xlnode*			next;						// (4/8) doubly linked list sorted by physical address
			xlnode*			prev;						// (4/8)  
		};

		static inline xlnode*	allocate_list_node(x_iallocator* allocator)
		{
			void* nodePtr = allocator->allocate(sizeof(xlnode), sizeof(void*));
			xlnode* node = (xlnode*)nodePtr;
			return node;
		}

		static inline xrbnodep*	allocate_tree_node(x_iallocator* allocator)
		{
			void* nodePtr = allocator->allocate(sizeof(xrbnodep), sizeof(void*));
			xrbnodep* node = (xrbnodep*)nodePtr;
			node->clear();
			node->node = NULL;
			return node;
		}

		static inline void		init_list_node(xlnode* node, u32 offset, xlnode::EState state, xlnode* next, xlnode* prev)
		{
			node->flags = 0;
			node->set_state(state);
			node->ptr = offset;
			node->next = next;
			node->prev = prev;
		}

		static inline void		deallocate_node(xlnode* nodePtr, x_iallocator* allocator)
		{
			allocator->deallocate(nodePtr);
		}
		static inline void		deallocate_node(xrbnode* nodePtr, x_iallocator* allocator)
		{
			allocator->deallocate(nodePtr);
		}

		static inline xsize_t	get_size(xlnode* nodePtr)
		{
			xlnode* nextNodePtr = nodePtr->next;
			xsize_t const nodeSize = (nextNodePtr->ptr) - (nodePtr->ptr);
			return nodeSize;
		}

		s32		rbtree_compare_addr(void* _a, xrbnode* _b)
		{
			xlnode* a = ((xlnode*)_a);
			xlnode* b = ((xrbnodep*)_b)->node;
			if (a->ptr < b->ptr)
				return -1;
			if (a->ptr > b->ptr)
				return 1;
			return 0;
		}

		s32		rbtree_compare_size(void* _a, xrbnode* _b)
		{
			xlnode* a = ((xlnode*)_a);
			xlnode* b = ((xrbnodep*)_b)->node;
			if (a == b)
				return 0;

			xsize_t const aSize = get_size(a);
			xsize_t const bSize = get_size(b);
			if (aSize == bSize)
			{
				if (a->ptr < b->ptr)
					return -1;
			}
			else
			{
				if (aSize < bSize)
					return -1;
			}
			return 1;
		}

		void	rbtree_swap(xrbnode* to_replace, xrbnode* to_remove)
		{
			xrbnodep* p = (xrbnodep*)to_replace;
			xrbnodep* r = (xrbnodep*)to_remove;
			xlnode* tmp = p->node;
			p->node = r->node;
			r->node = tmp;
		}

		static void			insert_size(xrbtree& tree, xrbnodep* node);
		static void			insert_addr(xrbtree& tree, xrbnodep* node);

		static bool			find_bestfit(xrbtree& tree, u32 size, u32 alignment, xrbnodep*& outNode, u32& outNodeSize);
		static bool			find_size(xrbtree& tree, xlnode* node, xrbnodep*& outNode);
		static bool			find_addr(xrbtree& tree, memptr ptr, xrbnodep*& outNode);

		static xlnode*		insert_new_node_in_list(xlnode* nodep, u32 size, x_iallocator* a);
		static void			remove_from_list(xlnode* node);

		static xrbnodep*	remove_from_size_tree(xrbtree& tree, xrbnodep* node);
		static xrbnodep*	remove_from_addr_tree(xrbtree& tree, xrbnodep* node);


		u32					xlargebin::sizeof_node()
		{
			return sizeof(xrbnodep) > sizeof(xlnode) ? sizeof(xrbnodep) : sizeof(xlnode);
		}

		void				xlargebin::init(void* mem_begin, u32 mem_size, u32 size_alignment, u32 address_alignment, x_iallocator* node_allocator)
		{
			x_iallocator* a = node_allocator;

			// Allocate the root nodes for the @size and @address tree
			mNodeAllocator		= a;
			mRootSizeTree.init(rbtree_compare_size, rbtree_swap);
			mRootAddrTree.init(rbtree_compare_addr, rbtree_swap);

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

			xlnode* beginNode = allocate_list_node(a);
			xlnode* sizeNode  = allocate_list_node(a);
			xlnode* endNode   = allocate_list_node(a);

			// Link these 3 nodes into a double linked list. This list will always be ordered by the
			// address of the managed memory that is identified with the node.
			// So after n allocations this list will contain n+3 nodes where every node will be
			// pointing in managed memory increasingly higher.
			init_list_node(beginNode,        0, xlnode::STATE_USED, sizeNode , endNode  );
			init_list_node(sizeNode ,        0, xlnode::STATE_FREE, endNode  , beginNode);
			init_list_node(endNode  , mem_size, xlnode::STATE_USED, beginNode, sizeNode );

			mNodeListHead = beginNode;

			// Add the 'initial' size node to the size tree
			xrbnodep* sizeTreeNode = allocate_tree_node(a);
			sizeTreeNode->node = sizeNode;
			insert_size(mRootSizeTree, sizeTreeNode);
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
			mNodeListHead = NULL;

			{
				xrbnode* iterator = NULL;
				xrbnode* node_to_destroy = mRootSizeTree.clear(iterator);
				while (node_to_destroy != NULL)
				{
					a->deallocate(node_to_destroy);
					node_to_destroy = mRootSizeTree.clear(iterator);
				};
			}
			{
				xrbnode* iterator = NULL;
				xrbnode* node_to_destroy = mRootAddrTree.clear(iterator);
				while (node_to_destroy != NULL)
				{
					a->deallocate(node_to_destroy);
					node_to_destroy = mRootAddrTree.clear(iterator);
				};
			}

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
			xrbnodep* thisTreeNode;
			u32 nodeSize;
			if (find_bestfit(mRootSizeTree, size, alignment, thisTreeNode, nodeSize) == false)
				return NULL;

			xlnode* thisListNode = thisTreeNode->node;

			// @TODO: We still need to check that if the alignment is very big to
			//        have a new node take that wasted-space.
			thisListNode->ptr = align_ptr(thisListNode->ptr, alignment);

			// Remove 'node' from the size tree since it is not available/free anymore
			thisTreeNode = remove_from_size_tree(mRootSizeTree, thisTreeNode);
			ASSERT(thisTreeNode->node == thisListNode);

			// Check if we have to split this node, if so do it, create a new node
			// to hold the left-over size, insert it into the double linked list and
			// insert it into the size tree.
			if ((nodeSize - size) >= mSizeAlignment)
			{
				// Add to the linear list after 'thisListNode'
				xlnode* newListNode = insert_new_node_in_list(thisListNode, size, mNodeAllocator);
				if (newListNode == NULL)
					return NULL;

				// Insert this 'free' new node into the size tree
				newListNode->set_free();
				xrbnodep* newTreeNode = allocate_tree_node(mNodeAllocator);
				newTreeNode->node = newListNode;
				insert_size(mRootSizeTree, newTreeNode);
			}

			// Mark our node as used
			thisListNode->set_used();

			// Insert our alloc node into the address tree so that we can find it when
			// deallocate is called.
			thisTreeNode->clear();
			insert_addr(mRootAddrTree, thisTreeNode);

			// Done...

			return (void*)((uptr)mBaseAddress + thisListNode->ptr);
		}


		bool			xlargebin::deallocate	(void* ptr)
		{
			x_iallocator* a = mNodeAllocator;

			memptr p = (xbyte*)ptr - (xbyte*)mBaseAddress;

			// Find this address in the address-tree and remove it from there.
			xrbnodep* thisTreeNode;
			if (find_addr(mRootAddrTree, p, thisTreeNode)==false)
				return false;

			// Remove the node from the tree, it is deallocated so it has no
			// reason to be in this tree anymore.
			thisTreeNode = remove_from_addr_tree(mRootAddrTree, thisTreeNode);

			xlnode* thisListNode = thisTreeNode->node;
			xlnode* prevListNode = thisListNode->prev;
			xlnode* nextListNode = thisListNode->next;
			
			mNodeAllocator->deallocate(thisTreeNode);
			thisTreeNode = NULL;

			// Check if we can merge with our previous and next nodes, handle
			// any merge and remove any obsolete node from the size-tree.

			// Mark our node to indicate it is free now
			thisListNode->set_free();

			if (!nextListNode->is_used())
			{	// remove 'next' and deallocate it
				xrbnodep* nextTreeNode = NULL;
				find_size(mRootSizeTree, nextListNode, nextTreeNode);
				nextTreeNode = remove_from_size_tree(mRootSizeTree, nextTreeNode);
				a->deallocate(nextTreeNode);

				remove_from_list(nextListNode);
				a->deallocate(nextListNode);
				nextListNode = NULL;
			}

			// 'prev' is not used as well, so we can merge it with 'node'
			if (!prevListNode->is_used())
			{
				xrbnodep* prevTreeNode = NULL;
				find_size(mRootSizeTree, prevListNode, prevTreeNode);
				prevTreeNode = remove_from_size_tree(mRootSizeTree, prevTreeNode);

				// Remove node from the list, after that the size of prev has increased
				remove_from_list(thisListNode);
				mNodeAllocator->deallocate(thisListNode);

				// Now insert 'prev' as a tree node back into the size tree
				prevTreeNode->clear();
				ASSERT(prevTreeNode->node == prevListNode);
				insert_size(mRootSizeTree, prevTreeNode);
			}
			else
			{	// 'prev' is still used so we cannot merge, so here we have to insert node back into the size tree
				thisTreeNode = allocate_tree_node(mNodeAllocator);
				thisTreeNode->node = thisListNode;
				insert_size(mRootSizeTree, thisTreeNode);
			}

			// Done...
			return true;
		}

		s32		rbtree_compare_addr(xrbnode* _a, xrbnode* _b)
		{
			xlnode* a = ((xrbnodep*)_a)->node;
			xlnode* b = ((xrbnodep*)_b)->node;
			s32 s = (a->ptr != b->ptr);
			s = s * (((a->ptr > b->ptr) * 2) - 1);
			return s;
		}

		s32		rbtree_compare_size(xrbnode* _a, xrbnode* _b)
		{
			xlnode* a = ((xrbnodep*)_a)->node;
			xlnode* b = ((xrbnodep*)_b)->node;
			if (a == b)
				return 0;

			s32 s = 1;
			xsize_t const aSize = get_size(a);
			xsize_t const bSize = get_size(b);
			if (aSize == bSize)
			{
				s = ((a->ptr > b->ptr) * 2) - 1;
			}
			else
			{
				s = ((aSize > bSize) * 2) - 1;
			}
			return s;
		}

		// Size tree function implementations
		static void			insert_size(xrbtree& tree, xrbnodep* node)
		{
			ASSERT(node->node->is_free());
			tree.insert(node->node, node);
			const char* result = NULL;
			tree.test(rbtree_compare_size, result);
			ASSERT(result == NULL);
		}

		static bool			can_handle_size(xrbnodep* node, u32 size, u32 alignment, u32& outSize)
		{
			// Compute the 'size' that 'node' can allocate. This is done by taking
			// the 'external mem ptr' of the current node and the 'external mem ptr'
			// of the next node and substracting them.
			xsize_t const node_size = get_size(node->node);

			ASSERT(x_intu::isPowerOf2(alignment));
			u32 align_mask = alignment - 1;

			// See if we can use this block
			if (size <= node_size)
			{
				// Verify the alignment
				u32 align_shift = (u32)((uptr)node->node->ptr & align_mask);
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

		static bool			find_bestfit(xrbtree& tree, u32 size, u32 alignment, xrbnodep*& outNode, u32& outNodeSize)
		{
			if (tree.is_empty())
				return false;

			rb_iterator iterator;
			xrbnodep* curNode = (xrbnodep*)tree.get_root();

			while (curNode != NULL)
			{
				xsize_t const curSize = get_size(curNode->node);
				if (size == curSize)
				{	// We have found a node that holds the same size, we will
					// traverse the tree from here searching for the best-fit.
					break;
				}
				s32 const s = size > curSize;
				xrbnodep* child = (xrbnodep*)curNode->get_child(s);
				iterator.push(curNode);
				curNode = child;
			}

			if (curNode == NULL)
			{
				if (iterator.top > 0)
					curNode = (xrbnodep*)iterator.pop();
			}
			iterator.node = curNode;

			// Now traverse to the right until we find a block that satisfies our size and alignment.
			while (curNode != NULL)
			{
				u32 curNodeSize;
				if (can_handle_size(curNode, size, alignment, curNodeSize))
				{
					outNode     = curNode;
					outNodeSize = curNodeSize;
					return true;
				}
				curNode = (xrbnodep*)iterator.move(rb_iterator::FORWARDS);
			}
			return false;
		}

		bool			find_size(xrbtree& tree, xlnode* node, xrbnodep*& outNode)
		{
			xsize_t const nodeSize = get_size(node);

			xrbnodep* curNode = (xrbnodep*)tree.get_root();
			while (curNode != NULL)
			{
				xlnode* cur = curNode->node;
				if (cur == node)
				{
					outNode = curNode;
					return true;
				}

				xsize_t const curSize = get_size(cur);

				s32 s;
				if (curSize == nodeSize)
				{
					s = ((node->ptr > cur->ptr) * 2) - 1;
				}
				else
				{
					s = ((nodeSize > curSize) * 2) - 1;
				}

				curNode = (xrbnodep*)curNode->get_child(s);
			}
			return false;
		}

		s32		rbtree_compare_addr2(xrbnode* _a, xrbnode* _b)
		{
			return rbtree_compare_addr(((xrbnodep*)_a)->node, _b);
		}

		void			insert_addr(xrbtree& tree, xrbnodep* node)
		{
			ASSERT(node->node->is_used());
			tree.insert(node->node, node);
			const char* result = NULL;
			tree.test(rbtree_compare_addr2, result);
			ASSERT(result == NULL);
		}

		bool			find_addr(xrbtree& tree, memptr ptr, xrbnodep*& outNode)
		{
			xrbnodep* curNode = (xrbnodep*)tree.get_root();
			while (curNode != NULL)
			{
				memptr cptr = curNode->node->ptr;
				if (ptr == cptr)
				{
					outNode = curNode;
					return true;
				}
				s32 const s = ptr > cptr;
				curNode  = (xrbnodep*)curNode->get_child(s);
			}
			return false;
		}

		xlnode*		insert_new_node_in_list(xlnode* node, u32 size, x_iallocator* a)
		{
			xlnode* newNode = allocate_list_node(a);
			if (newNode == NULL)
				return NULL;

			init_list_node(newNode, advance_ptr(node->ptr, size), xlnode::STATE_USED, node->next, node);

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

		static xrbnodep*	remove_from_size_tree(xrbtree& tree, xrbnodep* node)
		{
			xrbnode* o = NULL;
			tree.remove(node->node, o);
			const char* result = NULL;
			tree.test(rbtree_compare_size, result);
			ASSERT(result == NULL);
			return (xrbnodep*)o;
		}

		static xrbnodep*	remove_from_addr_tree(xrbtree& tree, xrbnodep* node)
		{
			xrbnode* o = NULL;
			tree.remove(node->node, o);
			const char* result = NULL;
			tree.test(rbtree_compare_addr2, result);
			ASSERT(result == NULL);
			return (xrbnodep*)o;
		}
	}
}