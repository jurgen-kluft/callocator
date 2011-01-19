#include "xbase\x_target.h"
#ifdef TARGET_N3DS

#include "xallocator\x_allocator.h"

namespace xcore
{
	class x_allocator_3ds_system : public x_iallocator
	{
	public:
		x_allocator_3ds_system() 
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

	};

	x_iallocator*		gCreateSystemAllocator()
	{
		x_allocator_3ds_system* allocator = NULL;
		return allocator;
	}

};

#endif