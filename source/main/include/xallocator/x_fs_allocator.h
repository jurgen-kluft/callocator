#ifndef __X_FSALLOCATOR_H__
#define __X_FSALLOCATOR_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	// xfsalloc is a 'fixed size' allocator

	class xfsalloc
	{
	public:
		virtual void*		allocate(u32& size) = 0;
		virtual void		deallocate(void* p) = 0;
		virtual void		release() = 0;

	protected:
		virtual				~xfsalloc() {}
	};	
};

#endif	/// __X_FSALLOCATOR_H__