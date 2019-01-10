#ifndef __X_VIRTUAL_MEMORY_INTERFACE_H__
#define __X_VIRTUAL_MEMORY_INTERFACE_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

namespace xcore
{
	class xvmem 
	{
	public:
		virtual bool	reserve(u64 address_range, u32 page_size, u32 attributes, void*& baseptr, void*& id) = 0;
		virtual bool	release(void* baseptr, void* id) = 0;

		virtual bool	commit(void* id, void* page_address, u32 page_count) = 0;
		virtual bool	decommit(void* id, void* page_address, u32 page_count) = 0;
	};

	extern xvmem*	gGetVirtualMemory();

}; // namespace xcore

#endif /// __X_TLSF_ALLOCATOR_H__
