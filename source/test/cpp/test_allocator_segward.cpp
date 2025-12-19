#include "callocator/c_allocator_segward.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(segward)
{
    UNITTEST_FIXTURE(main)
    {
        // UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() { }
        UNITTEST_FIXTURE_TEARDOWN() { }

        UNITTEST_TEST(create)
        {
            nsegward::allocator_t* allocator = nsegward::create(64 * 1024, 8 * 1024 * 1024);

            nsegward::destroy(allocator);
        }

        UNITTEST_TEST(alloc_free_forward)
        {

        }
    }
}
UNITTEST_SUITE_END
