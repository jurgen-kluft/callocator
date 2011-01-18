//==============================================================================
//  x_allocator.h
//==============================================================================
#ifndef __X_ALLOCATOR_H__
#define __X_ALLOCATOR_H__
#include "xbase\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase\x_types.h"

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	/// Forward declares
	class x_iallocator;

	/// The custom allocators
	extern x_iallocator*		gCreateDlAllocator(void* mem, s32 memsize);
	extern x_iallocator*		gCreateTlsfAllocator(void* mem, s32 memsize);
	
	/// This system allocator, will just call system malloc/realloc/free
	extern x_iallocator*		gCreateSystemAllocator();
	
	/// The allocator interface
	class x_iallocator
	{
	public:
		virtual					~x_iallocator() {}

		virtual void*			allocate(s32 size, s32 alignment) = 0;
		virtual void*			callocate(s32 nelem, s32 elemsize) = 0;
		virtual void*			reallocate(void* ptr, s32 size, s32 alignment) = 0;
		virtual void			deallocate(void* ptr) = 0;

		virtual u32				usable_size(void *ptr) = 0;

		typedef void			(*Callback)(void);
		virtual void			set_out_of_memory_callback(Callback user_callback) = 0;
	};


	//==============================================================================
	// END xCore namespace
	//==============================================================================
};

#endif	/// __X_ALLOCATOR_H__