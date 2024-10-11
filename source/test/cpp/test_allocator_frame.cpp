#include "ccore/c_allocator.h"
#include "callocator/c_allocator_frame.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(frame_alloc)
{
	UNITTEST_FIXTURE(main)
	{
		UNITTEST_ALLOCATOR;

		void* frame_alloc_mem = nullptr;

		UNITTEST_FIXTURE_SETUP()
		{
			frame_alloc_mem = Allocator->allocate(1 * cMB);
		}

		UNITTEST_FIXTURE_TEARDOWN()
		{
			Allocator->deallocate(frame_alloc_mem);
		}

		UNITTEST_TEST(alloc3_free3)
		{
			frame_alloc_t alloc(frame_alloc_mem, 1 * cMB);

			for (s32 i=0; i<12; ++i)
			{
				void* mem1 = alloc.allocate(512, 8);
				void* mem2 = alloc.allocate(1024, 16);
				void* mem3 = alloc.allocate(512, 32);
				void* mem4 = alloc.allocate(1024, 256);
				void* mem5 = alloc.allocate(256, 32);
				CHECK_NOT_NULL(mem1);
				CHECK_NOT_NULL(mem2);
				CHECK_NOT_NULL(mem3);
				CHECK_NOT_NULL(mem4);
				CHECK_NOT_NULL(mem5);

				alloc.deallocate(mem4); mem4=nullptr;

				void* mem6 = alloc.allocate(8, 8);
				CHECK_NOT_NULL(mem6);
				alloc.deallocate(mem6); mem6=nullptr;

				void* mem7 = alloc.allocate(2048, 256);
				void* mem8 = alloc.allocate(1024, 256);
				CHECK_NOT_NULL(mem7);
				CHECK_NOT_NULL(mem8);

				alloc.deallocate(mem1); mem1=nullptr;
				alloc.deallocate(mem3); mem3=nullptr;
				alloc.deallocate(mem2); mem2=nullptr;

				void* mem9 = alloc.allocate(16, 8);
				CHECK_NOT_NULL(mem9);

				alloc.deallocate(mem7); mem7=nullptr;
				alloc.deallocate(mem5); mem5=nullptr;

				// This should wrap around
				void* mema = alloc.allocate(2048, 8);
				CHECK_NOT_NULL(mema);

				alloc.deallocate(mem9); mem9=nullptr;
				alloc.deallocate(mem8); mem8=nullptr;
				alloc.deallocate(mema); mema=nullptr;
			}
		}
	}
}
UNITTEST_SUITE_END
