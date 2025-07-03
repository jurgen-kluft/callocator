#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "callocator/c_allocator_linear.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(linear)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        const u32 alloc_committed_size = 16 * cKB;
        const u32 alloc_reserved_size  = 128 * cKB;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(alloc3_free3)
        {
            linear_alloc_t* alloc = g_create_linear_allocator(alloc_committed_size, alloc_reserved_size);

            for (s32 i = 0; i < 16; ++i)
            {
                CHECK_EQUAL(0, g_current_size(alloc));

                {
                    void* mema = alloc->allocate(1 * cKB, 16);
                    CHECK_TRUE(nmem::ptr_is_aligned(mema, 16));
                    void* memb = alloc->allocate(512, 32);
                    CHECK_TRUE(nmem::ptr_is_aligned(memb, 32));
                    alloc->deallocate(mema);
                    alloc->deallocate(memb);
                }

                void* mem1 = alloc->allocate(512, 8);
                CHECK_NOT_NULL(mem1);
                CHECK_TRUE(nmem::ptr_is_aligned(mem1, 8));
                alloc->deallocate(mem1);
                mem1 = nullptr;

                void* mem2 = alloc->allocate(1 * cKB, 16);
                void* mem3 = alloc->allocate(512, 32);
                void* mem4 = alloc->allocate(1 * cKB, 256);
                void* mem5 = alloc->allocate(256, 32);
                CHECK_NOT_NULL(mem2);
                CHECK_NOT_NULL(mem3);
                CHECK_NOT_NULL(mem4);
                CHECK_NOT_NULL(mem5);
                CHECK_TRUE(nmem::ptr_is_aligned(mem2, 16));
                CHECK_TRUE(nmem::ptr_is_aligned(mem3, 32));
                CHECK_TRUE(nmem::ptr_is_aligned(mem4, 256));
                CHECK_TRUE(nmem::ptr_is_aligned(mem5, 32));

                alloc->deallocate(mem4);
                mem4 = nullptr;

                void* mem6 = alloc->allocate(8, 8);
                CHECK_NOT_NULL(mem6);
                CHECK_TRUE(nmem::ptr_is_aligned(mem6, 8));
                alloc->deallocate(mem6);
                mem6 = nullptr;

                void* mem7 = alloc->allocate(2 * cKB, 256);
                void* mem8 = alloc->allocate(1 * cKB, 256);
                CHECK_NOT_NULL(mem7);
                CHECK_NOT_NULL(mem8);
                CHECK_TRUE(nmem::ptr_is_aligned(mem7, 256));
                CHECK_TRUE(nmem::ptr_is_aligned(mem8, 256));

                alloc->deallocate(mem3);
                mem3 = nullptr;
                alloc->deallocate(mem2);
                mem2 = nullptr;

                void* mem9 = alloc->allocate(16, 8);
                CHECK_NOT_NULL(mem9);
                CHECK_TRUE(nmem::ptr_is_aligned(mem9, 8));

                alloc->deallocate(mem7);
                mem7 = nullptr;
                alloc->deallocate(mem5);
                mem5 = nullptr;

                void* memfull = alloc->allocate(2 * cKB, 8);
                CHECK_NOT_NULL(memfull);
                CHECK_TRUE(nmem::ptr_is_aligned(memfull, 8));

                // This should not fail, since the allocator has wrapped around
                void* mema = alloc->allocate(2 * cKB, 8);
                CHECK_NOT_NULL(mema);

                alloc->deallocate(mem9);
                mem9 = nullptr;
                alloc->deallocate(mem8);
                mem8 = nullptr;
                alloc->deallocate(memfull);
                memfull = nullptr;
                alloc->deallocate(mema);
                mema = nullptr;

                alloc->reset();
            }

            CHECK_EQUAL(0, g_current_size(alloc));
        }
    }
}
UNITTEST_SUITE_END
