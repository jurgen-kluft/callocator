#include "xbase\x_target.h"
#include "xbase\x_types.h"

#include "xunittest\xunittest.h"

using namespace xcore;

UNITTEST_SUITE_LIST(xAllocatorUnitTest);
UNITTEST_SUITE_DECLARE(xAllocatorUnitTest, x_allocator_system);

int main(int argc, char** argv)
{
	UnitTest::TestReporterStdout reporter;
	int r = UNITTEST_SUITE_RUN(reporter, xAllocatorUnitTest);

	return r;
}