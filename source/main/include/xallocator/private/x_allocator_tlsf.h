//==============================================================================
//
//  x_memory_heap.h
//
//==============================================================================
#ifndef __X_MEMORY_HEAP_H__
#define __X_MEMORY_HEAP_H__
#include "..\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "..\x_memory.h"

#if 0

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	#define XMEM_HEAP_DEBUG								1


	typedef				void*	(*SysAllocFunc)(xsize_t size);
	typedef				void	(*SysFreeFunc)(void* ptrsize);

	//////////////////////////////////////////////////////////////////////////
	// A memory heap capable of managing multiple segments (based on dlmalloc)
	struct xmem_managed_size
	{
		xsize_t		mMaxSystemSize;
		xsize_t		mCurrentSystemSize;
		xsize_t		mCurrentInuseSize;
	};

	//////////////////////////////////////////////////////////////////////////
	// A memory heap capable of managing multiple segments (based on dlmalloc)
	class xmem_heap_base
	{
	protected:
		void*				mPool;

		SysAllocFunc		mSysAlloc;
		SysFreeFunc			mSysFree;

	public:
		void*				__alloc(xsize_t bytes);													///< Normal allocation
		void*				__allocA(xsize_t alignment, xsize_t size);								///< Aligned allocation
		void*				__allocR(void* ptr, xsize_t size);										///< Re allocation
		void*				__allocN(xsize_t n_elements, xsize_t element_size);						///< Elements allocation
		void**				__allocIC(xsize_t n_elements, xsize_t element_size, void** chunks);		///< Independent continues with equal sized elements
		void**				__allocICO(xsize_t n_elements, xsize_t* element_sizes, void** chunks);	///< Independent continues with different size specified for every element
		void				__free(void* ptr);

		u32					__usable_size(void* mem);
		void				__stats(xmem_managed_size& stats);

#ifdef TARGET_PC
		void				__set_sys_calls(SysAllocFunc sys_alloc, SysFreeFunc sys_free)
		{
			mSysAlloc = sys_alloc;
			mSysFree = sys_free;
		}
#endif
	};

	//////////////////////////////////////////////////////////////////////////
	/// xmem_space is an opaque type representing an independent region of space that supports mspace_malloc, etc.
	class xmem_space : public xmem_heap_base
	{
	public:
		void				__manage(void* mem, xsize_t size);
		void				__destroy();

	private:
		void				__initialize(xbyte* tbase, xsize_t tsize);
	};

	//////////////////////////////////////////////////////////////////////////
	// A memory heap capable of managing multiple segments (based on dlmalloc)
	class xmem_heap : public xmem_heap_base
	{
	public:
		void				__initialize();
		void				__destroy();

		void				__manage(void* mem, xsize_t size);
	};


	//==============================================================================
	// END xCore namespace
	//==============================================================================
};

#endif

#endif	/// __X_MEMORY_HEAP_H__