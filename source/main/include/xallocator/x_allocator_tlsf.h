//==============================================================================
//  x_tlsf_allocator.h
//==============================================================================
#ifndef __X_TLSF_ALLOCATOR_H__
#define __X_TLSF_ALLOCATOR_H__
#include "xbase\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	/// Forward declares
	class x_iallocator;

	/// A custom allocator; 'Two-Level Segregate Fit' allocator
	extern x_iallocator*		gCreateTlsfAllocator(void* mem, u32 memsize);

};

#endif	/// __X_TLSF_ALLOCATOR_H__

