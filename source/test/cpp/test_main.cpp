#include "xbase\x_base.h"
#include "xbase\x_allocator.h"
#include "xbase\x_console.h"

#include "xunittest\xunittest.h"
#include "xunittest\private\ut_ReportAssert.h"

using namespace xcore;

UNITTEST_SUITE_LIST(xAllocatorUnitTest);
//UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_dlmalloc);	// Doesn't work on 64-bit systems
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_tlfs);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_freelist);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_hext);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_pool);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_forward);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_idx_allocator_array);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_idx_allocator_pool);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_small_ext);
//UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_large_ext);
//UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_large16_ext);
//UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_large32_ext);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_memento);


namespace xcore
{
	// Our own assert handler
	class UnitTestAssertHandler : public xcore::x_asserthandler
	{
	public:
		UnitTestAssertHandler()
		{
			NumberOfAsserts = 0;
		}

		virtual xcore::xbool	HandleAssert(u32& flags, const char* fileName, s32 lineNumber, const char* exprString, const char* messageString)
		{
			UnitTest::reportAssert(exprString, fileName, lineNumber);
			NumberOfAsserts++;
			return false;
		}

		xcore::s32		NumberOfAsserts;
	};

	class UnitTestAllocator : public UnitTest::Allocator
	{
		xcore::x_iallocator*	mAllocator;
	public:
		inline			UnitTestAllocator(xcore::x_iallocator* allocator)	{ mAllocator = allocator; mNumAllocations = 0; }
		virtual void*	Allocate(xsize_t size)								{ ++mNumAllocations; return mAllocator->allocate((u32)size, 4); }
		virtual void	Deallocate(void* ptr)								{ --mNumAllocations; mAllocator->deallocate(ptr); }

		u64				mNumAllocations;
	};

	class TestAllocator : public x_iallocator
	{
		x_iallocator*		mAllocator;
	public:
		TestAllocator(x_iallocator* allocator) : mAllocator(allocator) { }

		virtual const char*	name() const										{ return "xbase unittest test heap allocator"; }

		virtual void*		allocate(xsize_t size, u32 alignment)
		{
			UnitTest::IncNumAllocations();
			return mAllocator->allocate(size, alignment);
		}

		virtual void*		reallocate(void* mem, xsize_t size, u32 alignment)
		{
			if (mem == NULL)
				return allocate(size, alignment);
			else
				return mAllocator->reallocate(mem, size, alignment);
		}

		virtual void		deallocate(void* mem)
		{
			UnitTest::DecNumAllocations();
			mAllocator->deallocate(mem);
		}

		virtual void		release()
		{
			mAllocator->release();
			mAllocator = NULL;
		}
	};
}

xcore::x_iallocator* gSystemAllocator = NULL;
xcore::UnitTestAssertHandler gAssertHandler;

bool gRunUnitTest(UnitTest::TestReporter& reporter)
{
	xcore::x_iallocator* systemAllocator = gCreateSystemAllocator();
	xcore::UnitTestAllocator unittestAllocator(systemAllocator);
	UnitTest::SetAllocator(&unittestAllocator);

	xbase::x_Init();

#ifdef TARGET_DEBUG
	xcore::x_asserthandler::sRegisterHandler(&gAssertHandler);
#endif

	xcore::xconsole::write("Configuration: ");
	xcore::xconsole::writeLine(TARGET_FULL_DESCR_STR);

	xcore::TestAllocator testAllocator(systemAllocator);
	gSystemAllocator = &testAllocator;

	int r = UNITTEST_SUITE_RUN(reporter, xAllocatorUnitTest);
	if (unittestAllocator.mNumAllocations!=0)
	{
		reporter.reportFailure(__FILE__, __LINE__, __FUNCTION__, "memory leaks detected!");
		r = -1;
	}

	gSystemAllocator = NULL;
	systemAllocator->release();
	UnitTest::SetAllocator(NULL);

	xbase::x_Exit();
	return r == 0;
}


