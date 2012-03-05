#include "xbase\x_types.h"
#include "xbase\x_allocator.h"
#include "xallocator\private\x_allocator_small_ext.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_allocator_small_ext)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP()
		{
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
		}

        UNITTEST_TEST(init1)
        {
			xexternal_memory::xsmall_allocator sb;
			sb.init((void*)0x80000000, 65536, 64);
        }

		UNITTEST_TEST(allocate1)
		{
			xexternal_memory::xsmall_allocator sb;
			sb.init((void*)0x80000000, 65536, 64);

			void* p1 = sb.allocate(60, 4, gSystemAllocator);
			sb.deallocate(p1);

			sb.release(gSystemAllocator);
		}	
	
		UNITTEST_TEST(allocate2)
		{
			xexternal_memory::xsmall_allocator sb;
			sb.init((void*)0x80000000, 65536, 2048);

			for (s32 i=0; i<32; ++i)
			{
				void* p1 = sb.allocate(60, 4, gSystemAllocator);
				CHECK_NOT_NULL(p1);
			}
			void* p2 = sb.allocate(60, 4, gSystemAllocator);
			CHECK_NULL(p2);

			sb.release(gSystemAllocator);
		}	
	}
}
UNITTEST_SUITE_END
