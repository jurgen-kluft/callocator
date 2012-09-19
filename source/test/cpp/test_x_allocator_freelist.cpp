#include "xbase\x_types.h"
#include "xbase\x_allocator.h"
#include "xbase\x_idx_allocator.h"
#include "xbase\x_integer.h"
#include "xallocator\x_allocator_freelist.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;

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
			return xcore::x_intu::isAligned(bits, alignment);
		}

		static void gFill(void* p, u32 size, xbyte v)
		{
			xbyte* dst = (xbyte*)p;
			xbyte* end = (xbyte*)p + size;
			while (dst < end)
				*dst++ = v;
		}

		static bool gTest(void* p, u32 size, xbyte v)
		{
			xbyte const* dst = (xbyte*)p;
			xbyte const* end = (xbyte*)p + size;
			while (dst < end)
			{
				xbyte const b = *dst++;
				if (b != v)
					return false;
			}
			return true;
		}

        UNITTEST_TEST(alloc3_free3)
        {
			u32 const alignment = 2048;
			x_iallocator* alloc = gCreateFreeListAllocator(gSystemAllocator, alignment, alignment, 128);

			void* mem1 = alloc->allocate( 512,    8);
			CHECK_TRUE(gIsAligned(mem1, alignment));

			void* mem2 = alloc->allocate(1024,   16);
			
			void* mem3 = alloc->allocate( 512,   32);

			void* mem4 = alloc->allocate(1024, 1024);

			void* mem5 = alloc->allocate( 256,   32);
			gFill(mem5, 256, 5);
			gFill(mem3, 512, 3);
			gFill(mem4, 1024, 4);
			gFill(mem1, 512, 1);
			gFill(mem2, 1024, 2);

			CHECK_TRUE(gTest(mem3, 512, 3));
			CHECK_TRUE(gTest(mem4, 1024, 4));
			CHECK_TRUE(gTest(mem1, 512, 1));

			alloc->deallocate(mem4);

			void* mem6 = alloc->allocate( 8,    8);
			void* mem7 = alloc->allocate(16, 1024);
			void* mem8 = alloc->allocate(32, 2048);

			alloc->deallocate(mem1);
			alloc->deallocate(mem3);
			alloc->deallocate(mem2);

			void* mem9 = alloc->allocate(16, 8);

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
			x_iallocator* alloc = gCreateFreeListIdxAllocator(gSystemAllocator, alignment, alignment, 128);

			void* mem1 = alloc->allocate( 512,    8);
			CHECK_TRUE(gIsAligned(mem1, alignment));

			void* mem2 = alloc->allocate(1024,   16);
			
			void* mem3 = alloc->allocate( 512,   32);

			void* mem4 = alloc->allocate(1024, 1024);

			void* mem5 = alloc->allocate( 256,   32);
			gFill(mem5, 256, 5);
			gFill(mem3, 512, 3);
			gFill(mem4, 1024, 4);
			gFill(mem1, 512, 1);
			gFill(mem2, 1024, 2);

			CHECK_TRUE(gTest(mem3, 512, 3));
			CHECK_TRUE(gTest(mem4, 1024, 4));
			CHECK_TRUE(gTest(mem1, 512, 1));

			alloc->deallocate(mem4);

			void* mem6 = alloc->allocate( 8,    8);
			void* mem7 = alloc->allocate(16, 1024);
			void* mem8 = alloc->allocate(32, 2048);

			alloc->deallocate(mem1);
			alloc->deallocate(mem3);
			alloc->deallocate(mem2);

			void* mem9 = alloc->allocate(16, 8);

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
