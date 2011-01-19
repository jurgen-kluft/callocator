#include "xbase\x_target.h"
#ifdef TARGET_PC

#include <string>

#include "xbase\x_memory_std.h"
#include "xbase\x_integer.h"
#include "xallocator\x_allocator.h"

namespace xcore
{
	class x_allocator_win32_system : public x_iallocator
	{
	public:
		void			init() 
		{
			mDefaultAlignment = 4;
		}

		void*			operator new(u32 num_bytes, void* mem)
		{
			return mem;
		}

		virtual void*			allocate(s32 size, s32 alignment)
		{
			void* mem = _aligned_malloc(size, alignment);
			return mem;
		}

		virtual void*			callocate(s32 nelem, s32 elemsize)
		{
			return allocate(nelem * elemsize, 4);
		}

		virtual void*			reallocate(void* ptr, s32 size, s32 alignment)
		{
			ptr = _aligned_realloc(ptr, size, alignment);
			return ptr;
		}

		virtual void			deallocate(void* ptr)
		{
			_aligned_free(ptr);
		}

		virtual void			release()
		{
			mDefaultAlignment = 0;
			_aligned_free(this);
		}

		s32								mDefaultAlignment;
	};

	x_iallocator*		gCreateSystemAllocator()
	{
		void* mem = _aligned_malloc(sizeof(x_allocator_win32_system), 8);
		x_allocator_win32_system* allocator = new (mem) x_allocator_win32_system();
		allocator->init();
		return allocator;
	}

};

#endif