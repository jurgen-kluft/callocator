#include "cbase/c_allocator.h"
#include "cbase/c_context.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"

#include "callocator/c_allocator_segment.h"

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
            range.setup(Allocator, 15, 15); // 32768 nodes

            u16 node = 0;
            CHECK_TRUE(range.allocate(1, node));
            CHECK_EQUAL(0, node);

            CHECK_TRUE(range.allocate(1, node));
            CHECK_EQUAL(1, node);

            CHECK_TRUE(range.allocate(2, node));
            CHECK_EQUAL(2, node);

            CHECK_TRUE(range.allocate(4, node));
            CHECK_EQUAL(4, node);

            CHECK_TRUE(range.allocate(8, node));
            CHECK_EQUAL(8, node);

            CHECK_TRUE(range.allocate(16, node));
            CHECK_EQUAL(16, node);

            CHECK_TRUE(range.allocate(32, node));
            CHECK_EQUAL(32, node);

            range.teardown(Allocator);
        }

        UNITTEST_TEST(allocate_and_deallocate)
        {
            nsegmented::allocator_t<u16> range;
            range.setup(Allocator, 8, 8); // 256 nodes

            for (s32 i = 0; i < 16; ++i)
            {
                u16 node;
                CHECK_TRUE(range.allocate(1, node));
                CHECK_EQUAL(0, node);

                u16 node2 = 0;
                CHECK_TRUE(range.allocate(1, node2));
                CHECK_EQUAL(1, node2);

                u16 node3 = 0;
                CHECK_TRUE(range.allocate(2, node3));
                CHECK_EQUAL(2, node3);

                CHECK_TRUE(range.deallocate(node));
                CHECK_TRUE(range.deallocate(node2));
                CHECK_TRUE(range.deallocate(node3));
            }

            range.teardown(Allocator);
        }
    }
}
