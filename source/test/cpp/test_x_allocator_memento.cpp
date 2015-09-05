#include "xbase\x_allocator.h"
#include "xbase\x_console.h"
#include "xallocator\x_allocator_memento.h"

#include "xunittest\xunittest.h"

using namespace xcore;

extern x_iallocator* gSystemAllocator;

class x_memento_print_test : public x_memento_print
{
public:
	virtual void print(const char* format, const char* str) 
	{
		xconsole::write(format, x_va_list(x_va(str)));
	}

	virtual void print(const char* format, void* ptr) 
	{ 
		xconsole::write(format, x_va_list(x_va(ptr)));
	}

	virtual void print(const char* format, int n, int value1, int value2 = 0, int value3 = 0, int value4 = 0) 
	{
		switch (n)
		{
		case 1:
			xconsole::write(format, x_va_list(x_va(value1)));
			break;
		case 2:
			xconsole::write(format, x_va_list(x_va(value1), x_va(value2)));
			break;
		case 3:
			xconsole::write(format, x_va_list(x_va(value1), x_va(value2), x_va(value3)));
			break;
		}
	}
};


UNITTEST_SUITE_BEGIN(x_allocator_memento)
{
	UNITTEST_FIXTURE(main)
	{
		x_memento_print_test	memento_printer;
		x_iallocator*			memento_allocator;
		x_memento_config		memento_config;

		UNITTEST_FIXTURE_SETUP()
		{
			memento_config.init(&memento_printer);
			memento_config.m_freemaxsizekeep = 0;	/// Do not keep any allocations

			memento_allocator = gCreateMementoAllocator(memento_config, gSystemAllocator);
		}

		UNITTEST_FIXTURE_TEARDOWN()
		{
			memento_allocator->release();
		}

		UNITTEST_TEST(alloc_preguard)
		{
			u32* data = (u32*)memento_allocator->allocate(32, 8);
			u32 const g = data[-1];
			data[-1] = 100;
			memento_allocator->deallocate(data);
			data[-1] = g;
			memento_allocator->deallocate(data);
		}
		UNITTEST_TEST(alloc_postguard)
		{
			u32* data = (u32*)memento_allocator->allocate(32, 8);
			u32 const g = data[8];
			data[8] = 100;
			memento_allocator->deallocate(data);
			data[8] = g;
			memento_allocator->deallocate(data);
		}
		UNITTEST_TEST(alloc_preandpostguard)
		{
			u32* data = (u32*)memento_allocator->allocate(32, 8);
			u32 const g1 = data[-1];
			u32 const g2 = data[8];
			data[-1] = 100;
			data[8] = 100;
			memento_allocator->deallocate(data);
			data[-1] = g1;
			data[8] = g2;
			memento_allocator->deallocate(data);
		}

	}
}
UNITTEST_SUITE_END

