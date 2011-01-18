#include "../x_target.h"
#include "../x_system.h"
#include "../x_time.h"

#include "x_memorymanager.h"

namespace xcore
{
	#define MULTIPLIER_B2MB			(1.0f / (1024.0f * 1024.0f))

	x_memstat_scoped::x_memstat_scoped(const char* func, const char* name)
		: mFunc(func)
		, mName(name)
	{
#ifdef TARGET_WII
		xmem_managed_size mem1Stats;
		xmem_mem1_heap::_stats(mem1Stats);
		mFreeMem1 = (mem1Stats.mMaxSystemSize - mem1Stats.mCurrentInuseSize) * MULTIPLIER_B2MB ;
#endif
		xmem_managed_size mem2Stats;
		xmem_main_heap::_stats(mem2Stats);
		mFreeMem2 = (mem2Stats.mCurrentSystemSize - mem2Stats.mCurrentInuseSize) * MULTIPLIER_B2MB ;

		x_printf("%s", x_va_list(mFunc));
		if (mName) x_printf(": %s", x_va_list(mName));
#ifndef TARGET_WII
		x_printf(", Begin, Mem Free:%f MB\n", x_va_list(mFreeMem2));
#else
		x_printf(", Begin, Mem1 Free:%f MB ; Mem2 Free:%f MB.\n", x_va_list(mFreeMem1 , mFreeMem2));
#endif
	}

	x_memstat_scoped::~x_memstat_scoped()
	{
#ifdef TARGET_WII
		xcore::xmem_managed_size mem1Stats;
		xmem_mem1_heap::_stats(mem1Stats);
		f32 endFreeMem1 = (mem1Stats.mMaxSystemSize - mem1Stats.mCurrentInuseSize) * MULTIPLIER_B2MB ;
#endif

		xmem_managed_size mem2Stats;
		xmem_main_heap::_stats(mem2Stats);			
		f32 endFreeMem2 = (mem2Stats.mCurrentSystemSize - mem2Stats.mCurrentInuseSize) * MULTIPLIER_B2MB ;

		x_printf("%s", x_va_list(mFunc));
		if (mName) x_printf(": %s", x_va_list(mName));

#ifndef TARGET_WII
		x_printf(", End, Mem Free:%f MB, Cost:%f MB\n", x_va_list(endFreeMem2, mFreeMem2 - endFreeMem2 ));	
#else
		x_printf(", End, Mem1 Free:%f MB, Cost:%f MB ; Mem2 Free:%f MB, Cost:%f MB\n", x_va_list(endFreeMem1, mFreeMem1-endFreeMem1, endFreeMem2, mFreeMem2-endFreeMem2 ));	
#endif
	}

	xmem_heap		xmem_main_heap::sHeap;

#ifdef TARGET_PC
	void			xmem_main_heap::_set_sys_calls(SysAllocFunc sys_alloc, SysFreeFunc sys_free)
	{
		sHeap.__set_sys_calls(sys_alloc, sys_free);
	}
#endif
#ifdef TARGET_MULTITHREADED_MEMORY_MANAGER
	void				xmem_main_heap::_destroy()
	{
#ifdef TARGET_MULTITHREADED_MEMORY_MANAGER
		x_Destruct(sCriticalSection);
		sHeap.__free(sCriticalSection);
		sCriticalSection = NULL;
#endif

		sHeap.__destroy(); 
	}

	void				xmem_main_heap::_manage(void* mem, s32 memsize)
	{
		sHeap.__manage(mem, memsize); 

#ifdef TARGET_MULTITHREADED_MEMORY_MANAGER
		sCriticalSection = (xcritical_section*)sHeap.__allocA(XMEM_FLAG_ALIGN_16B, sizeof(xcritical_section));
		x_Construct(sCriticalSection);
#endif
	}

	xcritical_section*	xmem_main_heap::sCriticalSection;
#endif

#ifdef TARGET_WII
	xmem_heap		xmem_mem1_heap::sHeap;
#endif

	f32		_read_free_mem_heap()
	{
		const f32 B2MB = 1.0f / (1024.0f * 1024.0f);
		xmem_managed_size	   mem2Stats ;
		xmem_main_heap::_stats(mem2Stats);
		return	(mem2Stats.mCurrentSystemSize - mem2Stats.mCurrentInuseSize) * B2MB;
	}

#ifndef TARGET_MULTITHREADED_MEMORY_MANAGER

void* xmem_main_heap::_allocate(s32 size, s32 alignment)				
{ 
	return (alignment<=X_MEMALIGN ? sHeap.__alloc(size) : sHeap.__allocA(alignment, size));
}

#ifdef TARGET_WII
void* xmem_mem1_heap::_allocate(s32 size, s32 alignment)				
{ 
	return (alignment<=X_MEMALIGN ? sHeap.__alloc(size) : sHeap.__allocA(alignment, size));
}

#endif //TARGET_WII

#endif //TARGET_MULTITHREADED_MEMORY_MANAGER

};	///< namespace xcore

