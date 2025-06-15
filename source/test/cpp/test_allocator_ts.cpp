#include "ccore/c_allocator.h"
#include "cbase/c_runes.h"

#include "callocator/c_allocator_ts.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(nts)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(simple)
        {
            nts::allocation_t allocations[] = {
                {0, 64, 512, 0, 0},
                {0, 64, 1024, 1, 0},
                {0, 64, 256, 2, 0},
            };

            u32 num_allocations = g_array_size(allocations);
            u32 max_memory_used = nts::g_process_sequence(allocations, num_allocations, Allocator);

            CHECK_EQUAL(1792, max_memory_used);

            CHECK_EQUAL(1, allocations[0].index);
            CHECK_EQUAL(0, allocations[1].index);
            CHECK_EQUAL(2, allocations[2].index);

            CHECK_EQUAL(0, allocations[0].address);
            CHECK_EQUAL(1024, allocations[1].address);
            CHECK_EQUAL(1536, allocations[2].address);
        }

        UNITTEST_TEST(separate)
        {
            nts::allocation_t allocations[] = {
                {0, 64, 512, 0, 0},
                {0, 64, 1024, 1, 0},
                {64, 128, 256, 2, 0},
            };

            u32 num_allocations = g_array_size(allocations);
            u32 max_memory_used = nts::g_process_sequence(allocations, num_allocations, Allocator);

            CHECK_EQUAL(1536, max_memory_used);

            CHECK_EQUAL(1, allocations[0].index);
            CHECK_EQUAL(0, allocations[1].index);
            CHECK_EQUAL(2, allocations[2].index);

            CHECK_EQUAL(0, allocations[0].address);
            CHECK_EQUAL(1024, allocations[1].address);
            CHECK_EQUAL(1024, allocations[2].address);
        }

        UNITTEST_TEST(fragmentation1)
        {
            nts::allocation_t allocations[] = {
                {0, 64, 512, 0, 0},
                {1, 128, 1024, 1, 0},
                {64, 128, 256, 2, 0},
            };

            u32 num_allocations = g_array_size(allocations);
            u32 max_memory_used = nts::g_process_sequence(allocations, num_allocations, Allocator);

            CHECK_EQUAL(1536, max_memory_used);

            CHECK_EQUAL(0, allocations[0].index);
            CHECK_EQUAL(1, allocations[1].index);
            CHECK_EQUAL(2, allocations[2].index);

            CHECK_EQUAL(0, allocations[0].address);
            CHECK_EQUAL(512, allocations[1].address);
            CHECK_EQUAL(0, allocations[2].address);
        }

        UNITTEST_TEST(fragmentation2)
        {
            nts::allocation_t allocations[] = {
                {0, 4, 1, 0, 0},
                {1, 8, 4, 1, 0},
                {4, 9, 2, 2, 0},
                {9, 12, 1, 3, 0},
            };

            // ASCII ART, time layout
            // ----
            //  -------
            //     -----
            //          ---

            // ASCII ART, sorted into non-overlapping buckets
            // 0 ----
            // 1  -------
            // 0     -----
            // 1          ---

            // ASCII ART, memory layout
            // -
            //   ----
            // --
            // -

            u32 num_allocations = g_array_size(allocations);
            u32 max_memory_used = nts::g_process_sequence(allocations, num_allocations, Allocator);

            CHECK_EQUAL(6, max_memory_used);

            CHECK_EQUAL(0, allocations[0].index);
            CHECK_EQUAL(1, allocations[1].index);
            CHECK_EQUAL(2, allocations[2].index);
            CHECK_EQUAL(3, allocations[3].index);

            CHECK_EQUAL(4, allocations[0].address);
            CHECK_EQUAL(0, allocations[1].address);
            CHECK_EQUAL(4, allocations[2].address);
            CHECK_EQUAL(4, allocations[3].address);
        }
    }
}
UNITTEST_SUITE_END
