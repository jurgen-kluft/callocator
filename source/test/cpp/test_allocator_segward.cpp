#include "callocator/c_allocator_segward.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(segward)
{
    UNITTEST_FIXTURE(main)
    {
        // UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(create)
        {
            nsegward::allocator_t* allocator = nsegward::create(64 * 1024, 8 * 1024 * 1024);
            CHECK_NOT_NULL(allocator);
            nsegward::destroy(allocator);
        }

        // Helper function to write pseudo-random data to allocated memory at random locations
        static void random_writes(void* ptr, u32 size, u32 num_writes)
        {
            u8* byte_ptr = (u8*)ptr;
            u32 seed     = (u32)(u64)ptr + 123456789;
            for (u32 i = 0; i < num_writes; ++i)
            {
                seed                  = (1103515245 * seed + 12345) & 0x7fffffff;
                byte_ptr[seed % size] = (u8)(seed & 0xFF);
            }
        }

        UNITTEST_TEST(alloc_free)
        {
            nsegward::allocator_t* allocator = nsegward::create(64 * 1024, 8 * 1024 * 1024);
            CHECK_NOT_NULL(allocator);

            void* ptr1 = nsegward::allocate(allocator, 1024, 16);
            CHECK_NOT_NULL(ptr1);
            // random_writes(ptr1, 1024, 64);
            nsegward::deallocate(allocator, ptr1);

            nsegward::destroy(allocator);
        }

        // 1) Alignment correctness across a variety of alignments
        UNITTEST_TEST(alloc_alignment)
        {
            nsegward::allocator_t* allocator = nsegward::create(1024 * 1024, 256 * 1024 * 1024);
            CHECK_NOT_NULL(allocator);

            const u32 sizes[]      = {1, 7, 13, 64, 1024, 4096};
            const u32 alignments[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 4096};

            for (u32 s = 0; s < DARRAYSIZE(sizes); ++s)
            {
                for (u32 a = 0; a < DARRAYSIZE(alignments); ++a)
                {
                    void* p = nsegward::allocate(allocator, sizes[s], alignments[a]);
                    CHECK_NOT_NULL(p);

                    // Check pointer alignment
                    CHECK_TRUE(((uptr_t)p % alignments[a]) == 0);

                    // Free immediately to keep per-segment counters balanced
                    nsegward::deallocate(allocator, p);
                }
            }

            nsegward::destroy(allocator);
        }

        // 2) Forward allocations fill a segment and then roll to the next
        UNITTEST_TEST(alloc_free_forward)
        {
            const u32              kSegSize  = 64 * 1024;
            nsegward::allocator_t* allocator = nsegward::create(kSegSize, 8 * 1024 * 1024);
            CHECK_NOT_NULL(allocator);

            // Allocate 64 * 1KB => roughly within one segment
            void* p1[64];
            for (u32 i = 0; i < 64; ++i)
            {
                p1[i] = nsegward::allocate(allocator, i == 0 ? 512 : 1024, 16);
                CHECK_NOT_NULL(p1[i]);
            }

            // Next allocation should move to the next segment and still succeed
            void* p5 = nsegward::allocate(allocator, 1024, 16);
            CHECK_NOT_NULL(p5);

            // Free all; segment(s) should retire and be reusable
            for (u32 i = 0; i < 64; ++i)
            {
                nsegward::deallocate(allocator, p1[i]);
            }
            nsegward::deallocate(allocator, p5);

            // Reuse after retirement: fresh allocation should succeed
            void* p6 = nsegward::allocate(allocator, 1024, 16);
            CHECK_NOT_NULL(p6);
            nsegward::deallocate(allocator, p6);

            nsegward::destroy(allocator);
        }

        // 3) Out-of-memory when all 3 segments are full, then recovery after freeing
        UNITTEST_TEST(out_of_memory_and_reuse)
        {
            const u32 kSegSize = 64 * 1024;
            // Total size just enough for 3 segments; create() requires >=3 segments.
            nsegward::allocator_t* allocator = nsegward::create(kSegSize, 3 * kSegSize);
            CHECK_NOT_NULL(allocator);

            const u32 alloc_size = 256;

            // Fill each segment with allocations
            void* s1[64];
            for (i32 i = 0; i < 64; ++i)
            {
                s1[i] = nsegward::allocate(allocator, alloc_size, 1024);
                CHECK_NOT_NULL(s1[i]);
            }

            void* s2[64];
            for (i32 i = 0; i < 64; ++i)
            {
                s2[i] = nsegward::allocate(allocator, alloc_size, 1024);
                CHECK_NOT_NULL(s2[i]);
            }

            void* s3[64];
            for (i32 i = 0; i < 64; ++i)
            {
                s3[i] = nsegward::allocate(allocator, alloc_size, 1024);
                CHECK_NOT_NULL(s3[i]);
            }

            // Next allocation should fail (all segments active and fully consumed)
            void* fail = nsegward::allocate(allocator, 1024, 16);
            CHECK_NULL(fail);

            // Free one large allocation; its segment should retire and be reusable
            for (i32 i = 0; i < 64; ++i)
            {
                nsegward::deallocate(allocator, s2[i]);
            }

            // Now we should be able to allocate again successfully
            void* ok = nsegward::allocate(allocator, 1024, 16);
            CHECK_NOT_NULL(ok);
            CHECK_EQUAL(s2[0], ok); // should reuse the freed segment

            // Cleanup remaining allocations
            for (i32 i = 0; i < 64; ++i)
            {
                nsegward::deallocate(allocator, s1[i]);
            }
            for (i32 i = 0; i < 64; ++i)
            {
                nsegward::deallocate(allocator, s3[i]);
            }

            nsegward::deallocate(allocator, ok);
            nsegward::destroy(allocator);
        }

        // 4) Interleaved allocations across segments and reverse frees
        UNITTEST_TEST(interleaved_alloc_free)
        {
            const u32              kSegSize  = 64 * 1024;
            nsegward::allocator_t* allocator = nsegward::create(kSegSize, 4 * kSegSize);
            CHECK_NOT_NULL(allocator);

            // Force multiple segments by allocating near 1/2 segment each

            // segment 0
            void* a0[64];
            for (i32 i = 0; i < 64; ++i)
            {
                a0[i] = nsegward::allocate(allocator, 1024, 16);
                CHECK_NOT_NULL(a0[i]);
            }

            // segment 1
            void* a1[64];
            for (i32 i = 0; i < 64; ++i)
            {
                a1[i] = nsegward::allocate(allocator, 1024, 16);
                CHECK_NOT_NULL(a1[i]);
            }

            // segment 2
            void* a2[64];
            for (i32 i = 0; i < 64; ++i)
            {
                a2[i] = nsegward::allocate(allocator, 1024, 16);
                CHECK_NOT_NULL(a2[i]);
            }

            // segment 3
            void* a3[64];
            for (i32 i = 0; i < 64; ++i)
            {
                a3[i] = nsegward::allocate(allocator, 1024, 16);
                CHECK_NOT_NULL(a3[i]);
            }

            // Reverse free order; counters should underflow-check and retire when zero

            // segment 3
            for (i32 i = 0; i < 64; i++)
            {
                nsegward::deallocate(allocator, a3[i]);
            }
            // segment 2
            for (i32 i = 0; i < 64; i++)
            {
                nsegward::deallocate(allocator, a2[i]);
            }
            // segment 1
            for (i32 i = 0; i < 64; i++)
            {
                nsegward::deallocate(allocator, a1[i]);
            }
            // segment 0
            for (i32 i = 0; i < 64; i++)
            {
                nsegward::deallocate(allocator, a0[i]);
            }

            // After frees, allocator should be able to serve new allocations again
            void* d1 = nsegward::allocate(allocator, 1024, 16);
            CHECK_NOT_NULL(d1);
            CHECK_TRUE(((uptr_t)d1 % 16) == 0);
            nsegward::deallocate(allocator, d1);

            nsegward::destroy(allocator);
        }
    }
}
UNITTEST_SUITE_END
