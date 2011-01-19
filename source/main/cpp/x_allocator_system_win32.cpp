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
			mOutOfMemoryCallback = NULL;
		}

		void*			operator new(u32 num_bytes, void* mem)
		{
			return mem;
		}

		struct header
		{
			void*	real_ptr;
			u32		real_size;
			u32		requested_size;
			u32		requested_alignment;
		};

		u32				recalc_size(s32 size, s32 alignment)
		{
			alignment = x_intu::max(alignment, mDefaultAlignment);
			return size + sizeof(header) + alignment*2;
		}

		header*			get_header(void* ptr)
		{
			header* _header = (header*)((u32)ptr - sizeof(header));
			return _header;
		}

		void*			set_header(void* ptr, s32 size, s32 requested_size, s32 requested_alignment)
		{
			void* new_ptr = (void*)(x_intu::alignUp((u32)ptr + (size - requested_size), requested_alignment));

			header* _header = get_header(new_ptr);
			_header->real_ptr = ptr;
			_header->real_size = requested_size;
			_header->requested_size = requested_size;
			_header->requested_alignment = requested_alignment;
			return new_ptr;
		}

		virtual void*			allocate(s32 size, s32 alignment)
		{
			s32 new_size = recalc_size(size, alignment);
			void* mem = malloc(new_size);
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
			void* mem = allocate(size, alignment);
			x_memcpy(mem, ptr, _header->real_size);
			deallocate(ptr);
			return mem;
		}

		virtual void			deallocate(void* ptr)
		{
			header* _header = get_header(ptr);
			free(_header->real_ptr);
		}

		virtual u32				usable_size(void *ptr)
		{
			header* _header = get_header(ptr);
			return _header->requested_size;
		}

		virtual void			release()
		{
			mDefaultAlignment = 0;
			mOutOfMemoryCallback = NULL;
			_aligned_free(this);
		}

		virtual void			set_out_of_memory_callback(Callback user_callback)
		{
			mOutOfMemoryCallback = user_callback;
		}

		s32						mDefaultAlignment;
		Callback				mOutOfMemoryCallback;
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