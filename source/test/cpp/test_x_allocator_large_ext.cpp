#include "xbase\x_allocator.h"
#include "xbase\x_idx_allocator.h"
#include "xallocator\x_allocator_freelist.h"
#include "xallocator\private\x_largebin.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;


UNITTEST_SUITE_BEGIN(x_allocator_large_ext)
{
    UNITTEST_FIXTURE(main)
    {
		static x_iidx_allocator* gIdxAllocator = NULL;

        UNITTEST_FIXTURE_SETUP()
		{
			gIdxAllocator = gCreateFreeListIdxAllocator(gSystemAllocator, xexternal::xlargebin::sizeof_node(), 8, 65536);
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
			gIdxAllocator->release();
		}


		UNITTEST_TEST(init1)
        {
			xexternal::xlargebin sb;
			CHECK_EQUAL(0, gIdxAllocator->size());
			sb.init((void*)0x80000000, 65536, 64, 4, gIdxAllocator);
			CHECK_EQUAL(4, gIdxAllocator->size());
			sb.release();
			CHECK_EQUAL(0, gIdxAllocator->size());
        }

		UNITTEST_TEST(allocate1)
		{
			xexternal::xlargebin sb;
			CHECK_EQUAL(0, gIdxAllocator->size());
			sb.init((void*)0x80000000, 65536, 64, 4, gIdxAllocator);
			CHECK_EQUAL(4, gIdxAllocator->size());

			void* p1 = sb.allocate(60, 4);
			sb.deallocate(p1);

			sb.release();
			CHECK_EQUAL(0, gIdxAllocator->size());
		}	
	
		UNITTEST_TEST(allocate2)
		{
			xexternal::xlargebin sb;
			void* base = (void*)0x80000000;
			CHECK_EQUAL(0, gIdxAllocator->size());
			sb.init(base, 65536, 2048, 32, gIdxAllocator);
			CHECK_EQUAL(4, gIdxAllocator->size());

			for (s32 j = 0; j < 10; j++)
			{
				for (s32 i = 0; i < 32; ++i)
				{
					void* p1 = sb.allocate(60, 4);
					CHECK_NOT_NULL(p1);
					void* pp = (void*)((char*)base + i * 2048);
					CHECK_EQUAL(pp, p1);
				}
				for (s32 i = 0; i < 32; ++i)
				{
					bool deallocated = sb.deallocate((char*)base + i * 2048);
					CHECK_TRUE(deallocated);
				}
			}
			for (s32 i = 0; i < 32; ++i)
			{
				void* p1 = sb.allocate(60, 4);
				CHECK_NOT_NULL(p1);
				void* pp = (void*)((char*)base + i * 2048);
				CHECK_EQUAL(pp, p1);
			}

			// Last allocation caused the allocator to deplete the memory so it
			// did not have to allocate a 'split' node.
			CHECK_EQUAL(32*2-2+4, gIdxAllocator->size());

			// Allocator has no memory left so this should fail
			void* p2 = sb.allocate(60, 4);
			CHECK_NULL(p2);

			sb.release();
			CHECK_EQUAL(0, gIdxAllocator->size());
		}

		UNITTEST_TEST(allocate3)
		{
			xexternal::xlargebin sb;
			void* base = (void*)0x80000000;
			CHECK_EQUAL(0, gIdxAllocator->size());
			sb.init(base, 3 * 512 * 1024 * 1024, 256, 256, gIdxAllocator);
			CHECK_EQUAL(4, gIdxAllocator->size());

			const int max_tracked_allocs = 16384;
			void*	allocations[max_tracked_allocs];
			for (s32 i = 0; i < max_tracked_allocs; ++i)
			{
				allocations[i] = NULL;
			}

			for (s32 i = 0; i < 100000; ++i)
			{
				for (s32 a = 1; a <= 32; ++a)
				{
					void* p1 = sb.allocate(a * 4096, 4);
					CHECK_NOT_NULL(p1);
					int alloc_idx = rand() % max_tracked_allocs;
					if (allocations[alloc_idx] != NULL)
					{
						bool deallocated = sb.deallocate(allocations[alloc_idx]);
						if (!deallocated)
						{
							CHECK_TRUE(deallocated);
						}
					}
					allocations[alloc_idx] = p1;
				}
			}

			for (s32 i = 0; i < max_tracked_allocs; ++i)
			{
				if (allocations[i] != NULL)
				{
					bool deallocated = sb.deallocate(allocations[i]);
					CHECK_TRUE(deallocated);
				}
			}

			sb.release();
			CHECK_EQUAL(0, gIdxAllocator->size());
		}
	}
}
UNITTEST_SUITE_END
