#include "xbase/x_allocator.h"
#include "xbase/x_idx_allocator.h"
#include "xbase/x_integer.h"
#include "xallocator/private/x_bitlist.h"

#include "xunittest/xunittest.h"

using namespace xcore;

extern xalloc* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_bitlist)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP()
		{
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
		}

        UNITTEST_TEST(init)
        {
			u32 numdwords = xbitlist::size_in_dwords(3000);
        }
	}
}
UNITTEST_SUITE_END
