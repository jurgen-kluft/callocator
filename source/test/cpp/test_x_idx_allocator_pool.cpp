#include "xbase\x_target.h"
#include "xbase\x_idx_allocator.h"

#include "xallocator\x_idx_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern xcore::x_iallocator* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_idx_allocator_pool)
{
	UNITTEST_FIXTURE(main)
	{
		UNITTEST_FIXTURE_SETUP()
		{
		}

		UNITTEST_FIXTURE_TEARDOWN()
		{
		}

		UNITTEST_TEST(pool_1_1)
		{
			x_iidx_allocator* allocator = gCreatePoolIdxAllocator(gSystemAllocator, sizeof(f64), 8);
			allocator->init();
			allocator->clear();
			allocator->release();
		}

		UNITTEST_TEST(pool_1_1_alloc)
		{
			u32 size = 50;
			x_iidx_allocator* allocator = gCreatePoolIdxAllocator(gSystemAllocator, sizeof(f64), 8);
			allocator->init();

			CHECK_EQUAL(0x7ffffff0, allocator->max_size());
			CHECK_EQUAL(0, allocator->size());
			for (u32 i=0; i<size; ++i)
			{
				void* obj_mem;
				u32 idx = allocator->iallocate(obj_mem);
				CHECK_NOT_NULL(obj_mem);
				CHECK_EQUAL(i, idx);

				f64* obj = (f64*)obj_mem;
				*obj = i * 1.0;
			}
			CHECK_EQUAL(size, allocator->size());

			for (u32 i=0; i<size; ++i)
			{
				allocator->ideallocate(i);
			}
			CHECK_EQUAL(0, allocator->size());

			allocator->clear();
			allocator->release();
		}

		UNITTEST_TEST(pool_1_2)
		{
			x_iidx_allocator* allocator = gCreatePoolIdxAllocator(gSystemAllocator, gSystemAllocator, gSystemAllocator, sizeof(f32), 4, 32, 1, 1, 1);
			allocator->init();
			allocator->clear();
			allocator->release();
		}

		struct myobject
		{
			u32		mIndex;
		};

		UNITTEST_TEST(pool_1_3)
		{
			u32 size = 64;
			x_iidx_allocator* allocator = gCreatePoolIdxAllocator(gSystemAllocator, sizeof(myobject), 8, 16, 1, 1, 1);
			allocator->init();
			CHECK_EQUAL(0, allocator->size());

			for (u32 i=0; i<size; ++i)
			{
				void* obj_mem;
				u32 idx = allocator->iallocate(obj_mem);

				CHECK_NOT_NULL(obj_mem);
				CHECK_EQUAL(i, idx);
				CHECK_EQUAL(i+1, allocator->size());

				myobject* obj = (myobject*)obj_mem;
				obj->mIndex = idx;
			}
			CHECK_EQUAL(size, allocator->size());

			void* null_ptr = allocator->to_ptr(size);
			CHECK_NULL(null_ptr);
			null_ptr = allocator->to_ptr(allocator->to_idx(NULL));
			CHECK_NULL(null_ptr);

			// Check allocated items
			for (u32 i=size-1; i!=0xffffffff; --i)
			{
				void* p = allocator->to_ptr(i);
				CHECK_NOT_NULL(p);
				myobject* obj = (myobject*)p;
				CHECK_EQUAL(i, obj->mIndex);
			}

			// Reversed item deallocation
			for (u32 i=size-1; i!=0xffffffff; --i)
			{
				void* p = allocator->to_ptr(i);
				CHECK_NOT_NULL(p);
				myobject* obj = (myobject*)p;
				CHECK_EQUAL(i, obj->mIndex);
				allocator->ideallocate(obj->mIndex);
				CHECK_EQUAL(i, allocator->size());
			}

			allocator->clear();
			allocator->release();
		}
	}
}
UNITTEST_SUITE_END
