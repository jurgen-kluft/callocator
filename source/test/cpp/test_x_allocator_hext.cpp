#include "xbase/x_allocator.h"
#include "xbase/x_idx_allocator.h"
#include "xallocator/x_allocator_freelist.h"
#include "xallocator/x_allocator_hext.h"

#include "xunittest/xunittest.h"

using namespace xcore;

extern xalloc* gSystemAllocator;


UNITTEST_SUITE_BEGIN(x_allocator_hext)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP()
		{
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
		}


		UNITTEST_TEST(create_release)
        {
			xalloc* a = gCreateHextAllocator(gSystemAllocator, (void*)0x87654000, 128 * 1024 * 1024, 1024, 1 * 1024 * 1024);
			a->release();
        }

		UNITTEST_TEST(allocate1)
		{
			xalloc* a = gCreateHextAllocator(gSystemAllocator, (void*)0x87654000, 128 * 1024 * 1024, 1024, 1 * 1024 * 1024);

			void* p1 = a->allocate(60, 4);
			a->deallocate(p1);

			a->release();
		}
	
		UNITTEST_TEST(allocate2)
		{
			void* base = (void*)0x87654000;
			xalloc* a = gCreateHextAllocator(gSystemAllocator, base, 32 * 1024, 1024, 1 * 1024 * 1024);

			for (s32 i=0; i<32; ++i)
			{
				void* p1 = a->allocate(60, 4);
				CHECK_NOT_NULL(p1);
				void* pp = (void*)((char*)base + i*1024);
				CHECK_EQUAL(pp, p1);
			}

			// Allocator has no memory left so this should fail
			void* p2 = a->allocate(60, 4);
			CHECK_NULL(p2);

			a->release();
		}

		UNITTEST_TEST(allocate3)
		{
			void* base = (void*)0x80000000;
			xalloc* ha = gCreateHextAllocator(gSystemAllocator, base, 1024 * 1024 * 1024, 1024, 1 * 1024 * 1024);

			const int max_tracked_allocs = 1000;
			void*	allocations[max_tracked_allocs];
			for (s32 i = 0; i < max_tracked_allocs; ++i)
			{
				allocations[i] = NULL;
			}

			for (s32 i = 0; i < 10000; ++i)
			{
				for (s32 a = 0; a < 32; ++a)
				{
					void* p1 = ha->allocate(60, 4);
					CHECK_NOT_NULL(p1);
					int alloc_idx = rand() % max_tracked_allocs;
					if (allocations[alloc_idx] != NULL)
					{
						ha->deallocate(allocations[alloc_idx]);
					}
					allocations[alloc_idx] = p1;
				}
			}

			for (s32 i = 0; i < max_tracked_allocs; ++i)
			{
				if (allocations[i] != NULL)
				{
					ha->deallocate(allocations[i]);
				}
			}

			ha->release();
		}
	}
}
UNITTEST_SUITE_END
