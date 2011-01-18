//==============================================================================
//
//  x_memorymanager.h
//
//==============================================================================
#ifndef __X_MEMORY_MANAGER_H__
#define __X_MEMORY_MANAGER_H__
#include "..\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "..\x_memory.h"
#ifdef TARGET_MULTITHREADED_MEMORY_MANAGER
	#include "..\x_critical_section.h"
#endif

#include "x_memory_heap_dlmalloc.h"

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	class x_memstat_scoped
	{		
	public:
					x_memstat_scoped(const char* func, const char* name);
					~x_memstat_scoped();

	private:
		const char*	mFunc;
		const char*	mName;
		f32			mFreeMem1;
		f32			mFreeMem2;
	};	

	#define __MEMSTAT_STR2__(x)		":"#x
	#define __MEMSTAT_STR1__(x)		__MEMSTAT_STR2__(x)

	#define XSMEMSTAT()			xcore::x_memstat_scoped x_memstat_scoped_(__FUNCTION__, NULL)
	#define XSNMEMSTAT(name)	xcore::x_memstat_scoped x_memstat_scoped_##name (__FUNCTION__, #name)

#ifdef TARGET_MULTITHREADED_MEMORY_MANAGER

	class xmem_main_heap
	{
	public:
		static void				_initialize()									{ sHeap.__initialize(); }
		static void				_destroy();

		static void				_manage(void* mem, s32 memsize);

		static void*			_allocate(s32 size, s32 alignment)				{ sCriticalSection->beginAtomic(); void* mem; if (alignment<=X_MEMALIGN) mem=sHeap.__alloc(size); else mem=sHeap.__allocA(alignment, size); sCriticalSection->endAtomic(); return mem; }
		static void*			_callocate(s32 nelem, s32 elemsize)				{ sCriticalSection->beginAtomic(); void* mem; mem=sHeap.__allocN(nelem, elemsize); sCriticalSection->endAtomic(); return mem; }
		static void*			_reallocate(void* ptr, s32 size, s32 alignment)	{ sCriticalSection->beginAtomic(); void* mem=sHeap.__allocR(ptr,alignment,size); sCriticalSection->endAtomic(); return mem; }
		static void				_free(void* ptr)								{ sCriticalSection->beginAtomic(); sHeap.__free(ptr); sCriticalSection->endAtomic(); }

		static u32				_usable_size(void *ptr)							{ sCriticalSection->beginAtomic(); u32 size=sHeap.__usable_size(ptr); sCriticalSection->endAtomic(); return size; }
		static void				_stats(xmem_managed_size& stats)				{ sCriticalSection->beginAtomic(); sHeap.__stats(stats); sCriticalSection->endAtomic(); }

#ifdef TARGET_PC
		static void				_set_sys_calls(SysAllocFunc sys_alloc, SysFreeFunc sys_free);
#endif
	private:
		static xmem_heap		sHeap;
		static xcritical_section* sCriticalSection;
	};

#else

	class xmem_main_heap
	{
	public:
		static void				_initialize()									{ sHeap.__initialize(); }
		static void				_destroy()										{ sHeap.__destroy(); }

		static void				_manage(void* mem, s32 memsize)					{ sHeap.__manage(mem, memsize); }

		static void*			_allocate(s32 size, s32 alignment);

		static void*			_callocate(s32 nelem, s32 elemsize)				{ void* mem; mem=sHeap.__allocN(nelem, elemsize); return mem; }
		static void*			_reallocate(void* ptr, s32 size, s32 alignment)	{ return sHeap.__allocR(ptr,alignment, size); }
		static void				_free(void* ptr)								{ sHeap.__free(ptr); }

		static u32				_usable_size(void *ptr)							{ u32 size=sHeap.__usable_size(ptr); return size; }
		static void				_stats(xmem_managed_size& stats)				{ sHeap.__stats(stats); }

#ifdef TARGET_PC
		static void				_set_sys_calls(SysAllocFunc sys_alloc, SysFreeFunc sys_free);
#endif	///< #ifdef TARGET_PC
	private:
		static xmem_heap		sHeap;
	};

#endif	///< #ifdef TARGET_MULTITHREADED_MEMORY_MANAGER


#ifdef TARGET_WII
	class xmem_mem1_heap
	{
	public:
		static void				_initialize()									{ sHeap.__initialize(); }
		static void				_destroy()										{ sHeap.__destroy(); }

		static void				_manage(void* mem, s32 memsize)					{ sHeap.__manage(mem, memsize); }

		static void*			_allocate(s32 size, s32 alignment);

		static void*			_callocate(s32 nelem, s32 elemsize)				{ void* mem; mem=sHeap.__allocN(nelem, elemsize); return mem; }
		static void*			_reallocate(void* ptr, s32 size, s32 alignment)	{ return sHeap.__allocR(ptr,alignment,size); }
		static void				_free(void* ptr)								{ sHeap.__free(ptr); }

		static u32				_usable_size(void *ptr)							{ u32 size=sHeap.__usable_size(ptr); return size; }
		static void				_stats(xmem_managed_size& stats)				{ sHeap.__stats(stats); }

	private:
		static xmem_heap		sHeap;
	};
#endif	///< #ifdef TARGET_WII

	f32		_read_free_mem_heap();



	//==============================================================================
	// END xCore namespace
	//==============================================================================
};

#endif	/// __X_MEMORY_MANAGER_H__