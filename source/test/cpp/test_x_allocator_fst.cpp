#include "xbase\x_types.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xallocator\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gUnitTestAllocator;


UNITTEST_SUITE_BEGIN(x_allocator_fst)
{
    UNITTEST_FIXTURE(main)
    {
		s32				gElemSize;
		s32				gElemAlignment;
		s32				gBlockElemCount;
		s32				gBlockInitialCount;
		s32				gBlockGrowthCount;
		s32				gBlockMaximumCount;

		x_iallocator*	gCustomAllocator;

        UNITTEST_FIXTURE_SETUP()
		{
			gElemSize = 12;
			gElemAlignment = 16;
			gBlockElemCount = 16;
			gBlockInitialCount = 1;
			gBlockGrowthCount = 1;
			gBlockMaximumCount = 2;

			gCustomAllocator = gCreateFstAllocator(gUnitTestAllocator, gElemSize, gElemAlignment, gBlockElemCount, gBlockInitialCount, gBlockGrowthCount, gBlockMaximumCount);
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
			gCustomAllocator->release();
		}

        UNITTEST_TEST(alloc3_free3)
        {
			void* mem1 = gCustomAllocator->allocate(12, gElemAlignment);
			void* mem2 = gCustomAllocator->allocate(10, gElemAlignment);
			void* mem3 = gCustomAllocator->allocate( 8, gElemAlignment);

			CHECK_TRUE(x_intu::isAligned((u32)mem1, gElemAlignment));
			CHECK_TRUE(x_intu::isAligned((u32)mem2, gElemAlignment));
			CHECK_TRUE(x_intu::isAligned((u32)mem3, gElemAlignment));

			gCustomAllocator->deallocate(mem2);
			gCustomAllocator->deallocate(mem1);
			gCustomAllocator->deallocate(mem3);
        }

        UNITTEST_TEST(alloc_realloc_free)
        {
			void* mem = gCustomAllocator->allocate(12, gElemAlignment);
			mem = gCustomAllocator->reallocate(mem, 10, gElemAlignment);
			mem = gCustomAllocator->reallocate(mem, 8, gElemAlignment);
			mem = gCustomAllocator->reallocate(mem, 4, gElemAlignment);
			gCustomAllocator->deallocate(mem);
        }

	}
}
UNITTEST_SUITE_END
