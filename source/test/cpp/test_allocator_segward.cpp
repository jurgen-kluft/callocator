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

        UNITTEST_TEST(alloc_free)
        {
            nsegward::allocator_t* allocator = nsegward::create(64 * 1024, 8 * 1024 * 1024);
            CHECK_NOT_NULL(allocator);

            void* ptr1 = nsegward::allocate(allocator, 1024, 16);
            CHECK_NOT_NULL(ptr1);
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

            // Allocate 4 * 16KB => roughly within one segment
            void* p1 = nsegward::allocate(allocator, 14 * 1024, 16);
            void* p2 = nsegward::allocate(allocator, 16 * 1024, 16);
            void* p3 = nsegward::allocate(allocator, 16 * 1024, 16);
            void* p4 = nsegward::allocate(allocator, 16 * 1024, 16);

            CHECK_NOT_NULL(p1);
            CHECK_NOT_NULL(p2);
            CHECK_NOT_NULL(p3);
            CHECK_NOT_NULL(p4);

            // Next allocation should move to the next segment and still succeed
            void* p5 = nsegward::allocate(allocator, 4096, 16);
            CHECK_NOT_NULL(p5);

            // Free all; segment(s) should retire and be reusable
            nsegward::deallocate(allocator, p1);
            nsegward::deallocate(allocator, p2);
            nsegward::deallocate(allocator, p3);
            nsegward::deallocate(allocator, p4);
            nsegward::deallocate(allocator, p5);

            // Reuse after retirement: fresh allocation should succeed
            void* p6 = nsegward::allocate(allocator, 8 * 1024, 64);
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

            // Fill each segment with one large allocation
            void* s1[64];
            for (i32 i = 0; i < 64; ++i)
            {
                s1[i] = nsegward::allocate(allocator, alloc_size, i>0 ? 1024 : 16);
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
            void* ok = nsegward::allocate(allocator, 2048, 64);
            CHECK_NOT_NULL(ok);

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
            void* a1 = nsegward::allocate(allocator, 32 * 1024, 64);
            void* a2 = nsegward::allocate(allocator, 32 * 1024, 64); // likely completes segment 0
            void* b1 = nsegward::allocate(allocator, 24 * 1024, 64); // segment 1
            void* b2 = nsegward::allocate(allocator, 24 * 1024, 64); // segment 1
            void* c1 = nsegward::allocate(allocator, 8 * 1024, 64);  // possibly segment 1 or 2

            CHECK_NOT_NULL(a1);
            CHECK_NOT_NULL(a2);
            CHECK_NOT_NULL(b1);
            CHECK_NOT_NULL(b2);
            CHECK_NOT_NULL(c1);

            // Reverse free order; counters should underflow-check and retire when zero
            nsegward::deallocate(allocator, c1);
            nsegward::deallocate(allocator, b2);
            nsegward::deallocate(allocator, b1);
            nsegward::deallocate(allocator, a2);
            nsegward::deallocate(allocator, a1);

            // After frees, allocator should be able to serve new allocations again
            void* d1 = nsegward::allocate(allocator, 16 * 1024, 128);
            CHECK_NOT_NULL(d1);
            CHECK_TRUE(((uptr_t)d1 % 128) == 0);
            nsegward::deallocate(allocator, d1);

            nsegward::destroy(allocator);
        }
    }
}
UNITTEST_SUITE_END
