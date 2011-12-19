#include "xbase\x_types.h"
#include "xbase\x_allocator.h"
#include "xallocator\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;

UNITTEST_SUITE_BEGIN(x_allocator_eb)
{
    UNITTEST_FIXTURE(main)
    {
		void*			gBlock;
		s32				gBlockSize;
		x_iallocator*	gCustomAllocator;

		class my_extmem : public x_imemory
		{
			void*			mBlock;
			s32				mBlockSize;
		public:
			my_extmem()
				: mBlock(NULL)
				, mBlockSize(0)
			{
			}

			my_extmem(void* mem_begin, u32 mem_size)
				: mBlock(mem_begin)
				, mBlockSize(mem_size)
			{
			}
			
			virtual const char*		name() const
			{
				return "my external memory";
			}
			
			virtual u32				mem_range(void*& low, void*& high) const
			{
				low = mBlock;
				high = (xbyte*)mBlock + mBlockSize;
				return mBlockSize;
			}
			
			virtual void			mem_copy(void* dst, void const* src, u32 size) const
			{
			}

			virtual u32				page_size() const
			{
				return 64 * 1024;
			}

			virtual u32				align_size() const
			{
				return 64;
			}
		};

		my_extmem		mExtMemory;

        UNITTEST_FIXTURE_SETUP()
		{
			gBlockSize = 128 * 1024;
			gBlock = gSystemAllocator->allocate(gBlockSize, 8);
			mExtMemory = my_extmem(gBlock, gBlockSize);
			
			gCustomAllocator = gCreateExtBlockAllocator(gSystemAllocator, &mExtMemory);
		}

        UNITTEST_FIXTURE_TEARDOWN()
		{
			gCustomAllocator->release();
			gSystemAllocator->deallocate(gBlock);
			gBlock = NULL;
			gBlockSize = 0;
		}

        UNITTEST_TEST(alloc3_free3)
        {
			void* mem1 = gCustomAllocator->allocate(512, 8);
			void* mem2 = gCustomAllocator->allocate(1024, 16);
			void* mem3 = gCustomAllocator->allocate(512, 32);
			void* mem4 = gCustomAllocator->allocate(1024, 1024);
			void* mem5 = gCustomAllocator->allocate(256, 32);

			gCustomAllocator->deallocate(mem4);

			void* mem6 = gCustomAllocator->allocate(8, 8);
			void* mem7 = gCustomAllocator->allocate(100000, 1024);
			void* mem8 = gCustomAllocator->allocate(20000, 2048);

			gCustomAllocator->deallocate(mem1);

			void* mem9 = gCustomAllocator->allocate(16, 8);

			gCustomAllocator->deallocate(mem7);
			
			gCustomAllocator->deallocate(mem2);

			gCustomAllocator->deallocate(mem3);
			gCustomAllocator->deallocate(mem5);
			gCustomAllocator->deallocate(mem8);
			gCustomAllocator->deallocate(mem9);

        }

	}
}
UNITTEST_SUITE_END
