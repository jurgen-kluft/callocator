#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "callocator/c_allocator_heap.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(heap)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        s32 gInitSize;
        s32 gBlockSize;

        UNITTEST_FIXTURE_SETUP()
        {
            gInitSize  = 32 * 1024 * 1024;  // 32 MB
            gBlockSize = 256 * 1024 * 1024; // 256 MB

            alloc_t* tlsf = g_create_heap(gInitSize, gBlockSize);

            g_release_heap(tlsf);
        }

        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(alloc3_free3)
        {
            alloc_t* tlsf = g_create_heap(gInitSize, gBlockSize);

            void* mem1 = tlsf->allocate(512, 8);
            void* mem2 = tlsf->allocate(1024, 16);
            void* mem3 = tlsf->allocate(256, 32);

            CHECK_NOT_NULL(mem1);
            CHECK_NOT_NULL(mem2);
            CHECK_NOT_NULL(mem3);

            tlsf->deallocate(mem2);
            tlsf->deallocate(mem1);
            tlsf->deallocate(mem3);

            g_release_heap(tlsf);
        }

        // static void random_test(alloc_t * t, uint_t spacelen, const uint_t cap)
        static uint_t s_holdrand = 1;
        static void   s_srand(uint_t seed) { s_holdrand = seed; }
        static uint_t s_rand() { return (((s_holdrand = s_holdrand * 214013L + 2531011L) >> 16) & 0x7fffffff); }

        UNITTEST_TEST(random_sizes_test)
        {
            const uint_t sizes[] = {16, 32, 64, 128, 256, 512, 1024};

            alloc_t* tlsf = g_create_heap(gInitSize, gBlockSize);

            for (u32 i = 0; i < g_array_size(sizes); i++)
            {
                s32 n = 1024;

                s_srand((uint_t)sizes[i]);

                while (n--)
                {
                    uint_t cap      = (uint_t)s_rand() % sizes[i] + 1;
                    uint_t spacelen = sizes[i];
                    {
                        const s32 maxitems = (s32)(2 * spacelen);

                        void** pointers = g_allocate_array_and_clear<void*>(Allocator, maxitems);
                        CHECK_NOT_NULL(pointers);

                        // Allocate random sizes up to the cap threshold.
                        // Track them in an array.
                        s64 rest = (s64)spacelen * (s_rand() % 6 + 1);
                        s32 p    = 0;
                        while (rest > 0)
                        {
                            uint_t len = ((uint_t)s_rand() % cap) + 1;
                            if (s_rand() % 2 == 0)
                            {
                                pointers[p] = tlsf->allocate((u32)(len));
                            }
                            else
                            {
                                u32 align = 1U << (s_rand() % 7);
                                if (cap < align)
                                    align = 0;
                                else
                                    len = align * (((uint_t)s_rand() % (cap / align)) + 1);
                                pointers[p] = !align || !len ? tlsf->allocate((u32)len) : tlsf->allocate((u32)(len), align);
                                if (align)
                                {
                                    uint_t const aligned = ((uint_t)pointers[p] & (align - 1));
                                    CHECK_EQUAL((uint_t)0, aligned);
                                }
                            }
                            CHECK_NOT_NULL(pointers[p]);
                            rest -= (s64)len;

                            if (s_rand() % 10 == 0)
                            {
                                uint_t newlen = ((uint_t)s_rand() % cap) + 1;
                                pointers[p]   = g_reallocate(tlsf, pointers[p], (u32)len, (u32)newlen);
                                CHECK_NOT_NULL(pointers[p]);
                                len = newlen;
                            }

                            // tlsf check();

                            // Fill with magic (only when testing up to 1MB).
                            u8* data = (u8*)pointers[p];
                            if (spacelen <= 1024 * 1024)
                                nmem::memset(data, 0, len);
                            data[0] = 0xa5;

                            ++p;
                            if (p == maxitems)
                                break;
                        }
                        --p;

                        // Randomly deallocate the memory blocks until all of them are freed.
                        // The free space should match the free space after initialisation.
                        for (s32 r = p; r > 0;)
                        {
                            uint_t target = (uint_t)s_rand() % p;
                            if (pointers[target] == nullptr)
                                continue;
                            u8* data = (u8*)pointers[target];
                            CHECK_TRUE(data[0] == 0xa5);
                            tlsf->deallocate(pointers[target]);
                            pointers[target] = nullptr;
                            r--;
                            // tlsf_check(t);
                        }

                        Allocator->deallocate(pointers);
                    }
                }
            }

            g_release_heap(tlsf);
        }
    }
}
UNITTEST_SUITE_END
