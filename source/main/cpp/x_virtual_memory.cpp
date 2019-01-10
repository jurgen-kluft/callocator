#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_memory_std.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"

#include "xallocator/x_virtual_memory.h"

namespace xcore
{
	class xvmem_os : public xvmem
	{
	public:
		virtual bool	reserve(u64 address_range, u32 page_size, u32 attributes, void*& baseptr, void*& id);
		virtual bool	release(void* baseptr, void* id);

		virtual bool	commit(void* id, void* page_address, u32 page_count);
		virtual bool	decommit(void* id, void* page_address, u32 page_count);
	};

#if defined TARGET_MAC

	bool xvmem_os::reserve(u64 address_range, u32 page_size, u32 attributes, void*& baseptr, void*& id)
	{
		return false;
	}

	bool	xvmem_os::release(void* baseptr, void* id)
	{
		return false;
	}

	bool	xvmem_os::commit(void* id, void* page_address, u32 page_count)
	{
		return false;
	}

	bool	xvmem_os::decommit(void* id, void* page_address, u32 page_count)
	{
		return false;
	}

#elif defined TARGET_PC

	bool xvmem_os::reserve(u64 address_range, u32 page_size, u32 attributes, void*& baseptr, void*& id)
	{
		return false;
	}

	bool	xvmem_os::release(void* baseptr, void* id)
	{
		return false;
	}

	bool	xvmem_os::commit(void* id, void* page_address, u32 page_count)
	{
		return false;
	}

	bool	xvmem_os::decommit(void* id, void* page_address, u32 page_count)
	{
		return false;
	}


#else

	#error Unknown Platform/Compiler configuration for xvmem

#endif


	xvmem*	gGetVirtualMemory()
	{
		static xvmem_os sVMem;
		return &sVMem;
	}

}; // namespace xcore


