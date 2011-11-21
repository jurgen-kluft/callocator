#include "xbase\x_types.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xallocator\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;


UNITTEST_SUITE_BEGIN(x_allocator_pool)
{
    UNITTEST_FIXTURE(main)
    {
		xpool_params gParams;	

		x_iallocator*	gCustomAllocator;

        UNITTEST_FIXTURE_SETUP()
		{
			gParams.set_elem_size(12);
			gParams.set_elem_alignment(16);
			gParams.set_block_size(16);
			gParams.set_block_initial_count(1);
			gParams.set_block_growth_count(1);
			gParams.set_block_max_count(2);

			gCustomAllocator = gCreatePoolAllocator(gSystemAllocator, gParams);
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
			gCustomAllocator->release();
		}

        UNITTEST_TEST(alloc3_free3)
        {
			void* mem1 = gCustomAllocator->allocate(12, gParams.get_elem_alignment());
			void* mem2 = gCustomAllocator->allocate(10, gParams.get_elem_alignment());
			void* mem3 = gCustomAllocator->allocate( 8, gParams.get_elem_alignment());

			CHECK_TRUE(x_intu::isAligned((u32)mem1, gParams.get_elem_alignment()));
			CHECK_TRUE(x_intu::isAligned((u32)mem2, gParams.get_elem_alignment()));
			CHECK_TRUE(x_intu::isAligned((u32)mem3, gParams.get_elem_alignment()));

			gCustomAllocator->deallocate(mem2);
			gCustomAllocator->deallocate(mem1);
			gCustomAllocator->deallocate(mem3);
        }

        UNITTEST_TEST(alloc_realloc_free)
        {
			void* mem = gCustomAllocator->allocate(12, gParams.get_elem_alignment());
			mem = gCustomAllocator->reallocate(mem, 10, gParams.get_elem_alignment());
			mem = gCustomAllocator->reallocate(mem, 8, gParams.get_elem_alignment());
			mem = gCustomAllocator->reallocate(mem, 4, gParams.get_elem_alignment());
			gCustomAllocator->deallocate(mem);
        }

	}
}
UNITTEST_SUITE_END
