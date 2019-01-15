#ifndef __X_VIRTUAL_MEMORY_FSA_ALLOCATOR_H__
#define __X_VIRTUAL_MEMORY_FSA_ALLOCATOR_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase/x_allocator.h"
#include "xallocator/x_virtual_memory.h"

namespace xcore
{
	class xfsalloc;

	struct xvfsa_params
	{
		xvfsa_params()
			: m_alloc_size(8)
			, m_page_size(65536)											// The page size
			, m_addr_range(64 * 1024 * 1024)							// The address range
		{}

		void		set_alloc_size(u32 size);
		void		set_page_size(u32 size);
		void		set_address_range(u64 size);

		u32			get_alloc_size() const;
		u32			get_page_size() const;
		u64			get_address_range() const;

	private:
		u32			m_alloc_size;
		u32			m_page_size;
		u64			m_addr_range;
	};

	extern xfsalloc*		gCreateVFsAllocator(xalloc* main_allocator, xpage_alloc* page_allocator, xvfsa_params const& params);
	
};

#endif	/// __X_VIRTUAL_MEMORY_FSA_ALLOCATOR_H__

