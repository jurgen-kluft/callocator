//==============================================================================
//  x_allocator_hext.h
//==============================================================================
#ifndef __X_ALLOCATOR_HEXT_H__
#define __X_ALLOCATOR_HEXT_H__
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


	// WORK IN PROGRESS, NOT FINISHED!

	/// High performance external heap allocator, manages external memory like Texture Memory, Sound Memory
	/// by putting bookkeeping data in separate ('main') memory.
	extern x_iallocator*		gCreateHextAllocator(x_iallocator *allocator, void* mem_base, u32 mem_size, u32 min_alloc_size, u32 max_alloc_size);

};

#endif	/// __X_ALLOCATOR_HEXT_H__