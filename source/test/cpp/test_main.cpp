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

        virtual bool handle_assert(const char* fileName, s32 lineNumber, const char* exprString, const char* messageString)
        {
            UnitTest::ReportAssert(exprString, fileName, lineNumber);
            NumberOfAsserts++;
            return false;
        }

        ncore::s32 NumberOfAsserts;
    };

    class TestAllocator : public alloc_t
    {
        UnitTest::TestAllocator* mAllocator;

    public:
        TestAllocator(UnitTest::TestAllocator* allocator)
            : mAllocator(allocator)
        {
        }

        virtual void* v_allocate(u32 size, u32 alignment) { return mAllocator->Allocate(size, alignment); }
        virtual void  v_deallocate(void* mem) { mAllocator->Deallocate(mem); }
    };
} // namespace ncore

bool gRunUnitTest(UnitTest::TestReporter& reporter, UnitTest::TestContext& context)
{
    cbase::init();

#ifdef TARGET_DEBUG
    ncore::UnitTestAssertHandler assertHandler;
    ncore::context_t::set_assert_handler(&assertHandler);
    ncore::gSetAssertHandler(&assertHandler);
#endif
    ncore::console->write("Configuration: ");
    ncore::console->setColor(ncore::console_t::YELLOW);
    ncore::console->writeLine(TARGET_FULL_DESCR_STR);
    ncore::console->setColor(ncore::console_t::NORMAL);

    ncore::TestAllocator testAllocator(context.mAllocator);
    ncore::alloc_t*      systemAllocator = ncore::context_t::system_alloc();
    ncore::context_t::set_system_alloc(&testAllocator);

    int r = UNITTEST_SUITE_RUN(context, reporter, cUnitTest);

    ncore::context_t::set_system_alloc(systemAllocator);

    cbase::exit();
    return r == 0;
}
