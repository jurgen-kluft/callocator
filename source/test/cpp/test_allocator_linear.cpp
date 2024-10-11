#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "callocator/c_allocator_linear.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(linear_alloc)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        void*     frame_alloc_mem  = nullptr;
        const u32 frame_alloc_size = 10 * cKB;

        UNITTEST_FIXTURE_SETUP() { frame_alloc_mem = Allocator->allocate(frame_alloc_size); }

        UNITTEST_FIXTURE_TEARDOWN() { Allocator->deallocate(frame_alloc_mem); }

        UNITTEST_TEST(alloc3_free3)
        {
            linear_alloc_t alloc;
            alloc.setup(frame_alloc_mem, frame_alloc_size);
            CHECK_TRUE(alloc.is_empty());

            for (s32 i = 0; i < 16; ++i)
            {
                CHECK_TRUE(alloc.is_empty());

                void* mem1 = alloc.allocate(512, 8);
                CHECK_NOT_NULL(mem1);
                CHECK_TRUE(nmem::ptr_is_aligned(mem1, 8));
                alloc.deallocate(mem1);
                mem1 = nullptr;

                CHECK_TRUE(alloc.is_empty());

                void* mem2 = alloc.allocate(1024, 16);
                void* mem3 = alloc.allocate(512, 32);
                void* mem4 = alloc.allocate(1024, 256);
                void* mem5 = alloc.allocate(256, 32);
                CHECK_NOT_NULL(mem2);
                CHECK_NOT_NULL(mem3);
                CHECK_NOT_NULL(mem4);
                CHECK_NOT_NULL(mem5);
                CHECK_TRUE(nmem::ptr_is_aligned(mem2, 16));
                CHECK_TRUE(nmem::ptr_is_aligned(mem3, 32));
                CHECK_TRUE(nmem::ptr_is_aligned(mem4, 256));
                CHECK_TRUE(nmem::ptr_is_aligned(mem5, 32));

                alloc.deallocate(mem4);
                mem4 = nullptr;

                void* mem6 = alloc.allocate(8, 8);
                CHECK_NOT_NULL(mem6);
                CHECK_TRUE(nmem::ptr_is_aligned(mem6, 8));
                alloc.deallocate(mem6);
                mem6 = nullptr;

                void* mem7 = alloc.allocate(2048, 256);
                void* mem8 = alloc.allocate(1024, 256);
                CHECK_NOT_NULL(mem7);
                CHECK_NOT_NULL(mem8);
                CHECK_TRUE(nmem::ptr_is_aligned(mem7, 256));
                CHECK_TRUE(nmem::ptr_is_aligned(mem8, 256));

                alloc.deallocate(mem3);
                mem3 = nullptr;
                alloc.deallocate(mem2);
                mem2 = nullptr;

                void* mem9 = alloc.allocate(16, 8);
                CHECK_NOT_NULL(mem9);
                CHECK_TRUE(nmem::ptr_is_aligned(mem9, 8));

                alloc.deallocate(mem7);
                mem7 = nullptr;
                alloc.deallocate(mem5);
                mem5 = nullptr;

                void* memfull = alloc.allocate(2 * cKB, 8);
                CHECK_NOT_NULL(memfull);
                CHECK_TRUE(nmem::ptr_is_aligned(memfull, 8));

                // This should fail
                void* mema = alloc.allocate(2 * cKB, 8);
                CHECK_NULL(mema);

                alloc.deallocate(mem9);
                mem9 = nullptr;
                alloc.deallocate(mem8);
                mem8 = nullptr;
                alloc.deallocate(memfull);
                memfull = nullptr;
                alloc.deallocate(mema);
                mema = nullptr;
            }

            CHECK_TRUE(alloc.is_empty());
        }
    }
}
UNITTEST_SUITE_END
