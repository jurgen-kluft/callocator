#include "xbase\x_types.h"
#include "xbase\x_allocator.h"
#include "xallocator\x_allocator_forward.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_allocator_forward)
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
			gCustomAllocator = gCreateForwardAllocator(gSystemAllocator, 32 * 1024);

			for (s32 i=0; i<12; ++i)
			{
				void* mem1 = gCustomAllocator->allocate(512, 8);
				void* mem2 = gCustomAllocator->allocate(1024, 16);
				void* mem3 = gCustomAllocator->allocate(512, 32);
				void* mem4 = gCustomAllocator->allocate(1024, 256);
				void* mem5 = gCustomAllocator->allocate(256, 32);
				CHECK_NOT_NULL(mem1);
				CHECK_NOT_NULL(mem2);
				CHECK_NOT_NULL(mem3);
				CHECK_NOT_NULL(mem4);
				CHECK_NOT_NULL(mem5);

				gCustomAllocator->deallocate(mem4); mem4=NULL;

				void* mem6 = gCustomAllocator->allocate(8, 8);
				CHECK_NOT_NULL(mem6);
				gCustomAllocator->deallocate(mem6); mem6=NULL;

				void* mem7 = gCustomAllocator->allocate(2048, 256);
				void* mem8 = gCustomAllocator->allocate(1024, 256);
				CHECK_NOT_NULL(mem7);
				CHECK_NOT_NULL(mem8);

				gCustomAllocator->deallocate(mem1); mem1=NULL;
				gCustomAllocator->deallocate(mem3); mem3=NULL;
				gCustomAllocator->deallocate(mem2); mem2=NULL;

				void* mem9 = gCustomAllocator->allocate(16, 8);
				CHECK_NOT_NULL(mem9);

				gCustomAllocator->deallocate(mem7); mem7=NULL;
				gCustomAllocator->deallocate(mem5); mem5=NULL;

				// This should wrap around
				void* mema = gCustomAllocator->allocate(2048, 8);
				CHECK_NOT_NULL(mema);

				gCustomAllocator->deallocate(mem9); mem9=NULL;
				gCustomAllocator->deallocate(mem8); mem8=NULL;
				gCustomAllocator->deallocate(mema); mema=NULL;
			}

			gCustomAllocator->release();
		}

	}
}
UNITTEST_SUITE_END
