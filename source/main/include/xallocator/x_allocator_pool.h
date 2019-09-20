#ifndef __X_POOL_ALLOCATOR_H__
#define __X_POOL_ALLOCATOR_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

namespace xcore
{
    /// Forward declares
    class xalloc;
    class xfsalloc;

    struct xpool
    {
        xpool()
            : m_sizeof_element(sizeof(void*))
			, m_alignof_element(sizeof(void*))
			, m_minimum_numberof_blocks(1)
			, m_maximum_numberof_blocks(256)
			, m_block_allocator(nullptr) {}

		xfsalloc* create(xalloc * main_allocator);

        void set_elem_size(u32 size);
        void set_elem_alignment(u32 alignment);
        void set_block_min_count(u32 min_num_blocks);
        void set_block_max_count(u32 max_num_blocks);
        void set_block_allocator(xalloc* block_allocator);

        u32       get_elem_size() const;
        u32       get_elem_alignment() const;
        u32       get_block_min_count() const;
        u32       get_block_max_count() const;
        xfsalloc* get_block_allocator() const;

    private:
        u32       m_sizeof_element;
        u32       m_alignof_element;
        u32       m_minimum_numberof_blocks;
        u32       m_maximum_numberof_blocks;
        xalloc*   m_block_allocator;
    };

}; // namespace xcore

#endif /// __X_POOL_ALLOCATOR_H__
