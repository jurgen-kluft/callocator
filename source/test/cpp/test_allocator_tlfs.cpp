#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "callocator/c_allocator_tlsf.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(x_allocator_tlfs)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        void* gBlock;
        s32   gBlockSize;

        UNITTEST_FIXTURE_SETUP()
        {
            gBlockSize                = 256 * 1024 * 1024;
            gBlock                    = Allocator->allocate(gBlockSize, 8);
            alloc_t* gCustomAllocator = g_create_tlsf(gBlock, gBlockSize);
        }

        UNITTEST_FIXTURE_TEARDOWN()
        {
            Allocator->deallocate(gBlock);
            gBlock     = nullptr;
            gBlockSize = 0;
        }

        UNITTEST_TEST(alloc3_free3)
        {
            alloc_t* gCustomAllocator = g_create_tlsf(gBlock, gBlockSize);
            void*    mem1             = gCustomAllocator->allocate(512, 8);
            void*    mem2             = gCustomAllocator->allocate(1024, 16);
            void*    mem3             = gCustomAllocator->allocate(256, 32);
            gCustomAllocator->deallocate(mem2);
            gCustomAllocator->deallocate(mem1);
            gCustomAllocator->deallocate(mem3);
        }

        // static void random_test(alloc_t * t, uint_t spacelen, const uint_t cap)
        static uint_t s_holdrand = 1;
        static void   s_srand(uint_t seed) { s_holdrand = seed; }
        static uint_t s_rand() { return (((s_holdrand = s_holdrand * 214013L + 2531011L) >> 16) & 0x7fffffff); }

        UNITTEST_TEST(random_sizes_test)
        {
            const uint_t sizes[] = {16, 32, 64, 128, 256, 512, 1024};

            alloc_t* tlsf = g_create_tlsf(gBlock, gBlockSize);

            for (s32 i = 0; i < g_array_size(sizes); i++)
            {
                s32 n = 1024;

                s_srand((uint_t)sizes[i]);

                while (n--)
                {
                    uint_t cap      = (uint_t)s_rand() % sizes[i] + 1;
                    uint_t spacelen = sizes[i];
                    {
                        const uint_t maxitems = 2 * spacelen;

                        void** p = (void**)Allocator->allocate(maxitems * sizeof(void*));
                        CHECK_NOT_NULL(p);

                        // Allocate random sizes up to the cap threshold.
                        // Track them in an array.
                        s64 rest = (s64)spacelen * (s_rand() % 6 + 1);
                        s32 i    = 0;
                        while (rest > 0)
                        {
                            uint_t len = ((uint_t)s_rand() % cap) + 1;
                            if (s_rand() % 2 == 0)
                            {
                                p[i] = tlsf->allocate(len);
                            }
                            else
                            {
                                uint_t align = 1U << (s_rand() % 7);
                                if (cap < align)
                                    align = 0;
                                else
                                    len = align * (((uint_t)s_rand() % (cap / align)) + 1);
                                p[i] = !align || !len ? tlsf->allocate(len) : tlsf->allocate(len, align);
                                if (align)
                                {
                                    uint_t const aligned = ((uint_t)p[i] & (align - 1));
                                    CHECK_EQUAL(0, aligned);
                                }
                            }
                            CHECK_NOT_NULL(p[i]);
                            rest -= (s64)len;

                            if (s_rand() % 10 == 0)
                            {
                                uint_t newlen = ((uint_t)s_rand() % cap) + 1;
                                p[i]          = g_reallocate(tlsf, p[i], len, newlen);
                                CHECK_NOT_NULL(p[i]);
                                len = newlen;
                            }

                            // tlsf check();

                            // Fill with magic (only when testing up to 1MB).
                            u8* data = (u8*)p[i];
                            if (spacelen <= 1024 * 1024)
                                nmem::memset(data, 0, len);
                            data[0] = 0xa5;

                            if (i++ == maxitems)
                                break;
                        }

                        // Randomly deallocate the memory blocks until all of them are freed.
                        // The free space should match the free space after initialisation.
                        for (s32 n = i; n;)
                        {
                            uint_t target = (uint_t)s_rand() % i;
                            if (p[target] == nullptr)
                                continue;
                            u8* data = (u8*)p[target];
                            CHECK_TRUE(data[0] == 0xa5);
                            tlsf->deallocate(p[target]);
                            p[target] = nullptr;
                            n--;

                            // tlsf_check(t);
                        }

                        Allocator->deallocate(p);
                    }
                }
            }
        }
    }
}
UNITTEST_SUITE_END
