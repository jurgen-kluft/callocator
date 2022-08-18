#include "cbase/c_allocator.h"
#include "cbase/c_integer.h"
#include "callocator/c_fsadexed_array.h"

#include "cunittest/cunittest.h"

using namespace ncore;

extern alloc_t* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_allocator_freelist)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP()
		{
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
		}

		static bool gIsAligned(void* p, u32 alignment)
		{
			// We only need the lower bits, so 32 or 64 bits is not an issue here
			u32 bits = (u32)p;
			return ncore::xisAligned(bits, alignment);
		}

		static void gFill(void* p, u32 size, u8 v)
		{
			u8* dst = (u8*)p;
			u8* end = (u8*)p + size;
			while (dst < end)
				*dst++ = v;
		}

		static bool gTest(void* p, u32 size, u8 v)
		{
			u8 const* dst = (u8*)p;
			u8 const* end = (u8*)p + size;
			while (dst < end)
			{
				u8 const b = *dst++;
				if (b != v)
					return false;
			}
			return true;
		}

        UNITTEST_TEST(alloc3_free3)
        {
			u32 const alignment = 2048;
			fsadexed_t* alloc = gCreateFreeListAllocator(gSystemAllocator, alignment, alignment, 128);

			void* mem1 = alloc->allocate();
			CHECK_TRUE(gIsAligned(mem1, alignment));
			void* mem2 = alloc->allocate();
			void* mem3 = alloc->allocate();
			void* mem4 = alloc->allocate();
			void* mem5 = alloc->allocate();
			gFill(mem5, 256, 5);
			gFill(mem3, 512, 3);
			gFill(mem4, 1024, 4);
			gFill(mem1, 512, 1);
			gFill(mem2, 1024, 2);

			CHECK_TRUE(gTest(mem3, 512, 3));
			CHECK_TRUE(gTest(mem4, 1024, 4));
			CHECK_TRUE(gTest(mem1, 512, 1));

			alloc->deallocate(mem4);

			void* mem6 = alloc->allocate();
			void* mem7 = alloc->allocate();
			void* mem8 = alloc->allocate();

			alloc->deallocate(mem1);
			alloc->deallocate(mem3);
			alloc->deallocate(mem2);

			void* mem9 = alloc->allocate();

			alloc->deallocate(mem7);
			alloc->deallocate(mem5);
			alloc->deallocate(mem8);
			alloc->deallocate(mem9);
			alloc->deallocate(mem6);

			alloc->release();
        }

        UNITTEST_TEST(alloc3_free3_idx)
        {
			u32 const alignment = 2048;
            fsadexed_t* alloc     = gCreateFreeListIdxAllocator(gSystemAllocator, alignment, alignment, 128);

			void* mem1 = alloc->allocate();
			CHECK_TRUE(gIsAligned(mem1, alignment));
			void* mem2 = alloc->allocate();
			void* mem3 = alloc->allocate();
			void* mem4 = alloc->allocate();

			void* mem5 = alloc->allocate();
			gFill(mem5, 256, 5);
			gFill(mem3, 512, 3);
			gFill(mem4, 1024, 4);
			gFill(mem1, 512, 1);
			gFill(mem2, 1024, 2);

			CHECK_TRUE(gTest(mem3, 512, 3));
			CHECK_TRUE(gTest(mem4, 1024, 4));
			CHECK_TRUE(gTest(mem1, 512, 1));

			alloc->deallocate(mem4);

			void* mem6 = alloc->allocate();
			void* mem7 = alloc->allocate();
			void* mem8 = alloc->allocate();

			alloc->deallocate(mem1);
			alloc->deallocate(mem3);
			alloc->deallocate(mem2);

			void* mem9 = alloc->allocate();

			alloc->deallocate(mem7);
			alloc->deallocate(mem5);
			alloc->deallocate(mem8);
			alloc->deallocate(mem9);
			alloc->deallocate(mem6);

			alloc->release();
        }
	}
}
UNITTEST_SUITE_END
