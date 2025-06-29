#ifndef __CALLOCATOR_SEGMENTED_H__
#define __CALLOCATOR_SEGMENTED_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "cbase/c_allocator.h"

namespace ncore
{
    namespace nsegmented
    {
        // A segmented allocator that allocates memory in segments of 2^N
        template <typename T> struct allocator_t
        {
            static const T c_null = ~0;
            typedef T      node_t;

            u8*     m_node_size;           // Size is '1 << (m_node_size[size_index] & 0x7F)', if the MSB is set the node is allocated
            node_t* m_node_next;           // Next node in the size list
            node_t* m_node_prev;           // Previous node in the size list
            u64     m_size_list_occupancy; // Which size list is non-empty
            node_t* m_size_lists;          // Every size has its own list
            u8      m_node_count_2log;     // 16, 15, 14, 13, 12, 11, 10
            u8      m_max_span_2log;

            void setup(alloc_t* allocator, u8 node_count_2log, u8 max_span_2log);
            void teardown(alloc_t* allocator);

            bool allocate(u32 size, node_t& out_node); // Returns false if the size is not available
            bool deallocate(node_t node);              // Deallocates the node

            bool split(node_t node); // Splits the node into two nodes

            void add_size(u8 size_index, node_t node);
            void remove_size(u8 size_index, node_t node);
        };

        template <typename T> void allocator_t<T>::setup(alloc_t* allocator, u8 node_count_2log, u8 max_span_2log)
        {
            m_node_count_2log    = node_count_2log;
            m_max_span_2log      = max_span_2log;
            u32 const node_count = 1 << node_count_2log;
            m_node_size          = g_allocate_array_and_clear<u8>(allocator, node_count);
            m_node_next          = g_allocate_array_and_clear<node_t>(allocator, node_count);
            m_node_prev          = g_allocate_array_and_clear<node_t>(allocator, node_count);

            u32 const size_list_count = m_node_count_2log + 1;
            m_size_lists              = g_allocate_array_and_memset<node_t>(allocator, size_list_count, 0xFFFFFFFF);
            m_size_list_occupancy     = 0;

            if (m_max_span_2log < m_node_count_2log)
            {

                u8 const span = m_max_span_2log;
                // Split the full range until we reach the maximum span
                for (u32 n = 0; n < node_count; n += (1 << span))
                {
                    m_node_size[n] = span;
                    add_size(m_max_span_2log, n);
                }
            }
            else
            {
                m_node_size[0] = m_node_count_2log;
                add_size(m_node_count_2log, 0);
            }
        }

        template <typename T> void allocator_t<T>::teardown(alloc_t* allocator)
        {
            allocator->deallocate(m_node_size);
            allocator->deallocate(m_node_next);
            allocator->deallocate(m_node_prev);
            allocator->deallocate(m_size_lists);
        }

        template <typename T> bool allocator_t<T>::split(node_t node)
        {
            const u8 span = m_node_size[node];
            remove_size(span, node);

            // Split the node into two nodes
            node_t const insert = node + (1 << (span - 1));
            m_node_size[node]   = span - 1;
            m_node_size[insert] = span - 1;

            // Invalidate the next and previous pointers of the new node (debugging)
            m_node_next[insert] = (node_t)c_null;
            m_node_prev[insert] = (node_t)c_null;

            // Add node and insert into the size list that is (index - 1)
            add_size(span - 1, node);
            add_size(span - 1, insert);

            return true;
        }

        template <typename T> void allocator_t<T>::add_size(u8 index, node_t node)
        {
            node_t& head = m_size_lists[index];
            if (head == (node_t)c_null)
            {
                head              = node;
                m_node_next[node] = node;
                m_node_prev[node] = node;
                m_size_list_occupancy |= ((u64)1 << index);
                return;
            }

            node_t const prev = m_node_prev[head];
            m_node_next[prev] = node;
            m_node_prev[head] = node;
            m_node_next[node] = head;
            m_node_prev[node] = prev;

            m_size_list_occupancy |= ((u64)1 << index);
        }

        template <typename T> void allocator_t<T>::remove_size(u8 index, node_t node)
        {
            ASSERT(index >= 0 && index <= (m_node_count_2log + 1));
            node_t& head                   = m_size_lists[index];
            head                           = (head == node) ? m_node_next[node] : head;
            head                           = (head == node) ? (node_t)c_null : head;
            m_node_next[m_node_prev[node]] = m_node_next[node];
            m_node_prev[m_node_next[node]] = m_node_prev[node];
            m_node_next[node]              = (node_t)c_null;
            m_node_prev[node]              = (node_t)c_null;

            if (head == (node_t)c_null)
                m_size_list_occupancy &= ~((u64)1 << index);
        }

        template <typename T> bool allocator_t<T>::allocate(u32 size, node_t& out_node)
        {
            u8 const span  = math::g_ilog2(size);
            u8 const index = span;

            node_t node = m_size_lists[index];
            if (node == (node_t)c_null)
            {
                // In the occupancy find a bit set that is above our size
                // Mask out the bits below size and then do a find first bit
                u64 const occupancy = m_size_list_occupancy & ~(((u64)1 << index) - 1);
                if (occupancy == 0) // There are no free sizes available
                    return false;

                u8 occ = math::g_findFirstBit(occupancy);
                node   = m_size_lists[occ];
                while (occ > index)
                {
                    split(node); // Split the node until we reach the size
                    occ -= 1;
                }
            }

            remove_size(index, node);

            m_node_size[node] |= 0x80; // Mark the node as allocated

            out_node = node;
            return true;
        }

        template <typename T> bool allocator_t<T>::deallocate(node_t node)
        {
            if (node == c_null || node >= (1 << m_node_count_2log))
                return false;

            m_node_size[node] &= ~0x80; // Mark the node as free

            u8 span  = m_node_size[node];
            u8 index = span;

            // Keep span smaller or equal to the largest span allowed
            while (span < m_max_span_2log)
            {
                // Could be that we are freeing the largest size allocation, this means
                // that we should not merge the node with its sibling.
                if (index == m_node_count_2log)
                    break;

                const node_t sibling = ((node & ((2 << span) - 1)) == 0) ? node + (1 << span) : node - (1 << span);

                // Check if the sibling is free
                if ((m_node_size[sibling] & 0x80) == 0x80)
                    break;

                remove_size(index, sibling);                           // Sibling is being merged, remove it
                node_t const merged = node < sibling ? node : sibling; // Always take the left node as the merged node
                node_t const other  = node < sibling ? sibling : node; // Always take the right node as the other node
                m_node_size[merged] = span + 1;                        // Double the span (size) of the node
                m_node_size[other]  = 0;                               // Invalidate the node that is merged into 'merged'

                node  = merged;
                span  = m_node_size[node] & 0x7F;
                index = span;
            }

            add_size(index, node); // Add the merged node to the size list

            return true;
        }
    } // namespace nsegmented
} // namespace ncore

#endif
