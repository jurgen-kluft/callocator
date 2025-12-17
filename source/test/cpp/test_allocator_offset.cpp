#include "ccore/c_allocator.h"
#include "callocator/c_allocator_offset.h"
#include "cunittest/cunittest.h"

using namespace ncore;

namespace ncore
{
    namespace noffset
    {
        namespace nfloat
        {
            extern u32 SizeToBinCeil(u32 size);
            extern u32 SizeToBinFloor(u32 size);
            extern u32 U32ToF32RoundUp(u32 size);
            extern u32 U32ToF32RoundDown(u32 size);
            extern u32 F32ToU32(u32 floatValue);
        } // namespace nfloat
    } // namespace noffset
} // namespace ncore

UNITTEST_SUITE_BEGIN(offset)
{
    UNITTEST_FIXTURE(small_float)
    {
        //UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        const u32 sizes[] = {0,         1,         2,         3,          4,          5,          6,          7,          8,          9,          10,         11,        12,        13,        14,        15,        16,        18,        20,        22,
                             24,        26,        28,        30,         32,         36,         40,         44,         48,         52,         56,         60,        64,        72,        80,        88,        96,        104,       112,       120,
                             128,       144,       160,       176,        192,        208,        224,        240,        256,        288,        320,        352,       384,       416,       448,       480,       512,       576,       640,       704,
                             768,       832,       896,       960,        1024,       1152,       1280,       1408,       1536,       1664,       1792,       1920,      2048,      2304,      2560,      2816,      3072,      3328,      3584,      3840,
                             4096,      4608,      5120,      5632,       6144,       6656,       7168,       7680,       8192,       9216,       10240,      11264,     12288,     13312,     14336,     15360,     16384,     18432,     20480,     22528,
                             24576,     26624,     28672,     30720,      32768,      36864,      40960,      45056,      49152,      53248,      57344,      61440,     65536,     73728,     81920,     90112,     98304,     106496,    114688,    122880,
                             131072,    147456,    163840,    180224,     196608,     212992,     229376,     245760,     262144,     294912,     327680,     360448,    393216,    425984,    458752,    491520,    524288,    589824,    655360,    720896,
                             786432,    851968,    917504,    983040,     1048576,    1179648,    1310720,    1441792,    1572864,    1703936,    1835008,    1966080,   2097152,   2359296,   2621440,   2883584,   3145728,   3407872,   3670016,   3932160,
                             4194304,   4718592,   5242880,   5767168,    6291456,    6815744,    7340032,    7864320,    8388608,    9437184,    10485760,   11534336,  12582912,  13631488,  14680064,  15728640,  16777216,  18874368,  20971520,  23068672,
                             25165824,  27262976,  29360128,  31457280,   33554432,   37748736,   41943040,   46137344,   50331648,   54525952,   58720256,   62914560,  67108864,  75497472,  83886080,  92274688,  100663296, 109051904, 117440512, 125829120,
                             134217728, 150994944, 167772160, 184549376,  201326592,  218103808,  234881024,  251658240,  268435456,  301989888,  335544320,  369098752, 402653184, 436207616, 469762048, 503316480, 536870912, 603979776, 671088640, 738197504,
                             805306368, 872415232, 939524096, 1006632960, 1073741824, 1207959552, 1342177280, 1476395008, 1610612736, 1744830464, 1879048192, 2013265920};
        UNITTEST_TEST(sizeToBin)
        {
            for (u32 i = 0; i < sizeof(sizes) / sizeof(u32); i++)
            {
                u32 size = sizes[i];
                u32 bin  = ncore::noffset::nfloat::SizeToBinCeil(size);
                CHECK_EQUAL(i, bin);

                u32 bin2 = ncore::noffset::nfloat::U32ToF32RoundUp(size);
                CHECK_EQUAL(bin, bin2);
            }

            CHECK_EQUAL((u32)45, ncore::noffset::nfloat::SizeToBinCeil(200));
            CHECK_EQUAL((u32)90, ncore::noffset::nfloat::SizeToBinCeil(9500));
        }

        UNITTEST_TEST(uintToFloat)
        {
            // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
            // NOTE: Assuming 8 value (3 bit) mantissa.
            // If this test fails, please change this assumption!
            u32 preciseNumberCount = 17;
            for (u32 i = 0; i < preciseNumberCount; i++)
            {
                u32 roundUp   = ncore::noffset::nfloat::U32ToF32RoundUp(i);
                u32 roundDown = ncore::noffset::nfloat::U32ToF32RoundDown(i);
                CHECK_EQUAL(i, roundUp);
                CHECK_EQUAL(i, roundDown);
                u32 roundUp2 = ncore::noffset::nfloat::SizeToBinCeil(i);
                u32 roundDown2 = ncore::noffset::nfloat::SizeToBinFloor(i);
                CHECK_EQUAL(i, roundUp2);
                CHECK_EQUAL(i, roundDown2);
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
                u32               roundUp   = ncore::noffset::nfloat::U32ToF32RoundUp(v.number);
                u32               roundDown = ncore::noffset::nfloat::U32ToF32RoundDown(v.number);
                CHECK_EQUAL(roundUp, v.up);
                CHECK_EQUAL(roundDown, v.down);

                u32 roundUp2 = ncore::noffset::nfloat::SizeToBinCeil(v.number);
                u32 roundDown2 = ncore::noffset::nfloat::SizeToBinFloor(v.number);
                CHECK_EQUAL(roundUp2, v.up);
                CHECK_EQUAL(roundDown2, v.down);
            }
        }

        UNITTEST_TEST(F32ToU32)
        {
            // Denorms, exp=1 and exp=2 + mantissa = 0 are all precise.
            // NOTE: Assuming 8 value (3 bit) mantissa.
            // If this test fails, please change this assumption!
            u32 preciseNumberCount = 17;
            for (u32 i = 0; i < preciseNumberCount; i++)
            {
                u32 v = ncore::noffset::nfloat::F32ToU32(i);
                CHECK_EQUAL(i, v);
            }

            // Test that float->u32->float conversion is precise for all numbers
            // NOTE: Test values < 240. 240->4G = overflows 32 bit integer
            for (u32 i = 0; i < 240; i++)
            {
                u32 v         = ncore::noffset::nfloat::F32ToU32(i);
                u32 roundUp   = ncore::noffset::nfloat::U32ToF32RoundUp(v);
                u32 roundDown = ncore::noffset::nfloat::U32ToF32RoundDown(v);
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
            allocator = new (Allocator->allocate(sizeof(ncore::noffset::allocator_t))) ncore::noffset::allocator_t(Allocator, 1024 * 1024 * 256);
            allocator->setup();
        }

        UNITTEST_FIXTURE_TEARDOWN()
        {
            allocator->teardown();
            g_destruct(Allocator, allocator);
        }

        UNITTEST_TEST(basic)
        {
            ncore::noffset::allocator_t alloc(Allocator, 1024 * 1024 * 256);
            alloc.setup();

            ncore::noffset::allocation_t a      = alloc.allocate(1337);
            u32                          offset = a.offset;
            CHECK_EQUAL((u32)0, offset);
            alloc.free(a);

            alloc.teardown();
        }

        UNITTEST_TEST(allocate_simple)
        {
            // Free merges neighbor empty nodes. Next allocation should also have offset = 0
            ncore::noffset::allocation_t a = allocator->allocate(0);
            CHECK_EQUAL((u32)0, a.offset);

            ncore::noffset::allocation_t b = allocator->allocate(1);
            CHECK_EQUAL((u32)0, b.offset);

            ncore::noffset::allocation_t c = allocator->allocate(123);
            CHECK_EQUAL((u32)1, c.offset);

            ncore::noffset::allocation_t d = allocator->allocate(1234);
            CHECK_EQUAL((u32)124, d.offset);

            allocator->free(a);
            allocator->free(b);
            allocator->free(c);
            allocator->free(d);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL((u32)0, validateAll.offset);
            allocator->free(validateAll);
        }

        UNITTEST_TEST(merge_trivial)
        {
            // Free merges neighbor empty nodes. Next allocation should also have offset = 0
            ncore::noffset::allocation_t a = allocator->allocate(1337);
            CHECK_EQUAL((u32)0, a.offset);
            allocator->free(a);

            ncore::noffset::allocation_t b = allocator->allocate(1337);
            CHECK_EQUAL((u32)0, b.offset);
            allocator->free(b);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL((u32)0, validateAll.offset);
            allocator->free(validateAll);
        }

        UNITTEST_TEST(reuse_trivial)
        {
            // Allocator should reuse node freed by A since the allocation C fits in the same bin (using pow2 size to be sure)
            ncore::noffset::allocation_t a = allocator->allocate(1024);
            CHECK_EQUAL((u32)0, a.offset);

            ncore::noffset::allocation_t b = allocator->allocate(3456);
            CHECK_EQUAL((u32)1024, b.offset);
            allocator->free(a);

            ncore::noffset::allocation_t c = allocator->allocate(1024);
            CHECK_EQUAL((u32)0, c.offset);
            allocator->free(c);
            allocator->free(b);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL((u32)0, validateAll.offset);
            allocator->free(validateAll);
        }

        UNITTEST_TEST(reuse_complex)
        {
            // Allocator should not reuse node freed by A since the allocation C doesn't fits in the same bin
            // However node D and E fit there and should reuse node from A
            ncore::noffset::allocation_t a = allocator->allocate(1024);
            CHECK_EQUAL((u32)0, a.offset);

            ncore::noffset::allocation_t b = allocator->allocate(3456);
            CHECK_EQUAL((u32)1024, b.offset);

            allocator->free(a);

            ncore::noffset::allocation_t c = allocator->allocate(2345);
            CHECK_EQUAL((u32)(1024 + 3456), c.offset);

            ncore::noffset::allocation_t d = allocator->allocate(456);
            CHECK_EQUAL((u32)0, d.offset);

            ncore::noffset::allocation_t e = allocator->allocate(512);
            CHECK_EQUAL((u32)456, e.offset);

            ncore::noffset::storage_report_t report = allocator->storageReport();
            CHECK_EQUAL((u32)1024 * 1024 * 256 - 3456 - 2345 - 456 - 512, report.totalFreeSpace);
            CHECK_NOT_EQUAL(report.totalFreeSpace, report.largestFreeRegion);

            allocator->free(c);
            allocator->free(d);
            allocator->free(b);
            allocator->free(e);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL((u32)0, validateAll.offset);
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
            CHECK_EQUAL((u32)0, report.totalFreeSpace);
            CHECK_EQUAL((u32)0, report.largestFreeRegion);

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
            allocations[151] = allocator->allocate(1024 * 1024 * 4); // 4x larger

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
            CHECK_EQUAL((u32)1024 * 1024 * 256, report2.totalFreeSpace);
            CHECK_EQUAL((u32)1024 * 1024 * 256, report2.largestFreeRegion);

            // End: Validate that allocator has no fragmentation left. Should be 100% clean.
            ncore::noffset::allocation_t validateAll = allocator->allocate(1024 * 1024 * 256);
            CHECK_EQUAL((u32)0, validateAll.offset);
            allocator->free(validateAll);
        }
    }
}
UNITTEST_SUITE_END
