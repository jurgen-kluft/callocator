#include "xbase/x_base.h"
#include "xbase/x_allocator.h"
#include "xbase/x_console.h"

#include "xunittest/xunittest.h"
#include "xunittest/private/ut_ReportAssert.h"

UNITTEST_SUITE_LIST(xAllocatorUnitTest);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_freelist);
//UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_dlmalloc);	// Doesn't work on 64-bit systems
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_tlfs);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_freelist);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_forward);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_fsadexed_array);

namespace xcore
{
	// Our own assert handler
	class UnitTestAssertHandler : public xcore::xasserthandler
	{
	public:
		UnitTestAssertHandler()
		{
			NumberOfAsserts = 0;
		}

		virtual xcore::xbool	handle_assert(u32& flags, const char* fileName, s32 lineNumber, const char* exprString, const char* messageString)
		{
			UnitTest::reportAssert(exprString, fileName, lineNumber);
			NumberOfAsserts++;
			return false;
		}


		xcore::s32		NumberOfAsserts;
	};

	class UnitTestAllocator : public UnitTest::Allocator
	{
		xcore::xalloc*	mAllocator;
	public:
						UnitTestAllocator(xcore::xalloc* allocator)	{ mAllocator = allocator; }
		virtual void*	Allocate(xsize_t size)								{ return mAllocator->allocate((u32)size, sizeof(void*)); }
		virtual xsize_t Deallocate(void* ptr)								{ return mAllocator->deallocate(ptr); }
	};

	class TestAllocator : public xalloc
	{
		xalloc*		mAllocator;
	public:
							TestAllocator(xalloc* allocator) : mAllocator(allocator) { }

		virtual const char*	name() const										{ return "xbase unittest test heap allocator"; }

		virtual void*		v_allocate(u32 size, u32 alignment)
		{
			UnitTest::IncNumAllocations();
			return mAllocator->allocate(size, alignment);
		}

		virtual u32			v_deallocate(void* mem)
		{
			UnitTest::DecNumAllocations();
			return mAllocator->deallocate(mem);
		}

		virtual void		v_release()
		{
			mAllocator->release();
			mAllocator = NULL;
		}
	};
}

xcore::xalloc* gSystemAllocator = NULL;
xcore::UnitTestAssertHandler gAssertHandler;

bool gRunUnitTest(UnitTest::TestReporter& reporter)
{
	xbase::x_Init();

#ifdef TARGET_DEBUG
	xcore::xasserthandler::sRegisterHandler(&gAssertHandler);
#endif

	xcore::xalloc* systemAllocator = xcore::xalloc::get_system();
	xcore::UnitTestAllocator unittestAllocator( systemAllocator );
	UnitTest::SetAllocator(&unittestAllocator);

	xcore::console->write("Configuration: ");
	xcore::console->writeLine(TARGET_FULL_DESCR_STR);

	xcore::TestAllocator testAllocator(systemAllocator);
    gSystemAllocator = &testAllocator;

	int r = UNITTEST_SUITE_RUN(reporter, xAllocatorUnitTest);
	if (UnitTest::GetNumAllocations()!=0)
	{
		reporter.reportFailure(__FILE__, __LINE__, "xunittest", "memory leaks detected!");
		r = -1;
	}

	gSystemAllocator->release();

	UnitTest::SetAllocator(NULL);

	xbase::x_Exit();
	return r==0;
}

