#include "cbase/c_target.h"
#include "cbase/c_allocator.h"
#include "callocator/c_fsadexed_array.h"

#include "cunittest/xunittest.h"

using namespace ncore;

extern ncore::alloc_t* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_fsadexed_array)
{
	UNITTEST_FIXTURE(main)
	{
		UNITTEST_FIXTURE_SETUP()
		{
		}

		UNITTEST_FIXTURE_TEARDOWN()
		{
		}

		struct object
		{
			u32		mIndex;
		};

		UNITTEST_TEST(array)
		{
			u32 size = 64;
			fsadexed_t* allocator = gCreateFreeListIdxAllocator(gSystemAllocator, gSystemAllocator, sizeof(object), 8, size);

			for (u32 i=0; i<size; ++i)
			{
                void* obj_mem = allocator->allocate();
                u32   idx     = allocator->ptr2idx(obj_mem); 
				CHECK_NOT_NULL(obj_mem);
				CHECK_EQUAL(i, idx);

				object* obj = (object*)obj_mem;
				obj->mIndex = idx;
			}

			// 2 failure allocations
			for (u32 i=0; i<2; ++i)
			{
                void* obj_mem = allocator->allocate();
                u32   idx     = allocator->ptr2idx(obj_mem);
                CHECK_NULL(obj_mem);
				CHECK_EQUAL(0xffffffff, idx);
			}

			for (u32 i=0; i<64; ++i)
			{
				void* p = allocator->idx2ptr(i);
				CHECK_NOT_NULL(p);
				object* obj = (object*)p;
				CHECK_EQUAL(i, obj->mIndex);
				allocator->deallocate(p);
			}

			allocator->release();
		}
	}
}
UNITTEST_SUITE_END
