#include "xbase/x_target.h"
#include "xbase/x_idx_allocator.h"
#include "xallocator/x_idx_allocator.h"

#include "xunittest/xunittest.h"

using namespace xcore;

extern xcore::x_iallocator* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_idx_allocator_array)
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
			x_iidx_allocator* allocator = gCreateArrayIdxAllocator(gSystemAllocator, gSystemAllocator, sizeof(object), 8, size);
			allocator->init();

			for (u32 i=0; i<size; ++i)
			{
				void* obj_mem;
				u32 idx = allocator->iallocate(obj_mem);
				CHECK_NOT_NULL(obj_mem);
				CHECK_EQUAL(i, idx);

				object* obj = (object*)obj_mem;
				obj->mIndex = idx;
			}

			// 2 failure allocations
			for (u32 i=0; i<2; ++i)
			{
				void* obj_mem;
				u32 idx = allocator->iallocate(obj_mem);
				CHECK_NULL(obj_mem);
				CHECK_EQUAL(0xffffffff, idx);
			}

			for (u32 i=0; i<64; ++i)
			{
				void* p = allocator->to_ptr(i);
				CHECK_NOT_NULL(p);
				object* obj = (object*)p;
				CHECK_EQUAL(i, obj->mIndex);
				allocator->ideallocate(obj->mIndex);
			}

			allocator->clear();
			allocator->release();
		}
	}
}
UNITTEST_SUITE_END
