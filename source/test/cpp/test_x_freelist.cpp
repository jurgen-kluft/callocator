#include "xbase/x_allocator.h"
#include "xbase/x_idx_allocator.h"
#include "xbase/x_integer.h"
#include "xallocator/private/x_freelist.h"

#include "xunittest/xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_freelist)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP()
		{
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
		}

        UNITTEST_TEST(alloc)
        {
			xfreelist_t list;

			list.alloc(gSystemAllocator, 4, 4, 100);
			for (s32 i = 0; i < list.size(); ++i)
			{
				s32* elem = (s32*)list.ptr_of(i);
				*elem = i;
			}

			list.release();
        }
	}
}
UNITTEST_SUITE_END
