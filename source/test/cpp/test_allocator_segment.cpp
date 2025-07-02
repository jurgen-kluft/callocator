#include "cbase/c_allocator.h"
#include "cbase/c_context.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"

#include "callocator/c_allocator_segment.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(segmented)
{
    UNITTEST_FIXTURE(segment_node_based)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        const int_t s_min_size_u8   = 64 * cKB;
        const int_t s_max_size_u8   = 2 * cMB;
        const int_t s_total_size_u8 = 2 * cGB;

        UNITTEST_TEST(allocate_node8)
        {
            // 32768 nodes
            nsegmented::segment_alloc_t* range = nsegmented::g_create_segment_n_allocator(Allocator, s_min_size_u8, s_max_size_u8, s_total_size_u8);

            s64 ptr = 0;
            CHECK_TRUE(range->allocate(s_min_size_u8, ptr));
            CHECK_EQUAL(0, ptr);

            CHECK_TRUE(range->allocate(s_min_size_u8, ptr));
            CHECK_EQUAL(s_min_size_u8, ptr);

            CHECK_TRUE(range->allocate(2 * s_min_size_u8, ptr));
            CHECK_EQUAL(2 * s_min_size_u8, ptr);

            CHECK_TRUE(range->allocate(4 * s_min_size_u8, ptr));
            CHECK_EQUAL(4 * s_min_size_u8, ptr);

            CHECK_TRUE(range->allocate(8 * s_min_size_u8, ptr));
            CHECK_EQUAL(8 * s_min_size_u8, ptr);

            CHECK_TRUE(range->allocate(16 * s_min_size_u8, ptr));
            CHECK_EQUAL(16 * s_min_size_u8, ptr);

            CHECK_TRUE(range->allocate(32 * s_min_size_u8, ptr));
            CHECK_EQUAL(32 * s_min_size_u8, ptr);

            nsegmented::g_teardown(Allocator, range);
        }

        const int_t s_min_size_u16   = 64 * cKB;
        const int_t s_max_size_u16   = 256 * cMB;
        const int_t s_total_size_u16 = 8 * cGB;

        UNITTEST_TEST(allocate_and_deallocate_node16)
        {
            nsegmented::segment_alloc_t* range = nsegmented::g_create_segment_n_allocator(Allocator, s_min_size_u16, s_max_size_u16, s_total_size_u16);

            for (s32 i = 0; i < 16; ++i)
            {
                s64 ptr;
                CHECK_TRUE(range->allocate(s_min_size_u16, ptr));
                CHECK_EQUAL(0*s_min_size_u16, ptr);

                s64 ptr2 = 0;
                CHECK_TRUE(range->allocate(s_min_size_u16, ptr2));
                CHECK_EQUAL(1*s_min_size_u16, ptr2);

                s64 ptr3 = 0;
                CHECK_TRUE(range->allocate(s_min_size_u16, ptr3));
                CHECK_EQUAL(2*s_min_size_u16, ptr3);

                CHECK_TRUE(range->deallocate(ptr, s_min_size_u16));
                CHECK_TRUE(range->deallocate(ptr2, s_min_size_u16));
                CHECK_TRUE(range->deallocate(ptr3, s_min_size_u16));
            }

            nsegmented::g_teardown(Allocator, range);
        }
    }


    UNITTEST_FIXTURE(segment_bitmap_based)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        const int_t s_min_size   = 64 * cKB;
        const int_t s_max_size   = 2 * cMB;
        const int_t s_total_size = 2 * cGB;

        UNITTEST_TEST(allocate)
        {
            nsegmented::segment_alloc_t* range = nsegmented::g_create_segment_b_allocator(Allocator, s_min_size, s_max_size, s_total_size);

            s64 ptr = 0;
            CHECK_TRUE(range->allocate(s_min_size, ptr));
            CHECK_EQUAL(0, ptr);

            CHECK_TRUE(range->allocate(s_min_size, ptr));
            CHECK_EQUAL(s_min_size, ptr);

            CHECK_TRUE(range->allocate(2 * s_min_size, ptr));
            CHECK_EQUAL(2 * s_min_size, ptr);

            CHECK_TRUE(range->allocate(4 * s_min_size, ptr));
            CHECK_EQUAL(4 * s_min_size, ptr);

            CHECK_TRUE(range->allocate(8 * s_min_size, ptr));
            CHECK_EQUAL(8 * s_min_size, ptr);

            CHECK_TRUE(range->allocate(16 * s_min_size, ptr));
            CHECK_EQUAL(16 * s_min_size, ptr);

            CHECK_TRUE(range->allocate(32 * s_min_size, ptr));
            CHECK_EQUAL(32 * s_min_size, ptr);

            nsegmented::g_teardown(Allocator, range);
        }
    }

}
