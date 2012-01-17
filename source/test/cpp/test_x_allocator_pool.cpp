#include "xbase\x_types.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xallocator\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;


UNITTEST_SUITE_BEGIN(x_allocator_pool)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_FIXTURE_SETUP()
		{

		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
		}

		UNITTEST_TEST(static_block_allocations_deallocations)
		{
			xpool_params params;	
			params.set_elem_size(12);
			params.set_elem_alignment(16);
			params.set_block_size(16);
			params.set_block_initial_count(4);
			params.set_block_growth_count(1);
			params.set_block_max_count(4);

			x_iallocator* pool_allocator = gCreatePoolAllocator(gSystemAllocator, params);

			const s32 N = 4*16;
			void* allocs[N];

			for (s32 j=0; j<300; j++)
			{
				for (s32 i=0; i<N; ++i)
				{
					void* mem = pool_allocator->allocate(12, params.get_elem_alignment());
					CHECK_TRUE(x_intu::isAligned((u32)mem, params.get_elem_alignment()));
					if (j==0)
					{
						CHECK_NOT_NULL(mem);
						allocs[i] = mem;
					}
					else
					{
						CHECK_NOT_NULL(mem);
						CHECK_EQUAL(allocs[i], mem);
					}
				}

				// Deallocate in reversed order so that the state of the allocator will
				// be the same as when we started to allocate and when not using
				// dynamic added blocks
				for (s32 i=N-1; i>=0; --i)
				{
					void* mem = allocs[i];
					pool_allocator->deallocate(mem);
				}
			}
			pool_allocator->release();
		}

        UNITTEST_TEST(dynamic_block_allocations_deallocations)
        {
			xpool_params params;	
			params.set_elem_size(12);
			params.set_elem_alignment(16);
			params.set_block_size(16);
			params.set_block_initial_count(4);
			params.set_block_growth_count(1);
			params.set_block_max_count(2048 / 16);

			x_iallocator* pool_allocator = gCreatePoolAllocator(gSystemAllocator, params);

			const s32 N = 2048;
			void* allocs[N];

			for (s32 j=0; j<30; j++)
			{
				for (s32 i=0; i<N; ++i)
				{
					void* mem = pool_allocator->allocate(12, params.get_elem_alignment());
					CHECK_TRUE(x_intu::isAligned((u32)mem, params.get_elem_alignment()));
					if (j==0 || j==1)
					{
						CHECK_NOT_NULL(mem);
						allocs[i] = mem;
					}
					else
					{
						// After 1 full round all dynamic blocks are there, so the behavior
						// now will be that blocks are popped in order. This cannot be
						// guaranteed when the allocator is still growing.
						CHECK_NOT_NULL(mem);
						CHECK_EQUAL(allocs[i], mem);
					}
				}
	
				// Deallocate in reversed order so that the state of the allocator will
				// be the same as when we started to allocate.
				for (s32 i=N-1; i>=0; --i)
				{
					void* mem = allocs[i];
					pool_allocator->deallocate(mem);
				}
			}
			pool_allocator->release();
        }

        UNITTEST_TEST(alloc_realloc_free)
        {
			xpool_params params;	
			params.set_elem_size(12);
			params.set_elem_alignment(16);
			params.set_block_size(16);
			params.set_block_initial_count(4);
			params.set_block_growth_count(1);
			params.set_block_max_count(4);

			x_iallocator* pool_allocator = gCreatePoolAllocator(gSystemAllocator, params);

			void* mem = pool_allocator->allocate(12, params.get_elem_alignment());
			mem = pool_allocator->reallocate(mem, 10, params.get_elem_alignment());
			mem = pool_allocator->reallocate(mem, 8, params.get_elem_alignment());
			mem = pool_allocator->reallocate(mem, 4, params.get_elem_alignment());
			pool_allocator->deallocate(mem);

			pool_allocator->release();
}

	}
}
UNITTEST_SUITE_END
