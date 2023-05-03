#include "cbase/c_allocator.h"
#include "callocator/c_allocator_forward.h"
#include "callocator/test_allocator.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(x_allocator_forward)
{
	UNITTEST_FIXTURE(main)
	{
		UNITTEST_ALLOCATOR;

		alloc_t*	gCustomAllocator;

		UNITTEST_FIXTURE_SETUP()
		{
		}

		UNITTEST_FIXTURE_TEARDOWN()
		{
		}

		UNITTEST_TEST(alloc3_free3)
		{
			gCustomAllocator = gCreateForwardAllocator(Allocator, 32 * 1024);

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

				gCustomAllocator->deallocate(mem4); mem4=nullptr;

				void* mem6 = gCustomAllocator->allocate(8, 8);
				CHECK_NOT_NULL(mem6);
				gCustomAllocator->deallocate(mem6); mem6=nullptr;

				void* mem7 = gCustomAllocator->allocate(2048, 256);
				void* mem8 = gCustomAllocator->allocate(1024, 256);
				CHECK_NOT_NULL(mem7);
				CHECK_NOT_NULL(mem8);

				gCustomAllocator->deallocate(mem1); mem1=nullptr;
				gCustomAllocator->deallocate(mem3); mem3=nullptr;
				gCustomAllocator->deallocate(mem2); mem2=nullptr;

				void* mem9 = gCustomAllocator->allocate(16, 8);
				CHECK_NOT_NULL(mem9);

				gCustomAllocator->deallocate(mem7); mem7=nullptr;
				gCustomAllocator->deallocate(mem5); mem5=nullptr;

				// This should wrap around
				void* mema = gCustomAllocator->allocate(2048, 8);
				CHECK_NOT_NULL(mema);

				gCustomAllocator->deallocate(mem9); mem9=nullptr;
				gCustomAllocator->deallocate(mem8); mem8=nullptr;
				gCustomAllocator->deallocate(mema); mema=nullptr;
			}

			gCustomAllocator->release();
		}

	}
}
UNITTEST_SUITE_END
