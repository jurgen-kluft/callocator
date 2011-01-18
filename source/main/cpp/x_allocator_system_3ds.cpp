#include "xbase\x_target.h"
#ifdef TARGET_N3DS

#include <string>
#include "xallocator\x_allocator.h"

namespace xcore
{
	class x_allocator_3ds_system : public x_iallocator
	{
	public:
		x_allocator_3ds_system() : mOutOfMemoryCallback(NULL)
		{
		}

		struct header
		{
			void*	real_ptr;
			u32		real_size;
			u32		requested_size;
			u32		requested_alignment;
		};

		static u32				recalc_size(s32 size, s32 alignment)
		{
			if (alignment < 4) alignment = 4;
			return size + (sizeof(header) > alignment) ? sizeof(header) : alignment;
		}

		static header*			get_header(void* ptr)
		{
			header* _header = (header*)((u32)ptr - sizeof(header));
			return _header;
		}

		static void*			set_header(void* ptr, s32 size, s32 requested_size, s32 requested_alignment)
		{
			header* _header = (header*)((u32)ptr + requested_alignment - sizeof(header));
			_header->real_ptr = ptr;
			_header->real_size = requested_size;
			_header->requested_size = requested_size;
			_header->requested_alignment = requested_alignment;
			return (void*)((u32)ptr + requested_alignment);
		}

		virtual void*			allocate(s32 size, s32 alignment)
		{
			s32 new_size = recalc_size(size, alignment);
			void* mem = _aligned_malloc(new_size, alignment);
			return set_header(mem, new_size, size, alignment);
		}

		virtual void*			callocate(s32 nelem, s32 elemsize)
		{
			return allocate(nelem * elemsize, 4);
		}

		virtual void*			reallocate(void* ptr, s32 size, s32 alignment)
		{
			header* _header = get_header(ptr);
			s32 new_size = recalc_size(size, alignment);
			void* mem = _aligned_realloc(_header->real_ptr, new_size, alignment);
			return set_header(mem, new_size, size, alignment);
		}

		virtual void			deallocate(void* ptr)
		{
			header* _header = get_header(ptr);
			_aligned_free(_header->real_ptr);
		}

		virtual u32				usable_size(void *ptr)
		{
			header* _header = get_header(ptr);
			return _header->requested_size;
		}

		virtual void			set_out_of_memory_callback(Callback user_callback)
		{
			mOutOfMemoryCallback = user_callback;
		}

		Callback				mOutOfMemoryCallback;
	};

	x_iallocator*		gCreateSystemAllocator()
	{
		x_allocator_3ds_system* allocator = (x_allocator_3ds_system*)_aligned_malloc(sizeof(x_allocator_3ds_system), 8);
		return allocator;
	}

};

#endif