#include "xbase/x_allocator.h"
#include "xbase/x_idx_allocator.h"
#include "xallocator/x_allocator_freelist.h"
#include "xallocator/private/x_largebin32.h"

#include "xunittest/xunittest.h"

using namespace xcore;

extern xalloc* gSystemAllocator;

class node_allocator : public xalloc
{
public:
	xalloc*		m_allocator;
	u64					m_size;

	u64					size() const { return m_size; }

	virtual const char*	name() const { return "tree node allocator"; }

	virtual void*		allocate(xsize_t size, u32 align)
	{
		m_size += 1;
		return m_allocator->allocate(size, align);
	}

	virtual void*		reallocate(void* p, xsize_t size, u32 align)
	{
		return m_allocator->reallocate(p, size, align);
	}

	virtual void		deallocate(void* p)
	{
		m_size -= 1;
		return m_allocator->deallocate(p);
	}

	virtual void		release() {}
};

UNITTEST_SUITE_BEGIN(x_allocator_large32_ext)
{
    UNITTEST_FIXTURE(main)
    {
		static node_allocator sNodeAllocator;
		static node_allocator* gNodeAllocator;

        UNITTEST_FIXTURE_SETUP()
		{
			sNodeAllocator.m_allocator = gSystemAllocator;
			sNodeAllocator.m_size = 0;
			gNodeAllocator = &sNodeAllocator;
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
			
		}

        UNITTEST_TEST(advance_ptr1)
        {
			xexternal32::memptr ptr1 = (xexternal32::memptr)0x4000;
			xexternal32::memptr ptr2 = xexternal32::advance_ptr(ptr1, 0x100);
			CHECK_EQUAL((xexternal32::memptr)0x4100, ptr2);
			xexternal32::memptr ptr3 = xexternal32::advance_ptr(ptr1, 0x1000);
			CHECK_EQUAL((xexternal32::memptr)0x5000, ptr3);
		}

		UNITTEST_TEST(align_ptr1)
        {
			xexternal32::memptr ptr1 = (xexternal32::memptr)0x4010;
			xexternal32::memptr ptr2 = xexternal32::align_ptr(ptr1, 0x100);
			CHECK_EQUAL((xexternal32::memptr)0x4100, ptr2);
			xexternal32::memptr ptr3 = xexternal32::align_ptr(ptr1, 0x10);
			CHECK_EQUAL((xexternal32::memptr)0x4010, ptr3);
		}

		UNITTEST_TEST(diff_ptr1)
		{
			xexternal32::memptr ptr1 = (xexternal32::memptr)0x00000;
			xexternal32::memptr ptr2 = (xexternal32::memptr)0x00010;
			uptr d1 = xexternal32::diff_ptr(ptr1, ptr2);
			CHECK_EQUAL(0x00010, d1);
		}

		UNITTEST_TEST(init1)
        {
			xexternal32::xlargebin sb;
			CHECK_EQUAL(0, gNodeAllocator->size());
			sb.init((void*)(uptr)0x80000000, 65536, 64, 4, gNodeAllocator);
			CHECK_EQUAL(5, gNodeAllocator->size());
			sb.release();
			CHECK_EQUAL(0, gNodeAllocator->size());
        }

		UNITTEST_TEST(allocate1)
		{
			xexternal32::xlargebin sb;
			CHECK_EQUAL(0, gNodeAllocator->size());
			sb.init((void*)(uptr)0x80000000, 65536, 64, 4, gNodeAllocator);
			CHECK_EQUAL(5, gNodeAllocator->size());

			void* p1 = sb.allocate(60, 4);
			sb.deallocate(p1);

			sb.release();
			CHECK_EQUAL(0, gNodeAllocator->size());
		}	
	
		UNITTEST_TEST(allocate2)
		{
			xexternal32::xlargebin sb;
			void* base = (void*)(uptr)0x80000000;
			CHECK_EQUAL(0, gNodeAllocator->size());
			sb.init(base, 65536, 2048, 32, gNodeAllocator);
			CHECK_EQUAL(5, gNodeAllocator->size());

			for (s32 i=0; i<32; ++i)
			{
				void* p1 = sb.allocate(60, 4);
				CHECK_NOT_NULL(p1);
				void* pp = (void*)((char*)base + i*2048);
				CHECK_EQUAL(pp, p1);
			}
			// Last allocation caused the allocator to deplete the memory so it
			// did not have to allocate a 'split' node.
			CHECK_EQUAL(32-1+5, gNodeAllocator->size());

			// Allocator has no memory left so this should fail
			void* p2 = sb.allocate(60, 4);
			CHECK_NULL(p2);

			sb.release();
			CHECK_EQUAL(0, gNodeAllocator->size());
		}

		UNITTEST_TEST(allocate3)
		{
			xexternal32::xlargebin sb;
			void* base = (void*)(uptr)0x80000000;
			CHECK_EQUAL(0, gNodeAllocator->size());
			sb.init(base, 1 * 1024 * 1024 * 1024, 256, 256, gNodeAllocator);
			CHECK_EQUAL(5, gNodeAllocator->size());

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
					void* p1 = sb.allocate(60, 4);
					CHECK_NOT_NULL(p1);
					int alloc_idx = rand() % max_tracked_allocs;
					if (allocations[alloc_idx] != NULL)
					{
						sb.deallocate(allocations[alloc_idx]);
					}
					allocations[alloc_idx] = p1;
				}
			}

			for (s32 i = 0; i < max_tracked_allocs; ++i)
			{
				if (allocations[i] != NULL)
				{
					sb.deallocate(allocations[i]);
				}
			}


			sb.release();
			CHECK_EQUAL(0, gNodeAllocator->size());
		}
	}
}
UNITTEST_SUITE_END
