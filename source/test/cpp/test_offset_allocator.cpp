#include "ccore/c_allocator.h"
#include "callocator/c_offset_allocator.h"
#include "cunittest/cunittest.h"

using namespace ncore;

namespace ncore
{
    namespace noffset
    {
        namespace nfloat
        {
            extern u32 uintToFloatRoundUp(u32 size);
            extern u32 uintToFloatRoundDown(u32 size);
            extern u32 floatToUint(u32 floatValue);
        }  // namespace nfloat
    }  // namespace noffset
}  // namespace ncore

UNITTEST_SUITE_BEGIN(offset)
{
    UNITTEST_FIXTURE(small_float)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(uintToFloat)
        {
            // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
            // NOTE: Assuming 8 value (3 bit) mantissa.
            // If this test fails, please change this assumption!
            u32 preciseNumberCount = 17;
            for (u32 i = 0; i < preciseNumberCount; i++)
            {
                u32 roundUp   = ncore::noffset::nfloat::uintToFloatRoundUp(i);
                u32 roundDown = ncore::noffset::nfloat::uintToFloatRoundDown(i);
                CHECK_EQUAL(i, roundUp);
                CHECK_EQUAL(i, roundDown);
            }

            // Test some random picked numbers
            struct NumberFloatUpDown
            {
                u32 number;
                u32 up;
                u32 down;
            };

            // NumberFloatUpDown testData[] = {
            //   {.number = 17, .up = 17, .down = 16},      {.number = 118, .up = 39, .down = 38},      {.number = 1024, .up = 64, .down = 64},
            //   {.number = 65536, .up = 112, .down = 112}, {.number = 529445, .up = 137, .down = 136}, {.number = 1048575, .up = 144, .down = 143},
            // };
            NumberFloatUpDown testData[] = {
                {17, 17, 16}, {118, 39, 38}, {1024, 64, 64}, {65536, 112, 112}, {529445, 137, 136}, {1048575, 144, 143},
            };

            for (u32 i = 0; i < sizeof(testData) / sizeof(NumberFloatUpDown); i++)
            {
                NumberFloatUpDown v         = testData[i];
                u32               roundUp   = ncore::noffset::nfloat::uintToFloatRoundUp(v.number);
                u32               roundDown = ncore::noffset::nfloat::uintToFloatRoundDown(v.number);
                CHECK_EQUAL(roundUp, v.up);
                CHECK_EQUAL(roundDown, v.down);
            }
        }

        UNITTEST_TEST(floatToUint)
        {
            // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
            // NOTE: Assuming 8 value (3 bit) mantissa.
            // If this test fails, please change this assumption!
            u32 preciseNumberCount = 17;
            for (u32 i = 0; i < preciseNumberCount; i++)
            {
                u32 v = ncore::noffset::nfloat::floatToUint(i);
                CHECK_EQUAL(i, v);
            }

            // Test that float->u32->float conversion is precise for all numbers
            // NOTE: Test values < 240. 240->4G = overflows 32 bit integer
            for (u32 i = 0; i < 240; i++)
            {
                u32 v         = ncore::noffset::nfloat::floatToUint(i);
                u32 roundUp   = ncore::noffset::nfloat::uintToFloatRoundUp(v);
                u32 roundDown = ncore::noffset::nfloat::uintToFloatRoundDown(v);
                CHECK_EQUAL(i, roundUp);
                CHECK_EQUAL(i, roundDown);
                // if ((i%8) == 0) printf("\n");
                // printf("%u->%u ", i, v);
            }
        }
    }

    UNITTEST_FIXTURE(allocator)
    {
        UNITTEST_ALLOCATOR;

        ncore::noffset::allocator_t* allocator = nullptr;

        UNITTEST_FIXTURE_SETUP()
        {
            allocator = Allocator->construct<ncore::noffset::allocator_t>(Allocator, 1024 * 1024 * 256);
            allocator->setup();
        }

        UNITTEST_FIXTURE_TEARDOWN()
        {
            allocator->teardown();
            Allocator->destruct(allocator);
        }

        UNITTEST_TEST(basic)
        {
            ncore::noffset::allocator_t alloc(Allocator, 1024 * 1024 * 256);
            alloc.setup();

            ncore::noffset::allocation_t a      = alloc.allocate(1337);
            u32                       offset = a.offset;
            CHECK_EQUAL(0, offset);
            alloc.free(a);

            alloc.teardown();
        }

        UNITTEST_TEST(allocate_simple)
        {
            // Free merges neighbor empty nodes. Next allocation should also have offset = 0
            ncore::noffset::allocation_t a = allocator->allocate(0);
            CHECK_EQUAL(0, a.offset);

            ncore::noffset::allocation_t b = allocator->allocate(1);
            CHECK_EQUAL(0, b.offset);

            ncore::noffset::allocation_t c = allocator->allocate(123);
            CHECK_EQUAL(1, c.offset);

            ncore::noffset::allocation_t d = allocator->allocate(1234);
            CHECK_EQUAL(124, d.offset);

            allocator->free(a);
            allocator->free(b);
            allocator->free(c);
            allocator->free(d);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL(0, validateAll.offset);
            allocator->free(validateAll);
        }

        UNITTEST_TEST(merge_trivial)
        {
            // Free merges neighbor empty nodes. Next allocation should also have offset = 0
            ncore::noffset::allocation_t a = allocator->allocate(1337);
            CHECK_EQUAL(0, a.offset);
            allocator->free(a);

            ncore::noffset::allocation_t b = allocator->allocate(1337);
            CHECK_EQUAL(0, b.offset);
            allocator->free(b);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL(0, validateAll.offset);
            allocator->free(validateAll);
        }

        UNITTEST_TEST(reuse_trivial)
        {
            // Allocator should reuse node freed by A since the allocation C fits in the same bin (using pow2 size to be sure)
            ncore::noffset::allocation_t a = allocator->allocate(1024);
            CHECK_EQUAL(0, a.offset);

            ncore::noffset::allocation_t b = allocator->allocate(3456);
            CHECK_EQUAL(1024, b.offset);

            allocator->free(a);

            ncore::noffset::allocation_t c = allocator->allocate(1024);
            CHECK_EQUAL(0, c.offset);

            allocator->free(c);
            allocator->free(b);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL(0, validateAll.offset);
            allocator->free(validateAll);
        }

        UNITTEST_TEST(reuse_complex)
        {
            // Allocator should not reuse node freed by A since the allocation C doesn't fits in the same bin
            // However node D and E fit there and should reuse node from A
            ncore::noffset::allocation_t a = allocator->allocate(1024);
            CHECK_EQUAL(0, a.offset);

            ncore::noffset::allocation_t b = allocator->allocate(3456);
            CHECK_EQUAL(1024, b.offset);

            allocator->free(a);

            ncore::noffset::allocation_t c = allocator->allocate(2345);
            CHECK_EQUAL(1024 + 3456, c.offset);

            ncore::noffset::allocation_t d = allocator->allocate(456);
            CHECK_EQUAL(0, d.offset);

            ncore::noffset::allocation_t e = allocator->allocate(512);
            CHECK_EQUAL(456, e.offset);

            ncore::noffset::storage_report_t report = allocator->storageReport();
            CHECK_EQUAL(1024 * 1024 * 256 - 3456 - 2345 - 456 - 512, report.totalFreeSpace);
            CHECK_NOT_EQUAL(report.totalFreeSpace, report.largestFreeRegion);

            allocator->free(c);
            allocator->free(d);
            allocator->free(b);
            allocator->free(e);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL(0, validateAll.offset);
            allocator->free(validateAll);
        }

        UNITTEST_TEST(zero_fragmentation)
        {
            // Allocate 256x 1MB. Should fit. Then free four random slots and reallocate four slots.
            // Plus free four contiguous slots an allocate 4x larger slot. All must be zero fragmentation!
            ncore::noffset::allocation_t allocations[256];
            for (u32 i = 0; i < 256; i++)
            {
                allocations[i] = allocator->allocate(1024 * 1024);
                CHECK_EQUAL(i * 1024 * 1024, allocations[i].offset);
            }

            ncore::noffset::storage_report_t report = allocator->storageReport();
            CHECK_EQUAL(0, report.totalFreeSpace);
            CHECK_EQUAL(0, report.largestFreeRegion);

            // Free four random slots
            allocator->free(allocations[243]);
            allocator->free(allocations[5]);
            allocator->free(allocations[123]);
            allocator->free(allocations[95]);

            // Free four contiguous slot (allocator must merge)
            allocator->free(allocations[151]);
            allocator->free(allocations[152]);
            allocator->free(allocations[153]);
            allocator->free(allocations[154]);

            allocations[243] = allocator->allocate(1024 * 1024);
            allocations[5]   = allocator->allocate(1024 * 1024);
            allocations[123] = allocator->allocate(1024 * 1024);
            allocations[95]  = allocator->allocate(1024 * 1024);
            allocations[151] = allocator->allocate(1024 * 1024 * 4);  // 4x larger

            u32 const no_space = ncore::noffset::allocation_t::NO_SPACE;
            CHECK_NOT_EQUAL(no_space, allocations[243].offset);
            CHECK_NOT_EQUAL(no_space, allocations[5].offset);
            CHECK_NOT_EQUAL(no_space, allocations[123].offset);
            CHECK_NOT_EQUAL(no_space, allocations[95].offset);
            CHECK_NOT_EQUAL(no_space, allocations[151].offset);

            for (u32 i = 0; i < 256; i++)
            {
                if (i < 152 || i > 154)
                    allocator->free(allocations[i]);
            }

            ncore::noffset::storage_report_t report2 = allocator->storageReport();
            CHECK_EQUAL(1024 * 1024 * 256, report2.totalFreeSpace);
            CHECK_EQUAL(1024 * 1024 * 256, report2.largestFreeRegion);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL(0, validateAll.offset);
            allocator->free(validateAll);
        }
    }
}
UNITTEST_SUITE_END
