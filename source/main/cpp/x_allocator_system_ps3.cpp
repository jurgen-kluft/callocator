#include "xbase\x_target.h"
#ifdef TARGET_PS3

#include <string>
#include "xallocator\x_allocator.h"

namespace xcore
{
	class x_allocator_ps3_system : public x_iallocator
	{
	public:
		x_allocator_ps3_system() : mOutOfMemoryCallback(NULL)
		{
		}

		virtual void*			allocate(s32 size, s32 alignment)
		{
			return NULL;
		}

		virtual void*			callocate(s32 nelem, s32 elemsize)
		{
			return NULL;
		}

		virtual void*			reallocate(void* ptr, s32 size, s32 alignment)
		{
			return NULL;
		}

		virtual void			deallocate(void* ptr)
		{
		}

		virtual u32				usable_size(void *ptr)
		{
			return 0;
		}

		virtual void			set_out_of_memory_callback(Callback user_callback)
		{
			mOutOfMemoryCallback = user_callback;
		}

		Callback				mOutOfMemoryCallback;
	};

	x_iallocator*		gCreateSystemAllocator()
	{
		x_allocator_ps3_system* allocator = NULL;
		return allocator;
	}

};

#endif