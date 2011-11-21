#include "xbase\x_types.h"
#include "xbase\x_allocator.h"
#include "xallocator\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_allocator_eb)
{
    UNITTEST_FIXTURE(main)
    {
		void*			gBlock;
		s32				gBlockSize;
		x_iallocator*	gCustomAllocator;

		static void	my_extmem_copy(void const* src, u32 src_size, void* dst, u32 dst_size)
		{

		}

        UNITTEST_FIXTURE_SETUP()
		{
			gBlockSize = 128 * 1024;
			gBlock = gSystemAllocator->allocate(gBlockSize, 8);
			
			gCustomAllocator = gCreateEbAllocator(gBlock, gBlockSize, gSystemAllocator, &my_extmem_copy);
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
			gCustomAllocator->release();
			gSystemAllocator->deallocate(gBlock);
			gBlock = NULL;
			gBlockSize = 0;
		}

        UNITTEST_TEST(alloc3_free3)
        {
			void* mem1 = gCustomAllocator->allocate(512, 8);
			void* mem2 = gCustomAllocator->allocate(1024, 16);
			void* mem3 = gCustomAllocator->allocate(512, 32);
			void* mem4 = gCustomAllocator->allocate(1024, 1024);
			void* mem5 = gCustomAllocator->allocate(256, 32);

			gCustomAllocator->deallocate(mem4);

			void* mem6 = gCustomAllocator->allocate(8, 8);
			void* mem7 = gCustomAllocator->allocate(100000, 1024);
			void* mem8 = gCustomAllocator->allocate(20000, 2048);

			gCustomAllocator->deallocate(mem1);

			void* mem9 = gCustomAllocator->allocate(16, 8);

			gCustomAllocator->deallocate(mem7);
			
			gCustomAllocator->deallocate(mem2);

			gCustomAllocator->deallocate(mem3);
			gCustomAllocator->deallocate(mem5);
			gCustomAllocator->deallocate(mem8);
			gCustomAllocator->deallocate(mem9);

        }

	}
}
UNITTEST_SUITE_END
