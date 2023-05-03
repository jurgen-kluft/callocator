#include "cbase/c_allocator.h"
#include "cbase/c_integer.h"
#include "callocator/private/c_freelist.h"
#include "callocator/test_allocator.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(x_freelist)
{
    UNITTEST_FIXTURE(main)
    {
		UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP()
		{
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
		}

        UNITTEST_TEST(alloc)
        {
			freelist_t list;
			list.init_with_alloc(Allocator, 4, 4, 100);

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
