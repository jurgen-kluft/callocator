#include "xbase\x_target.h"
#include "xbase\x_types.h"
#include "xbase\x_allocator.h"

#include "xunittest\xunittest.h"

using namespace xcore;

UNITTEST_SUITE_LIST(xAllocatorUnitTest);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_dlmalloc);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_tlfs);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_fst);

class unittest_allocator : public x_iallocator
{
public:
	unittest_allocator(x_iallocator* allocator)
		: mAllocator(allocator)
		, mNumAllocations(0)
	{
	}

	x_iallocator*		mAllocator;
	int					mNumAllocations;

	virtual const char*		name() const
	{
		return "unittest allocator";
	}

	virtual void*			allocate(s32 size, s32 alignment)
	{
		++mNumAllocations;
		return mAllocator->allocate(size, alignment);
	}

	virtual void*			callocate(s32 nelem, s32 elemsize)
	{
		++mNumAllocations;
		return mAllocator->callocate(nelem, elemsize);
	}

	virtual void*			reallocate(void* ptr, s32 size, s32 alignment)
	{
		return mAllocator->reallocate(ptr, size, alignment);
	}

	virtual void			deallocate(void* ptr)
	{
		--mNumAllocations;
		return mAllocator->deallocate(ptr);
	}

	virtual void			release()
	{
		
	}
};

static x_iallocator* gSystemAllocator;
x_iallocator* gUnitTestAllocator;

static UnitTest::TestReporter* gUnitTestReporter;
static xbool sHasMemoryLeaks = xFALSE;

void*	UnitTest::Allocate(int size)
{
	return gSystemAllocator->allocate(size + 10000, 4);
}

void	UnitTest::Deallocate(void* ptr)
{
	gSystemAllocator->deallocate(ptr);
}

static const char* sTestFilename;
static const char* sTestSuiteName;
static const char* sTestFixtureName;
void	UnitTest::BeginFixture(const char* filename, const char* suite_name, const char* fixture_name)
{
	sTestFilename    = filename;
	sTestSuiteName   = suite_name;
	sTestFixtureName = fixture_name;
}
void	UnitTest::EndFixture()
{
	if (((unittest_allocator*)gUnitTestAllocator)->mNumAllocations!=0)
	{
		sHasMemoryLeaks = xTRUE;
		gUnitTestReporter->reportFailure(sTestFilename, 0, sTestFixtureName, "memory leaks detected!");
	}
}

int main(int argc, char** argv)
{
	gSystemAllocator = gCreateSystemAllocator();
	unittest_allocator allocator(gSystemAllocator);
	gUnitTestAllocator = &allocator;

	UnitTest::TestReporterStdout stdout_reporter;
	UnitTest::TestReporter& reporter = stdout_reporter;
	gUnitTestReporter = &reporter;
	int r = UNITTEST_SUITE_RUN(reporter, xAllocatorUnitTest);
	if (sHasMemoryLeaks)
		r = -1;

	gUnitTestReporter = NULL;

	gSystemAllocator->release();
	return r;
}