#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_integer.h"
#include "xbase/x_tree.h"
#include "xallocator/private/x_largebin.h"

namespace xcore
{
	namespace xexternal32
	{
		/*
		Large block allocator

		This allocator uses external book keeping data to manage the 'external memory'. This means that additional memory
		is used for data that contains meta-data about the allocated and free blocks of the 'external memory'.

		A red-black tree is used to quickly identify a suitable (best-fit) block for allocation, while the deallocation
		code path is using an additional red-black tree for quickly finding the allocated block so as to release it back
		into the red-black tree of free blocks.
		*/

		typedef	u32				memptr;

		static inline memptr	advance_ptr(memptr ptr, u32 size) { return (memptr)(ptr + size); }
		static inline memptr	align_ptr(memptr ptr, u32 alignment) { return (memptr)((ptr + (alignment - 1)) & ~((memptr)alignment - 1)); }
		static xsize_t			diff_ptr(memptr ptr, memptr next_ptr) { return (xsize_t)(next_ptr - ptr); }

		struct xlnode
		{
			xlnode() : flags(0), next(0), prev(0), ptr(0) {}

			enum EState { STATE_USED = 4, STATE_FREE = 0 };

			void			clear()
			{
				flags = 0;
				next = nullptr;
				prev = nullptr;
				ptr = 0;
			}

			void			set_state(EState state) { flags = (flags & ~STATE_USED) | state; }

			void			set_free() { flags = (flags & ~STATE_USED); }
			void			set_used() { flags = (flags & ~STATE_USED) | STATE_USED; }

			bool			is_free() const { return (flags & STATE_USED) == STATE_FREE; }
			bool			is_used() const { return (flags & STATE_USED) == STATE_USED; }

			u32				flags;
			xlnode*			next;						// (4/8) linear list ordered by physical address
			xlnode*			prev;						// (4/8)  
			memptr			ptr;						// (8) pointer/offset in external memory
		};

		static inline xlnode*	allocate_node(xalloc* allocator)
		{
			void* nodePtr = allocator->allocate(sizeof(xlnode), sizeof(void*));
			xlnode* node = (xlnode*)nodePtr;
			return node;
		}

		static inline void		init_node(xlnode* node, memptr offset, xlnode::EState state, xlnode* next, xlnode* prev)
		{
			node->clear();
			node->set_state(state);
			node->next = next;
			node->prev = prev;
			node->ptr = offset;
		}

		static inline void		deallocate_node(xalloc* allocator, xlnode* nodePtr)
		{
			allocator->deallocate(nodePtr);
		}

		static inline xsize_t	get_size(xlnode const* nodePtr)
		{
			xlnode const* nextNodePtr = (nodePtr->next);
			xsize_t const nodeSize = (nextNodePtr->ptr) - (nodePtr->ptr);
			return nodeSize;
		}
		static void			insert_size(xtree& tree, xlnode* node, xalloc* node_allocator);
		static bool			find_bestfit(xtree& tree, u32 size, u32 alignment, xalloc* a, xlnode *& outNode, u32& outNodeSize);

		static void			insert_addr(xtree& tree, xlnode* node, xalloc* node_allocator);
		static bool			find_addr(xtree& tree, memptr ptr, xalloc* a, xlnode *& outNode);

		static xlnode*		insert_in_list(xlnode* nodep, u32 size, xalloc* a, xlnode*& newNode);
		static void			remove_from_list(xlnode* node, xalloc* a);
		static void			remove_from_tree(xtree& tree, xlnode* node, xalloc* a);

		s32					compare_addr(void const* p1, void const* p2)
		{
			xlnode const* n1 = (xlnode const*)p1;
			xlnode const* n2 = (xlnode const*)p2;
			if (n1->ptr < n2->ptr)
				return -1;
			if (n1->ptr > n2->ptr)
				return 1;
			return 0;
		}

		s32					compare_size(void const* p1, void const* p2)
		{
			xlnode const* n1 = (xlnode const*)p1;
			xlnode const* n2 = (xlnode const*)p2;
			xsize_t s1 = get_size(n1);
			xsize_t s2 = get_size(n2);
			if (s1 < s2)
				return -1;
			if (s1 > s2)
				return 1;
			// Size is equal, now compare addresses
			if (n1->ptr < n2->ptr)
				return -1;
			if (n1->ptr > n2->ptr)
				return 1;

			// This should actually never happen
			return 0;
		}

		xlargebin::xlargebin()
			: mNodeAllocator(nullptr)
			, mRootSizeTree(nullptr)
			, mRootAddrTree(nullptr)
			, mBaseAddress(nullptr)
			, mNodeListHead(nullptr)
			, mSizeAlignment(256)
			, mAddressAlignment(8)
		{
		}

		void				xlargebin::init(void* mem_begin, u32 mem_size, u32 size_alignment, u32 address_alignment, xalloc* node_allocator)
		{
			xalloc* a = node_allocator;

			// Allocate the root nodes for the @size and @address tree
			mNodeAllocator = a;
			mRootSizeTree = xtree(a);
			mRootAddrTree = xtree(a);

			mRootSizeTree.set_cmp(compare_size);
			mRootAddrTree.set_cmp(compare_addr);

			mBaseAddress = mem_begin;
			mNodeListHead = nullptr;
			mSizeAlignment = size_alignment;
			mAddressAlignment = address_alignment;

			// The initial tree will contain 3 nodes, 2 nodes that define
			// the span of the managed memory (begin - end). These 2 nodes
			// are always marked USED, so the allocation code will never
			// touch them. They are there to make the logic simpler. When
			// the tree is empty only these 2 nodes are left and the logic
			// will figure out that there is no free memory.

			xlnode* beginNode = allocate_node(a);
			xlnode* sizeNode = allocate_node(a);
			xlnode* endNode = allocate_node(a);

			// Link these 3 nodes into a double linked list. This list will always be ordered by the
			// address of the managed memory that is identified with the node.
			// So after n allocations this list will contain n+3 nodes where every node will be
			// pointing in managed memory increasingly higher.
			init_node(beginNode, 0, xlnode::STATE_USED, sizeNode, endNode);
			init_node(sizeNode, 0, xlnode::STATE_FREE, endNode, beginNode);
			init_node(endNode, mem_size, xlnode::STATE_USED, beginNode, sizeNode);

			mNodeListHead = beginNode;

			// Add the 'initial' size node to the size tree
			insert_size(mRootSizeTree, sizeNode, a);

		}

		void				xlargebin::release()
		{
			xalloc* a = mNodeAllocator;

			void* node;
			while (!mRootSizeTree.clear(node))
			{
				a->deallocate(node);
			}
			while (!mRootAddrTree.clear(node))
			{
				a->deallocate(node);
			}

			mNodeListHead = nullptr;
			mSizeAlignment = 0xffffffff;
			mAddressAlignment = 0xffffffff;
		}

		void*				xlargebin::allocate(u32 size, u32 alignment)
		{
			xalloc* a = mNodeAllocator;

			// Align the size up with 'mSizeAlignment'
			// Align the alignment up with 'mAddressAlignment'
			size = xalignUp(size, mSizeAlignment);
			alignment = xmax(alignment, mAddressAlignment);

			// Find the first node in the size tree that has the same or larger size
			// Start to iterate through the tree until we find a node that can hold
			// our size+alignment.
			xlnode* nodep;
			u32 nodeSize;
			if (find_bestfit(mRootSizeTree, size, alignment, a, nodep, nodeSize) == false)
				return NULL;

			// @TODO: We still need to check that if the alignment is very big to
			//        have a new node take that wasted-space.
			nodep->ptr = align_ptr(nodep->ptr, alignment);

			// Check if we have to split this node, if so do it, create a new node
			// to hold the left-over size, insert it into the double linked list and
			// insert it into the size tree.
			if ((nodeSize - size) >= mSizeAlignment)
			{
				// Add to the linear list after 'node'
				xlnode* newNode;
				xlnode* newNodep = insert_in_list(nodep, size, a, newNode);
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
			remove_from_tree(mRootSizeTree, nodep, a);

			// Insert our alloc node into the address tree so that we can find it when
			// deallocate is called.
			insert_addr(mRootAddrTree, nodep, a);

			// Done...

			return (void*)((uptr)mBaseAddress + nodep->ptr);
		}


		void				xlargebin::deallocate(void* ptr)
		{
			xalloc* a = mNodeAllocator;

			memptr p = (memptr)((uptr)ptr - (uptr)mBaseAddress);

			// Find this address in the address-tree and remove it from there.
			xlnode* nodep;
			if (find_addr(mRootAddrTree, p, a, nodep) == false)
				return;

			// Remove the node from the tree, it is deallocated so it has no
			// reason to be in this tree anymore.
			remove_from_tree(mRootAddrTree, nodep, a);

			xlnode* nextp = nodep->next;
			xlnode* prevp = nodep->prev;

			// Check if we can merge with our previous and next nodes, handle
			// any merge and remove any obsolete node from the size-tree.
			bool prev_used = prevp->is_used();
			bool next_used = nextp->is_used();

			// mark our node to indicate it is free now
			nodep->set_free();

			if (!prev_used)
				remove_from_list(nodep, a);

			if (!next_used)
			{
				remove_from_list(nextp, a);
				remove_from_tree(mRootSizeTree, nextp, a);

				// next has been merged so we can deallocate it
				a->deallocate(nextp);
			}

			// The size of 'prev' has changed if we have merged, so we need to pull
			// out the 'prev' node from the size tree and re-insert it.
			if (!prev_used)
			{
				remove_from_tree(mRootSizeTree, prevp, a);
				insert_size(mRootSizeTree, prevp, a);

				// node has been merged with prev so it is not needed anymore
				a->deallocate(nodep);
			}
			else
			{
				// Insert node into the size tree
				insert_size(mRootSizeTree, nodep, a);
			}

			// Done...
		}

		// Size tree function implementations
		static void			insert_size(xtree& tree, xlnode* nodePtr, xalloc* a)
		{
			ASSERT(nodePtr->is_free());

			xlnode* nextNodePtr = nodePtr->next;
			xsize_t const nodeSize = diff_ptr(nodePtr->ptr, nextNodePtr->ptr);

			tree.insert(nodePtr);
		}

		static bool	can_handle_size(xlnode* nodep, u32 size, u32 alignment, xalloc* a, u32& outSize)
		{
			// Convert index to node ptr
			xlnode* nextp = nodep->next;

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
				if (align_shift != 0)
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

		static bool			find_bestfit(xtree& tree, u32 size, u32 alignment, xalloc* a, xlnode*& outNode, u32& outNodeSize)
		{
			xtree::iterator iter;
			tree.iterate(iter);

			xlnode* currNode = nullptr;
			s32 d = 0;
			void* data;
			while (iter.traverse(d, data))
			{
				currNode = (xlnode*)data;
				xsize_t const curSize = diff_ptr(currNode->ptr, currNode->next->ptr);

				if (size < curSize)
				{
					d = xtree::cLeft;
				}
				else if (size > curSize)
				{
					d = xtree::cRight;
				}
				else
				{
					break;
				}
			}

			if (d == xtree::cRight)
			{
				// Get the successor since our size is bigger than the size
				// that the lastNode is holding
				iter.sortorder(xtree::cRight, data);
				currNode = (xlnode*)data;
			}

			// Now traverse to the right until we find a block that satisfies our
			// size and alignment.
			while (currNode != nullptr)
			{
				u32 curNodeSize;
				if (can_handle_size(currNode, size, alignment, a, curNodeSize))
				{
					outNode = currNode;
					outNodeSize = curNodeSize;
					return true;
				}

				iter.sortorder(xtree::cRight, data);
				currNode = (xlnode*)data;
			}
			return false;
		}

		void			insert_addr(xtree& tree, xlnode* node, xalloc* a)
		{
			ASSERT(node->is_used());
			tree.insert(node);
		}

		bool			find_addr(xtree& tree, memptr ptr, xalloc* a, xlnode*& outNode)
		{
			xlnode fnode;
			fnode.ptr = ptr;
			void * found;
			if (tree.find(&fnode, found))
			{
				// We found the memory address 'ptr' in the address tree
				outNode = (xlnode*)found;
				return true;
			}
			return false;
		}

		xlnode*			insert_in_list(xlnode* nodep, u32 size, xalloc* a, xlnode*& newNode)
		{
			{
				newNode = (xlnode*)a->allocate(sizeof(xlnode), sizeof(void*));
				if (newNode == NULL)
					return NULL;
			}

			newNode->clear();
			newNode->next = nodep->next;
			newNode->prev = nodep;
			newNode->ptr = advance_ptr(nodep->ptr, size);

			xlnode* nextp = nodep->next;
			nextp->prev = newNode;
			nodep->next = newNode;

			return newNode;
		}

		void			remove_from_list(xlnode* nodep, xalloc* a)
		{
			// The node to remove from the linear list
			xlnode* nextp = nodep->next;
			xlnode* prevp = nodep->prev;
			prevp->next = nodep->next;
			nextp->prev = nodep->prev;
		}


		static void		remove_from_tree(xtree& tree, xlnode* node, xalloc* a)
		{
			tree.remove(node);
		}
	}
}