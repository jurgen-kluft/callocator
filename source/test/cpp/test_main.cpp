#include "cbase/c_base.h"
#include "cbase/c_allocator.h"
#include "cbase/c_console.h"
#include "cbase/c_context.h"

#include "cunittest/cunittest.h"

UNITTEST_SUITE_LIST

namespace ncore
{
    // Our own assert handler
    class UnitTestAssertHandler : public ncore::asserthandler_t
    {
    public:
        UnitTestAssertHandler() { NumberOfAsserts = 0; }

        virtual bool handle_assert(u32& flags, const char* fileName, s32 lineNumber, const char* exprString, const char* messageString)
        {
            UnitTest::ReportAssert(exprString, fileName, lineNumber);
            NumberOfAsserts++;
            return false;
        }

        ncore::s32 NumberOfAsserts;
    };

    class UnitTestAllocator : public UnitTest::TestAllocator
    {
    public:
        ncore::alloc_t* mAllocator;
        int             mNumAllocations;

        UnitTestAllocator(ncore::alloc_t* allocator)
            : mAllocator(allocator)
            , mNumAllocations(0)
        {
        }

        virtual void* Allocate(unsigned int size, unsigned int alignment)
        {
            mNumAllocations++;
            return mAllocator->allocate(size, alignment);
        }
        virtual unsigned int Deallocate(void* ptr)
        {
            --mNumAllocations;
            return mAllocator->deallocate(ptr);
        }
    };

    class TestAllocator : public alloc_t
    {
        UnitTest::TestAllocator* mAllocator;

    public:
        TestAllocator(UnitTestAllocator* allocator)
            : mAllocator(allocator)
        {
        }

        virtual void* v_allocate(u32 size, u32 alignment) { return mAllocator->Allocate(size, alignment); }

        virtual u32 v_deallocate(void* mem) { return mAllocator->Deallocate(mem); }

        virtual void v_release()
        {
            // Do nothing
        }
    };
} // namespace ncore

bool gRunUnitTest(UnitTest::TestReporter& reporter, UnitTest::TestContext& context)
{
    cbase::init();

#ifdef TARGET_DEBUG
    ncore::UnitTestAssertHandler assertHandler;
    ncore::context_t::set_assert_handler(&assertHandler);
#endif
    ncore::console->write("Configuration: ");
    ncore::console->setColor(ncore::console_t::YELLOW);
    ncore::console->writeLine(TARGET_FULL_DESCR_STR);
    ncore::console->setColor(ncore::console_t::NORMAL);

    ncore::alloc_t*          systemAllocator = ncore::context_t::system_alloc();
    ncore::UnitTestAllocator unittestAllocator(systemAllocator);
    context.mAllocator = &unittestAllocator;

    ncore::TestAllocator testAllocator(&unittestAllocator);
    ncore::context_t::set_system_alloc(&testAllocator);

    int r = UNITTEST_SUITE_RUN(context, reporter, cUnitTest);
    if (unittestAllocator.mNumAllocations != 0)
    {
        reporter.reportFailure(__FILE__, __LINE__, "cunittest", "memory leaks detected!");
        r = -1;
    }

    ncore::context_t::set_system_alloc(systemAllocator);

    cbase::exit();
    return r == 0;
}
