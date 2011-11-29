#include "xbase\x_types.h"
#include "xbase\x_allocator.h"
#include "xallocator\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_allocator_fr)
{
    UNITTEST_FIXTURE(main)
    {
		x_iallocator*	gCustomAllocator;

        UNITTEST_FIXTURE_SETUP()
		{
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
		}

        UNITTEST_TEST(alloc3_free3)
        {
			gCustomAllocator = gCreateForwardRingAllocator(gSystemAllocator, 64 * 1024);

			void* mem1 = gCustomAllocator->allocate(512, 8);
			void* mem2 = gCustomAllocator->allocate(1024, 16);
			void* mem3 = gCustomAllocator->allocate(512, 32);
			void* mem4 = gCustomAllocator->allocate(1024, 1024);
			void* mem5 = gCustomAllocator->allocate(256, 32);

			gCustomAllocator->deallocate(mem4);

			void* mem6 = gCustomAllocator->allocate(8, 8);
			void* mem7 = gCustomAllocator->allocate(10000, 1024);
			void* mem8 = gCustomAllocator->allocate(20000, 2048);

			gCustomAllocator->deallocate(mem1);
			gCustomAllocator->deallocate(mem3);
			gCustomAllocator->deallocate(mem2);

			void* mem9 = gCustomAllocator->allocate(16, 8);

			gCustomAllocator->deallocate(mem7);
			gCustomAllocator->deallocate(mem5);
			gCustomAllocator->deallocate(mem8);
			gCustomAllocator->deallocate(mem9);
			gCustomAllocator->deallocate(mem6);

			gCustomAllocator->release();
        }

	}
}
UNITTEST_SUITE_END
