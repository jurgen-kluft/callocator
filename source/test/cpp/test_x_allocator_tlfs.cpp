#include "cbase/c_allocator.h"
#include "callocator/c_allocator_tlsf.h"

#include "cunittest/cunittest.h"

using namespace ncore;

extern alloc_t* gSystemAllocator;


UNITTEST_SUITE_BEGIN(x_allocator_tlfs)
{
    UNITTEST_FIXTURE(main)
    {

		void*			gBlock;
		s32				gBlockSize;
		alloc_t*	gCustomAllocator;

        UNITTEST_FIXTURE_SETUP()
		{
			gBlockSize = 4 * 1024 * 1024;
			gBlock = gSystemAllocator->allocate(gBlockSize, 8);
			gCustomAllocator = gCreateTlsfAllocator(gBlock, gBlockSize);
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
			gCustomAllocator->release();
			gSystemAllocator->deallocate(gBlock);
			gBlock = nullptr;
			gBlockSize = 0;
		}

        UNITTEST_TEST(alloc3_free3)
        {
			void* mem1 = gCustomAllocator->allocate(512, 8);
			void* mem2 = gCustomAllocator->allocate(1024, 16);
			void* mem3 = gCustomAllocator->allocate(256, 32);
			gCustomAllocator->deallocate(mem2);
			gCustomAllocator->deallocate(mem1);
			gCustomAllocator->deallocate(mem3);
        }

	}
}
UNITTEST_SUITE_END

