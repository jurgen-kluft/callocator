#include "xbase\x_target.h"
#ifdef TARGET_WII

#include <revolution\mem.h>

#include "xallocator\x_allocator.h"

namespace xcore
{
	class x_allocator_wii_system : public x_iallocator
	{
		s32					mDefaultAlignment;
		MEMAllocator		mMem1Allocator;
		MEMAllocator		mMem2Allocator;

	public:
		x_allocator_wii_system() 
			: mDefaultAlignment(4)
		{
		}

		xbool				init()
		{
			// Heap on MEM1
			void*    arenaLo = OSGetMEM1ArenaLo();
			void*    arenaHi = OSGetMEM1ArenaHi();
	    
			MEMHeapHandle heapHandle = MEMCreateExpHeap(arenaLo, (u32)arenaHi - (u32)arenaLo);
			if ( heapHandle == MEM_HEAP_INVALID_HANDLE )
			{
				// MEM1 heap allocation error
				return xFALSE;
			}
			OSSetMEM1ArenaLo(arenaHi);
			MEMInitAllocatorForExpHeap(&mMem1Allocator, heapHandle, mDefaultAlignment);
		
			// Heap on MEM2
			arenaLo = OSGetMEM2ArenaLo();
			arenaHi = OSGetMEM2ArenaHi();
	    
			heapHandle = MEMCreateExpHeap(arenaLo, (u32)arenaHi - (u32)arenaLo);
			if ( heapHandle == MEM_HEAP_INVALID_HANDLE )
			{
				// MEM2 heap allocation error
				return xFALSE;
			}
			OSSetMEM2ArenaLo(arenaHi);
			MEMInitAllocatorForExpHeap(&mMem2Allocator, heapHandle, mDefaultAlignment);

			return xTRUE;
		}

		struct header
		{
			void*	real_ptr;
		};

		static u32				recalc_size(s32 size, s32 alignment)
		{
			alignment = x_intu::max(alignment, mDefaultAlignment);
			return size + sizeof(header) + alignment*2;
		}

		static header*			get_header(void* ptr)
		{
			header* _header = (header*)((u32)ptr - sizeof(header));
			return _header;
		}

		static void*			set_header(void* ptr, s32 size, s32 requested_size, s32 requested_alignment)
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
			void* mem = MEMAllocFromAllocator(mMem2Allocator, new_size);
			return set_header(mem, new_size, size, alignment);
		}

		virtual void*			callocate(s32 nelem, s32 elemsize)
		{
			return allocate(nelem * elemsize, mDefaultAlignment);
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
			MEMFreeToAllocator(mMem2Allocator, _header->real_ptr);
		}

		virtual void			release()
		{
		}
	};

	x_iallocator*		gCreateSystemAllocator()
	{
		void*	arenaLo = OSGetMEM1ArenaLo();
		void*	arenaHi = OSGetMEM1ArenaHi();
		u32		arenaSize = (u32)arenaHi - (u32)arenaLo;

		s32 allocator_class_size = x_intu::ceilPower2(sizeof(x_allocator_wii_system));
		if (arenaSize > allocator_class_size)
		{
			void*	mem = arenaLo;
			arenaLo = (void*)((u32)arenaLo + (u32)allocator_class_size);
			OSSetMEM1ArenaLo(arenaLo);

			x_allocator_wii_system* allocator = (x_allocator_wii_system*)mem;
			if (allocator->init() == xTRUE)
				return allocator;
		}

		// System failure
		return NULL;
	}

};

#endif