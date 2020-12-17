#ifndef __X_FSA_H__
#define __X_FSA_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

namespace xcore
{
    namespace fsa_t
    {
        // Note: There are contraints with a page:
        //       - 0xffff is the Null index
        //       - Page Size / Alloc Size <= 0xfffe / 65534

        // Some OK configurations:
        // Page Size = 64 KB, Min Size Alloc = 4
        // Page Size =  2 MB, Min Size Alloc = 32

        struct page_t
        {          
            enum
            {
                Null = 0xffff
            };

            // Smallest alloc size is '4 bytes'!
            // Alloc size is aligned up on '4 bytes'
            inline page_t() : m_allocs(0), m_head(0) {}

            void init(void* page_addr, u32 page_size, u32 alloc_size)
            {
                m_allocs            = 0;
                m_head              = 0;
                u32 const node_size = (alloc_size + 3) / 4;
                u32 const count     = (page_size / 4) / node_size;
                u16*      node      = (u16*)page_addr;
                for (u32 i = 0; i < count; ++i)
                {
                    node[0] = (i + 1);
                    node += node_size;
                }
                node            = (u16*)page_addr;
                node[count - 1] = Null;
            }

            void reset()
            {
                m_allocs = 0;
                m_head   = Null;
            }

            bool empty() const { return m_allocs == 0; }
            bool full() const { return m_head == Null; }

            void* allocate(void* page_addr, u32 alloc_size)
            {
                ASSERT(full() == false);
                u16* node = (u16*)((xbyte*)page_addr + m_head*alloc_size);
                m_head    = node[0];
                m_allocs += 1;
                return (void*)node;
            }

            // Make sure this is the correct page!
            void deallocate(void* alloc_addr, void* page_addr, u32 alloc_size)
            {
                u16* node = (u16*)alloc_addr;
                node[0]   = m_head;
                m_head    = (u16)(((uptr)alloc_addr - (uptr)page_addr) / alloc_size);
            }

            u16 m_allocs; // Number of allocations done on this page
            u16 m_head;   // The head of the free list (item = page-address + m_head)
        };

    }; // namespace fsa_t

}; // namespace xcore

#endif /// __X_FSA_H__
