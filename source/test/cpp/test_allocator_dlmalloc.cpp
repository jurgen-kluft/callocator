#include "cbase/c_allocator.h"
#include "callocator/c_allocator_dlmalloc.h"
#include "callocator/test_allocator.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(allocator_dlmalloc)
{
	UNITTEST_FIXTURE(main)
	{
		UNITTEST_ALLOCATOR;

		void*			gBlock;
		s32				gBlockSize;
		dlmalloc_t 	gCustomAllocator;

		UNITTEST_FIXTURE_SETUP()
		{
			gBlockSize = 128 * 1024;
			gBlock = Allocator->allocate(gBlockSize, 8);
			gCustomAllocator.init(gBlock, gBlockSize);
		}

		UNITTEST_FIXTURE_TEARDOWN()
		{
			gCustomAllocator.exit();
			gBlock = nullptr;
			gBlockSize = 0;
		}

		UNITTEST_TEST(alloc3_free3)
		{
			void* mem1 = gCustomAllocator.allocate(512, 8);
			void* mem2 = gCustomAllocator.allocate(1024, 16);
			void* mem3 = gCustomAllocator.allocate(256, 32);
			gCustomAllocator.deallocate(mem2);
			gCustomAllocator.deallocate(mem1);
			gCustomAllocator.deallocate(mem3);
		}

	}
}
UNITTEST_SUITE_END

