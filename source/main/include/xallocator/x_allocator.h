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
		virtual void*			allocate(s32 size, s32 align) = 0;				///< Allocate memory with alignment
		virtual void*			callocate(s32 n_e, s32 e_size) = 0;				///< Specialized element allocator
		virtual void*			reallocate(void* p, s32 size, s32 align) = 0;	///< Reallocate memory
		virtual void			deallocate(void* p) = 0;						///< Deallocate/Free memory

		virtual void			release() = 0;									///< Release/Destruct this allocator

	protected:
		virtual					~x_iallocator() {}
	};

	//==============================================================================
	// END xCore namespace
	//==============================================================================
};

#endif	/// __X_ALLOCATOR_H__