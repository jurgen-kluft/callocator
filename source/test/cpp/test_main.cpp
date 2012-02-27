#include "xbase\x_target.h"
#include "xbase\x_types.h"
#include "xbase\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

UNITTEST_SUITE_LIST(xAllocatorUnitTest);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_dlmalloc);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_tlfs);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_pool);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_forward);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_idx_allocator_array);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_idx_allocator_pool);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_small_ext);

xcore::x_iallocator* gSystemAllocator;

class UnitTestAllocator : public UnitTest::Allocator
{
public:
	xcore::x_iallocator*	mAllocator;
	int						mNumAllocations;

	UnitTestAllocator(xcore::x_iallocator* allocator)
		: mNumAllocations(0)
	{
		mAllocator = allocator;
	}

	virtual void*	Allocate(int size)
	{
		++mNumAllocations;
		return mAllocator->allocate(size, 4);
	}
	virtual void	Deallocate(void* ptr)
	{
		--mNumAllocations;
		mAllocator->deallocate(ptr);
	}
};

bool gRunUnitTest(UnitTest::TestReporter& reporter)
{
	xcore::x_iallocator* systemAllocator = xcore::gCreateSystemAllocator();
		
	UnitTestAllocator unittestAllocator( systemAllocator );
	UnitTest::SetAllocator(&unittestAllocator);

	gSystemAllocator = systemAllocator;

	int r = UNITTEST_SUITE_RUN(reporter, xAllocatorUnitTest);
	if (unittestAllocator.mNumAllocations!=0)
	{
		reporter.reportFailure(__FILE__, __LINE__, __FUNCTION__, "memory leaks detected!");
		r = -1;
	}

	gSystemAllocator = NULL;
	systemAllocator->release();
	return r==0;
}


