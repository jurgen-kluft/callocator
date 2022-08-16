#include "xbase/x_base.h"
#include "xbase/x_allocator.h"
#include "xbase/x_console.h"
#include "xbase/x_context.h"

#include "xunittest/xunittest.h"
#include "xunittest/private/ut_ReportAssert.h"

UNITTEST_SUITE_LIST(xAllocatorUnitTest);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_freelist);
//UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_dlmalloc);	// Doesn't work on 64-bit systems
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_tlfs);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_freelist);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_forward);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_fsadexed_array);

namespace ncore
{
	// Our own assert handler
	class UnitTestAssertHandler : public ncore::asserthandler_t
	{
	public:
		UnitTestAssertHandler()
		{
			NumberOfAsserts = 0;
		}

		virtual bool	handle_assert(u32& flags, const char* fileName, s32 lineNumber, const char* exprString, const char* messageString)
		{
			UnitTest::reportAssert(exprString, fileName, lineNumber);
			NumberOfAsserts++;
			return false;
		}


		ncore::s32		NumberOfAsserts;
	};

	class UnitTestAllocator : public UnitTest::Allocator
	{
		ncore::alloc_t*	mAllocator;
	public:
						UnitTestAllocator(ncore::alloc_t* allocator)	{ mAllocator = allocator; }
		virtual void*	Allocate(uint_t size)								{ return mAllocator->allocate((u32)size, sizeof(void*)); }
		virtual uint_t Deallocate(void* ptr)								{ return mAllocator->deallocate(ptr); }
	};

	class TestAllocator : public alloc_t
	{
		alloc_t*		mAllocator;
	public:
							TestAllocator(alloc_t* allocator) : mAllocator(allocator) { }

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
			mAllocator = nullptr;
		}
	};
}

ncore::alloc_t *gTestAllocator = nullptr;
ncore::UnitTestAssertHandler gAssertHandler;

bool gRunUnitTest(UnitTest::TestReporter& reporter)
{
	xbase::init();

#ifdef TARGET_DEBUG
	ncore::context_t::set_assert_handler(&gAssertHandler);
#endif
	ncore::console->write("Configuration: ");
	ncore::console->setColor(ncore::console_t::YELLOW);
	ncore::console->writeLine(TARGET_FULL_DESCR_STR);
	ncore::console->setColor(ncore::console_t::NORMAL);

	ncore::alloc_t* systemAllocator = ncore::context_t::system_alloc();
	ncore::UnitTestAllocator unittestAllocator( systemAllocator );
	UnitTest::SetAllocator(&unittestAllocator);

	ncore::TestAllocator testAllocator(systemAllocator);
	gTestAllocator = &testAllocator;
	ncore::context_t::set_system_alloc(&testAllocator);

	int r = UNITTEST_SUITE_RUN(reporter, xAllocatorUnitTest);
	if (UnitTest::GetNumAllocations()!=0)
	{
		reporter.reportFailure(__FILE__, __LINE__, "xunittest", "memory leaks detected!");
		r = -1;
	}

	gTestAllocator->release();

	UnitTest::SetAllocator(nullptr);
	ncore::context_t::set_system_alloc(systemAllocator);

	xbase::exit();
	return r==0;
}

