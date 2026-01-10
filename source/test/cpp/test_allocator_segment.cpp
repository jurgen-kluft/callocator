#include "cbase/c_allocator.h"
#include "cbase/c_context.h"
#include "cbase/c_integer.h"
#include "cbase/c_memory.h"
#include "ccore/c_random.h"

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

        const u8 s_min_size_shift = 16;
        const u8 s_max_size_shift = 21;
        const u8 s_tot_size_shift = 31;

        const int_t s_min_size   = (int_t)1 << s_min_size_shift;
        const int_t s_max_size   = (int_t)1 << s_max_size_shift;
        const int_t s_total_size = (int_t)1 << s_tot_size_shift;

        UNITTEST_TEST(allocate)
        {
            segment_alloc_t range;
            nsegment::initialize(&range, Allocator, s_min_size, s_max_size, s_total_size);

            s64 ptr[7];
            s64 sizes[7];
            sizes[0] = s_min_size;
            sizes[1] = s_min_size;
            sizes[2] = 2 * s_min_size;
            sizes[3] = 4 * s_min_size;
            sizes[4] = 8 * s_min_size;
            sizes[5] = 16 * s_min_size;
            sizes[6] = 32 * s_min_size;

            CHECK_TRUE(nsegment::allocate(&range, sizes[0], ptr[0]));
            CHECK_EQUAL(0, ptr[0]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[1], ptr[1]));
            CHECK_EQUAL(sizes[1], ptr[1]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[2], ptr[2]));
            CHECK_EQUAL(sizes[2], ptr[2]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[3], ptr[3]));
            CHECK_EQUAL(sizes[3], ptr[3]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[4], ptr[4]));
            CHECK_EQUAL(sizes[4], ptr[4]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[5], ptr[5]));
            CHECK_EQUAL(sizes[5], ptr[5]);

            CHECK_TRUE(nsegment::allocate(&range, sizes[6], ptr[6]));
            CHECK_EQUAL(sizes[6], ptr[6]);

            for (int_t i = 0; i < 7; ++i)
            {
                CHECK_TRUE(nsegment::deallocate(&range, ptr[i], sizes[i]));
            }

            // All free again
            CHECK_EQUAL((u32)(1 << (range.m_num_sizes - 1)), range.m_size_free);

            nsegment::teardown(&range, Allocator);
        }

        // Allocate and deallocate randomly many different sizes and lifetimes
        UNITTEST_TEST(stress_test)
        {
            segment_alloc_t range;
            nsegment::initialize(&range, Allocator, s_min_size, s_max_size, s_total_size);

            const int_t num_allocs = 900;
            s64         ptrs[num_allocs];
            s64         sizes[num_allocs];

            ncore::xor_random_t rng;
            rng.reset(12345);

            for (i32 i = 0; i < num_allocs; ++i)
            {
                // Random size between min_size and max_size
                s32 random_shift = g_random_u32_max(&rng, s_max_size_shift - s_min_size_shift + 1);
                sizes[i]         = ((s64)1 << (s_min_size_shift + random_shift));
                ptrs[i]          = -1;
            }

            // Allocate all
            for (i32 i = 0; i < num_allocs; ++i)
            {
                CHECK_TRUE(nsegment::allocate(&range, sizes[i], ptrs[i]));
            }

            // Shuffle
            i32 random_indices[num_allocs];
            for (i32 i = 0; i < num_allocs; ++i)
            {
                random_indices[i] = i;
            }
            // for (i32 i = 0; i < num_allocs; ++i)
            // {
            //     i32 idx1             = (i32)(rng.rand32() % num_allocs);
            //     i32 tmp              = random_indices[i];
            //     random_indices[i]    = random_indices[idx1];
            //     random_indices[idx1] = tmp;
            // }

            // Deallocate in random order
            for (i32 i = 0; i < num_allocs; ++i)
            {
                i32 idx = random_indices[i];
                CHECK_TRUE(nsegment::deallocate(&range, ptrs[idx], sizes[idx]));
            }

            // All free again
            CHECK_EQUAL((u32)(1 << (range.m_num_sizes - 1)), range.m_size_free);

            nsegment::teardown(&range, Allocator);
        }
    }
}
