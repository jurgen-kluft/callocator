#include "xbase\x_types.h"
#include "xallocator\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

UNITTEST_SUITE_BEGIN(x_allocator_system)
{
    UNITTEST_FIXTURE(main)
    {
		x_iallocator*	gSystemAllocator;

        UNITTEST_FIXTURE_SETUP()
		{
			gSystemAllocator = gCreateSystemAllocator();
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
			gSystemAllocator->release();
		}

        UNITTEST_TEST(alloc3_free3)
        {
			void* mem1 = gSystemAllocator->allocate(512, 8);
			void* mem2 = gSystemAllocator->allocate(1024, 16);
			void* mem3 = gSystemAllocator->allocate(256, 32);
			gSystemAllocator->deallocate(mem2);
			gSystemAllocator->deallocate(mem1);
			gSystemAllocator->deallocate(mem3);
        }

        UNITTEST_TEST(alloc_realloc_free)
        {
			void* mem = gSystemAllocator->allocate(512, 8);
			mem = gSystemAllocator->reallocate(mem, 1024, 16);
			mem = gSystemAllocator->reallocate(mem, 2050, 32);
			mem = gSystemAllocator->reallocate(mem, 5000, 8);
			gSystemAllocator->deallocate(mem);
        }

        UNITTEST_TEST(usable_size)
        {
			void* mem = gSystemAllocator->allocate(512, 8);
			s32 usable_size = gSystemAllocator->usable_size(mem);
			CHECK_TRUE(usable_size >= 512);
			gSystemAllocator->deallocate(mem);
        }
	}
}
UNITTEST_SUITE_END