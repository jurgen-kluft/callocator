#include "ccore/c_allocator.h"
#include "callocator/c_allocator_frame.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(frame)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        byte* frame_alloc_mem1 = nullptr;
        byte* frame_alloc_mem2 = nullptr;
        byte* frame_alloc_mem3 = nullptr;

        UNITTEST_FIXTURE_SETUP()
        {
            frame_alloc_mem1 = g_allocate_array<byte>(Allocator, 1 * cMB);
            frame_alloc_mem2 = g_allocate_array<byte>(Allocator, 1 * cMB);
            frame_alloc_mem3 = g_allocate_array<byte>(Allocator, 1 * cMB);
        }

        UNITTEST_FIXTURE_TEARDOWN()
        {
            g_deallocate_array(Allocator, frame_alloc_mem1);
            g_deallocate_array(Allocator, frame_alloc_mem2);
            g_deallocate_array(Allocator, frame_alloc_mem3);
        }

        UNITTEST_TEST(alloc3_free3)
        {
            frame_allocator_t alloc;

            alloc.setup(3, 1024 * 1024, 16 * cMB);

            s32 frame_ids[3];

            for (s32 i = 0; i < 12; ++i)
            {
                if (i >= 3)
                {
                    const s32 frame_id = frame_ids[(i - 3) % 3];
                    CHECK_TRUE(alloc.reset_frame(frame_id));
                }

                const s32 frame_id = alloc.new_frame();
                frame_ids[i % 3] = frame_id;

                void* mem1 = alloc.allocate(512, 8);
                void* mem2 = alloc.allocate(1024, 16);
                void* mem3 = alloc.allocate(512, 32);
                void* mem4 = alloc.allocate(1024, 256);
                void* mem5 = alloc.allocate(256, 32);
                CHECK_NOT_NULL(mem1);
                CHECK_NOT_NULL(mem2);
                CHECK_NOT_NULL(mem3);
                CHECK_NOT_NULL(mem4);
                CHECK_NOT_NULL(mem5);

                alloc.deallocate(mem4);
                mem4 = nullptr;

                void* mem6 = alloc.allocate(8, 8);
                CHECK_NOT_NULL(mem6);
                alloc.deallocate(mem6);
                mem6 = nullptr;

                void* mem7 = alloc.allocate(2048, 256);
                void* mem8 = alloc.allocate(1024, 256);
                CHECK_NOT_NULL(mem7);
                CHECK_NOT_NULL(mem8);

                alloc.deallocate(mem1);
                mem1 = nullptr;
                alloc.deallocate(mem3);
                mem3 = nullptr;
                alloc.deallocate(mem2);
                mem2 = nullptr;

                void* mem9 = alloc.allocate(16, 8);
                CHECK_NOT_NULL(mem9);

                alloc.deallocate(mem7);
                mem7 = nullptr;
                alloc.deallocate(mem5);
                mem5 = nullptr;

                // This should wrap around
                void* mema = alloc.allocate(2048, 8);
                CHECK_NOT_NULL(mema);

                alloc.deallocate(mem9);
                mem9 = nullptr;
                alloc.deallocate(mem8);
                mem8 = nullptr;
                alloc.deallocate(mema);
                mema = nullptr;

                CHECK_TRUE(alloc.end_frame());
            }
        }
    }
}
UNITTEST_SUITE_END
