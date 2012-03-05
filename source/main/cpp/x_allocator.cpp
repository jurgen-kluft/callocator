#include "xbase\x_target.h"

#include "xallocator\x_allocator.h"
#include "xallocator\x_allocator_dl.h"

namespace xcore
{
	x_iallocator*		gCreateHeapAllocator(void* mem_begin, u32 mem_size)
	{
		return gCreateDlAllocator(mem_begin, mem_size);
	}

};

