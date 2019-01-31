#ifndef __X_ALLOCATOR_VIRTUAL_ALLOCATOR_H__
#define __X_ALLOCATOR_VIRTUAL_ALLOCATOR_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	// Forward declares
	class xalloc;

	// A virtual memory allocator
	extern xalloc*		gCreateVmAllocator(xalloc*);

};

#endif	// __X_ALLOCATOR_VIRTUAL_ALLOCATOR_H__