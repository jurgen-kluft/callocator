#include "xbase\x_allocator.h"
#include "xallocator\x_allocator_memento.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;


UNITTEST_SUITE_BEGIN(x_allocator_memento)
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

		}

		UNITTEST_TEST(alloc_realloc_free)
		{

		}

	}
}
UNITTEST_SUITE_END

