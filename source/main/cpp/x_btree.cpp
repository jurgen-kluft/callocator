#include "xbase/x_target.h"
#include "xbase/x_debug.h"

#include "xallocator/private/x_btree.h"

namespace xcore
{
	class allocator
	{
	public:
		virtual void*	allocate(index& index) = 0;
		virtual void	deallocate(index index) = 0;
		virtual void*	idx2ptr(index index) const = 0;
		virtual index	ptr2idx(void* ptr) const = 0;
	};


	bool btree_t::add(u32 value, btree::index& leaf_index)
	{
		s32 level = 0;
		btree::node* parentNode = &m_root;
		do
		{
			s32 childIndex = m_idxr.get_index(value, level);
			btree::index childNodeIndex = parentNode->m_nodes[childIndex];
			if (childNodeIndex.is_null())
			{
				// No brainer, create leaf and insert child
				btree::leaf* leafNode = m_leaf_allocator->allocate(leaf_index);
				leafNode->m_value = value;
				parentNode->m_nodes[childIndex] = leaf_index;
				return true;
			}
			else if (childNodeIndex.is_leaf())
			{
				// Check for duplicate, see if this value is the value of this leaf
				btree::leaf* leaf = (btree::leaf*)m_leaf_allocator->idx2ptr(childNodeIndex);
				if (leaf->m_value == value)
				{
					leaf_index = childNodeIndex;
					return false;
				}

				// Create new node and add the existing item first and continue
				btree::index newChildNodeIndex;
				btree::node* newChildNode = m_node_allocator->allocate(newChildNodeIndex);
				parentNode->m_nodes[childIndex] = newChildNodeIndex;

				// Compute the child index of this already existing leaf one level up
				// and insert this existing leaf there.
				s32 const childChildIndex = m_idxr.get_index(leaf->m_value, level + 1);
				newChildNode->m_nodes[childChildIndex] = childNodeIndex;

				// Continue, stay at the same level, so that the above code will
				// get this newly created node as the leaf node.
			}
			else if (childNodeIndex.is_node())
			{
				// Continue traversing the tree until we find a null-child to place
				// our value.
				parentNode = (btree::node*)m_node_allocator->idx2ptr(childNodeIndex);
				level++;
			}
		} while (level < m_idxr.max_levels());

		leaf_index.reset();
		return false;
	}


	bool BTree::remove(u32 value)
	{
		struct level_t
		{
			level_t(btree::node* n, s32 i) : m_node(n), m_index(i) {}
			btree::node*	set_child_as_nill) {
				m_node->m_nodes[m_index] = btree::index();
				return m_node;
			}
			btree::node*	m_node;
			s32		m_index;
		};
		level_t branch[32];

		s32 level = 0;
		btree::node* node = &m_root;
		do
		{
			s32 childIndex = m_idxr.get_index(value, level);
			btree::index nodeIndex = node->m_nodes[childIndex];
			branch[level] = level_t(node, childIndex);
			if (nodeIndex.is_null())
			{
				break;
			}
			else if (nodeIndex.is_leaf())
			{
				btree::leaf* leaf = (btree::leaf*)m_leaf_allocator->idx2ptr(nodeIndex);
				if (leaf->m_value == value)
				{
					// We should track the all parents
					// Remove this leaf from node, if this results in node
					// having no more children then this node should also be
					// removed etc...

					// Null out the leaf from this node
					node->m_nodes[childIndex] = btree::index();

					// After removing the leaf from the current node we now go
					// into a loop to remove empty nodes. 
					// Back-traverse until we reach a node that after removal of
					// a child is not empty.
					while (node->is_empty()) 
					{
						m_node_allocator->deallocate(node);
						level -= 1;
						node = branch[level].set_child_as_nill();
					}
				}
				break;
			}
			else if (nodeIndex.is_node())
			{
				// Continue
				node = (btree::node*)m_node_allocator->idx2ptr(nodeIndex);				
			}
			level++;
		} while (level < m_idxr.max_levels());

		leaf_index.reset();
		return false;
	}

	bool BTree::find(u32 value, btree::index& leaf_index) const
	{
		s32 level = 0;
		btree::node* parentNode = &m_root;
		do
		{
			s32 childIndex = m_idxr.get_index(value, level);
			btree::index childNodeIndex = parentNode->m_nodes[childIndex];
			if (childNodeIndex.is_null())
			{
				break;
			}
			else if (childNodeIndex.is_leaf())
			{
				btree::leaf* leaf = (btree::leaf*)m_leaf_allocator->idx2ptr(childNodeIndex);
				if (leaf->m_value == value)
				{
					leaf_index = childNodeIndex;
					return true;
				}
				break;
			}
			else if (childNodeIndex.is_node())
			{
				// Continue
				parentNode = (btree::node*)m_node_allocator->idx2ptr(childNodeIndex);
			}
			level++;
		} while (level < m_idxr.max_levels());

		leaf_index.reset();
		return false;
	}

}