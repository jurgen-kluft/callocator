#include "xbase/x_bitlist.h"
#include "xunittest/xunittest.h"

using namespace xcore;

UNITTEST_SUITE_BEGIN(xhibitset)
{
	UNITTEST_FIXTURE(main)
	{
		UNITTEST_FIXTURE_SETUP() {}
		UNITTEST_FIXTURE_TEARDOWN() {}

		static u32 bitmap[4096];

		UNITTEST_TEST(set)
		{
			xhibitset bset;
			bset.init(bitmap, 8192, xhibitset::FIND_0);
			bset.reset();

			CHECK_EQUAL(false, bset.is_set(10));
			bset.set(10);
			CHECK_EQUAL(true, bset.is_set(10));
		}

		UNITTEST_TEST(find_free_bit_1)
		{
			xhibitset bset;
			bset.init(bitmap, 8192, xhibitset::FIND_0);
			bset.reset();

			for (s32 b=0; b<1024; b++)
			{
				u32 free_bit;
				bset.find(free_bit);
				CHECK_EQUAL(b, free_bit);
				bset.set(b);
			}
		}

		UNITTEST_TEST(find_free_bit_2)
		{
			xhibitset bset;
			bset.init(bitmap, 8192, xhibitset::FIND_1);
			bset.reset();

			// Should not be able to find any '1'
			u32 free_bit;
			CHECK_EQUAL(false, bset.find(free_bit));

			for (s32 b=1024-1; b>=0; --b)
			{
				bset.set(b);
				bset.find(free_bit);
				CHECK_EQUAL(b, free_bit);
			}
		}

	}
}
UNITTEST_SUITE_END
