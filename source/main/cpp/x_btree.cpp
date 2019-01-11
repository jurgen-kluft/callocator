#include "xbase/x_target.h"
#include "xbase/x_debug.h"

#include "xallocator/private/x_btree.h"

namespace xcore
{
    namespace btree
    {
        class vmalloc : public alloc
        {
            enum
            {
                Null32 = 0xffffffff,
                Null16 = 0xffff,
            };

			// Note: Keep bit 31 free! So only use bit 0-30 (31 bits)
            inline u32 page_index(u32 index) const { return (index >> 13) & 0x1FFFF; }
            inline u32 item_index(u32 index) const { return (index >> 0) & 0x1FFF; }
            inline u32 to_index(u32 page_index, u32 item_index) { return (page_index << 13) | (item_index & 0x1FFF); }

        public:
            virtual void* allocate(u32& index)
            {
                u32 page_index;
                if (!m_notfull_used_pages.find(page_index))
                {
                    if (!alloc_page(page_index))
                    {
                        index = Null32;
                        return nullptr;
                    }
                    m_notfull_used_pages.set(page_index);
                }

                page_t* page = get_page(page_index);

                u32   i;
                void* ptr = page->allocate(i);
                if (page->m_alloc_count == m_page_max_alloc_count)
                { // Page is full
                    m_notfull_used_pages.clr(page_index);
                }

                index = ptr2idx(ptr);
                return ptr;
            }

            virtual void deallocate(u32 i)
            {
                page_t* page = get_page(page_index(i));
                page->deallocate(item_index(i));
                if (page->m_alloc_count == 0)
                {
                    dealloc_page(page_index);
                }
            }

            virtual void* idx2ptr(u32 i) const
            {
                page_t* page = get_page(page_index(i));
                return page->idx2ptr(item_index(i), m_alloc_size);
            }

            virtual u32 ptr2idx(void* ptr) const
            {
                u32     page_index = ((uptr)ptr - (uptr)m_base_addr) / m_page_size;
                page_t* page       = get_page(page_index);
                u32     item_index = page->ptr2idx(ptr, m_alloc_size);
                return to_index(page_index, item_index);
            }

            // Every used page has it's first 'alloc size' used for book-keeping
            // sizeof == 8 bytes
            struct page_t
            {
                void init(u32 alloc_size)
                {
                    m_list_head  = Null16;
                    m_alloc_cnt  = 0;
                    m_alloc_size = alloc_size;
                    m_alloc_ptr  = alloc_size * 1;
                    // We do not initialize a free-list, instead we allocate from
                    // a 'pointer' which grows until we reach 'max alloc count'.
                    // After that allocation happens from m_list_head.
                }

                void* allocate(u32& item_index)
                {
                    if (m_list_head != Null16)
                    { // Take it from the list
                        item_index  = m_list_head;
                        m_list_head = ((u16*)this)[m_list_head >> 1];
                    }
                    else
                    {
                        item_index = m_alloc_ptr / m_alloc_size;
                        m_alloc_ptr += m_alloc_size;
                    }

                    m_alloc_cnt += 1;
                    return (void*)((uptr)this + (item_index * m_alloc_size));
                }

                void deallocate(u32 item_index)
                {
                    ((u16*)this)[item_index >> 1] = m_list_head;
                    m_list_head                   = item_index;
                    m_alloc_cnt -= 1;
                }

                void* idx2ptr(u32 index) const
                {
                    void* ptr = (void*)((uptr)this + (index * m_alloc_size));
                    return ptr;
                }

                u32 ptr2idx(void* ptr) const
                {
                    u32 item_index = ((uptr)ptr - (uptr)this) / m_alloc_size;
                    return item_index;
                }

                u16 m_list_head;
                u16 m_alloc_cnt;
                u16 m_alloc_size;
                u16 m_alloc_ptr;
            };

            page_t* get_page(u32 page_index)
            {
                page_t* page = (page_t*)((uptr)m_base_addr + page_index * m_page_size);
                return page;
            }

            bool alloc_page(u32& page_index)
            {
                u32 freepage_index;
                return m_notused_free_pages.find(freepage_index);
            }

            void dealloc_page(u32 page_index)
            {
                page_t* page = get_page(page_index);
                m_vmem->decommit((void*)page, m_page_size, 1);
                m_notused_free_pages.set(page_index);
            }

            void init(xalloc* a, xvirtual_memory* vmem, u32 alloc_size, u64 addr_range, u32 page_size)
            {
                m_alloc                = a;
                m_vmem                 = vmem;
                m_page_size            = page_size;
                m_page_max_count       = addr_range / page_size;
                m_page_max_alloc_count = (page_size / alloc_size) - 1;
				xheap heap(a);
				m_notfull_used_pages.init(heap, m_page_max_count, false, true);
				m_notused_free_pages.init(heap, m_page_max_count, true, true);
            }

            virtual void release()
            {
                // deallocate any allocated resources
                // decommit all used pages
                // release virtual memory

				xheap heap(a);
				m_notfull_used_pages.release(heap);
				m_notused_free_pages.release(heap);
            }

            xalloc*          m_alloc;
            xvirtual_memory* m_vmem;
            void*            m_base_addr;
            u32              m_page_size;
            s32              m_page_max_count;
            u32              m_page_max_alloc_count;
            xbitlist         m_notfull_used_pages;
            xbitlist         m_notused_free_pages;
        };

        alloc* gCreateVMemPageAllocator(xalloc* a, xvirtual_memory* vmem, u32 alloc_size, u64 addr_range, u32 page_size)
        {
            xheap    heap(a);
            vmalloc* allocator = heap.construct<vmalloc>();
            allocator->init(a, vmem, alloc_size, addr_range, page_size);
            return allocator;
        }

    } // namespace btree

	void btree_t::init(btree::indexer* indexer, btree::allocator* node_allocator, btree::allocator* leaf_allocator)
	{
		m_idxr = indexer;
		m_node_allocator = node_allocator;
		m_leaf_allocator = leaf_allocator;
	}

    bool btree_t::add(u32 value, btree::index& leaf_index)
    {
        s32          level      = 0;
        btree::node* parentNode = &m_root;
        do
        {
            s32          childIndex     = m_idxr.get_index(value, level);
            btree::index childNodeIndex = parentNode->m_nodes[childIndex];
            if (childNodeIndex.is_null())
            {
                // No brainer, create leaf and insert child
                btree::leaf* leafNode           = m_leaf_allocator->allocate(leaf_index);
                leafNode->m_value               = value;
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
                btree::node* newChildNode       = m_node_allocator->allocate(newChildNodeIndex);
                parentNode->m_nodes[childIndex] = newChildNodeIndex;

                // Compute the child index of this already existing leaf one level up
                // and insert this existing leaf there.
                s32 const childChildIndex              = m_idxr.get_index(leaf->m_value, level + 1);
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
            btree::node*	set_child_as_nill)
            {
                m_node->m_nodes[m_index] = btree::index();
                return m_node;
            }
            btree::node* m_node;
            s32          m_index;
        };
        level_t branch[32];

        s32          level = 0;
        btree::node* node  = &m_root;
        do
        {
            s32          childIndex = m_idxr.get_index(value, level);
            btree::index nodeIndex  = node->m_nodes[childIndex];
            branch[level]           = level_t(node, childIndex);
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
        s32          level      = 0;
        btree::node* parentNode = &m_root;
        do
        {
            s32          childIndex     = m_idxr.get_index(value, level);
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

} // namespace xcore