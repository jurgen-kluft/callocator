//==============================================================================
//  x_forward_allocator.h
//==============================================================================
#ifndef __X_FORWARD_ALLOCATOR_H__
#define __X_FORWARD_ALLOCATOR_H__
#include "xbase\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	/// Forward declares
	class x_iallocator;

	/// The forward allocator is a specialized allocator. You can use it when you are allocating different size blocks that
	/// all have a life-time that doesn't differ much. This allocator is very fast in allocation O(1), deallocations effectively
	/// also shows O(1) behavior but due to its coalesce mechanism can sometimes take a tiny bit more time. If you mostly allocate
	/// and deallocate in a non-random order than deallocation is surely O(1).
	extern x_iallocator*		gCreateForwardAllocator(x_iallocator* allocator, u32 memsize);

};

#endif	/// __X_FORWARD_ALLOCATOR_H__
