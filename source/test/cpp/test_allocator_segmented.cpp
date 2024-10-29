#include "cbase/c_allocator.h"
#include "cbase/c_context.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"

#include "callocator/c_allocator_segmented.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(segmented)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(allocate)
        {
            nsegmented::allocator_t<u16> range;
            range.setup(Allocator, 1 * cTB, 15);

            u64 address = 0;
            CHECK_TRUE(range.allocate(32 * cMB, address));
            CHECK_EQUAL(0, address);

            CHECK_TRUE(range.allocate(32 * cMB, address));
            CHECK_EQUAL(32 * cMB, address);

            CHECK_TRUE(range.allocate(64 * cMB, address));
            CHECK_EQUAL(64 * cMB, address);

            CHECK_TRUE(range.allocate(128 * cMB, address));
            CHECK_EQUAL(128 * cMB, address);

            CHECK_TRUE(range.allocate(256 * cMB, address));
            CHECK_EQUAL(256 * cMB, address);

            CHECK_TRUE(range.allocate(512 * cMB, address));
            CHECK_EQUAL(512 * cMB, address);

            CHECK_TRUE(range.allocate(1 * cGB, address));
            CHECK_EQUAL(1 * cGB, address);

            range.teardown(Allocator);
        }

        UNITTEST_TEST(allocate_and_deallocate)
        {
            nsegmented::allocator_t<u16> range;
            range.setup(Allocator, 4 * cGB, 8);

            for (s32 i = 0; i < 16; ++i)
            {
                u64 address = 0;
                CHECK_TRUE(range.allocate(32 * cMB, address));
                CHECK_EQUAL(0, address);

                u64 address2 = 0;
                CHECK_TRUE(range.allocate(32 * cMB, address2));
                CHECK_EQUAL(32 * cMB, address2);

                u64 address3 = 0;
                CHECK_TRUE(range.allocate(64 * cMB, address3));
                CHECK_EQUAL(64 * cMB, address3);

                CHECK_TRUE(range.deallocate(address));
                CHECK_TRUE(range.deallocate(address2));
                CHECK_TRUE(range.deallocate(address3));
            }

            range.teardown(Allocator);
        }
    }
}
