#include "xbase\x_types.h"
#include "xallocator\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

UNITTEST_SUITE_BEGIN(x_allocator_tlfs)
{
    UNITTEST_FIXTURE(main)
    {
		x_iallocator*	gSystemAllocator;

		void*			gBlock;
		s32				gBlockSize;
		x_iallocator*	gCustomAllocator;

        UNITTEST_FIXTURE_SETUP()
		{
			gSystemAllocator = gCreateSystemAllocator();
			gBlockSize = 128 * 1024;
			gBlock = gSystemAllocator->allocate(gBlockSize, 8);
			gCustomAllocator = gCreateDlAllocator(gBlock, gBlockSize);
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
			void* mem3 = gCustomAllocator->allocate(256, 32);
			gCustomAllocator->deallocate(mem2);
			gCustomAllocator->deallocate(mem1);
			gCustomAllocator->deallocate(mem3);
        }

        UNITTEST_TEST(alloc_realloc_free)
        {
			void* mem = gCustomAllocator->allocate(512, 8);
			mem = gCustomAllocator->reallocate(mem, 1024, 16);
			mem = gCustomAllocator->reallocate(mem, 2050, 32);
			mem = gCustomAllocator->reallocate(mem, 5000, 8);
			gCustomAllocator->deallocate(mem);
        }

	}
}
UNITTEST_SUITE_END