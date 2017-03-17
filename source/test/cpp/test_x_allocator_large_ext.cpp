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
			gIdxAllocator = gCreateFreeListIdxAllocator(gSystemAllocator, 2048, 2048, 128);
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
			gIdxAllocator->release();
		}

        UNITTEST_TEST(advance_ptr1)
        {
			xexternal::memptr ptr1 = (xexternal::memptr)0x4000;
			xexternal::memptr ptr2 = xexternal::advance_ptr(ptr1, 0x100);
			CHECK_EQUAL((xexternal::memptr)0x4100, ptr2);
			xexternal::memptr ptr3 = xexternal::advance_ptr(ptr1, 0x1000);
			CHECK_EQUAL((xexternal::memptr)0x5000, ptr3);
		}

		UNITTEST_TEST(align_ptr1)
        {
			xexternal::memptr ptr1 = (xexternal::memptr)0x4010;
			xexternal::memptr ptr2 = xexternal::align_ptr(ptr1, 0x100);
			CHECK_EQUAL((xexternal::memptr)0x4100, ptr2);
			xexternal::memptr ptr3 = xexternal::align_ptr(ptr1, 0x10);
			CHECK_EQUAL((xexternal::memptr)0x4010, ptr3);
		}

		UNITTEST_TEST(mark_ptr_01)
        {
			xexternal::memptr ptr1 = (xexternal::memptr)0x4010;
			xexternal::memptr ptr2 = xexternal::mark_ptr_0(ptr1, 4);
			CHECK_EQUAL((xexternal::memptr)0x4000, ptr2);
		}

		UNITTEST_TEST(mark_ptr_11)
        {
			xexternal::memptr ptr1 = (xexternal::memptr)0x4000;
			xexternal::memptr ptr2 = xexternal::mark_ptr_1(ptr1, 4);
			CHECK_EQUAL((xexternal::memptr)0x4010, ptr2);
		}

		UNITTEST_TEST(get_ptr_mark1)
        {
			xexternal::memptr ptr1 = (xexternal::memptr)0x4010;
			bool mark1 = xexternal::get_ptr_mark(ptr1, 1);
			CHECK_FALSE(mark1);
			bool mark2 = xexternal::get_ptr_mark(ptr1, 4);
			CHECK_TRUE(mark2);
		}

		UNITTEST_TEST(get_ptr1)
        {
			xexternal::memptr ptr1 = (xexternal::memptr)0x4010;
			xexternal::memptr ptr2 = xexternal::get_ptr(ptr1, 5);
			CHECK_EQUAL((xexternal::memptr)0x4000, ptr2);
		}

		UNITTEST_TEST(diff_ptr1)
		{
			xexternal::memptr ptr1 = (xexternal::memptr)0x00000;
			xexternal::memptr ptr2 = (xexternal::memptr)0x00010;
			uptr d1 = xexternal::diff_ptr(ptr1, ptr2);
			CHECK_EQUAL(0x00010, d1);
		}

		UNITTEST_TEST(init1)
        {
			xexternal::xlargebin sb;
			CHECK_EQUAL(0, gIdxAllocator->size());
			sb.init((void*)0x80000000, 65536, 64, 4, gIdxAllocator);
			CHECK_EQUAL(5, gIdxAllocator->size());
			sb.release();
			CHECK_EQUAL(0, gIdxAllocator->size());
        }

		UNITTEST_TEST(allocate1)
		{
			xexternal::xlargebin sb;
			CHECK_EQUAL(0, gIdxAllocator->size());
			sb.init((void*)0x80000000, 65536, 64, 4, gIdxAllocator);
			CHECK_EQUAL(5, gIdxAllocator->size());

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
			CHECK_EQUAL(5, gIdxAllocator->size());

			for (s32 i=0; i<32; ++i)
			{
				void* p1 = sb.allocate(60, 4);
				CHECK_NOT_NULL(p1);
				void* pp = (void*)((char*)base + i*2048);
				CHECK_EQUAL(pp, p1);
			}
			// Last allocation caused the allocator to deplete the memory so it
			// did not have to allocate a 'split' node.
			CHECK_EQUAL(32-1+5, gIdxAllocator->size());

			// Allocator has no memory left so this should fail
			void* p2 = sb.allocate(60, 4);
			CHECK_NULL(p2);

			sb.release();
			CHECK_EQUAL(0, gIdxAllocator->size());
		}	
	}
}
UNITTEST_SUITE_END
