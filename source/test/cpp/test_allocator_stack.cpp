#include "ccore/c_allocator.h"
#include "callocator/c_allocator_stack.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(stack)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        void*          stack_alloc_mem = nullptr;
        stack_alloc_t* stack_alloc;

        UNITTEST_FIXTURE_SETUP()
        {
            stack_alloc_mem = Allocator->allocate(1 * cMB);
            stack_alloc     = Allocator->construct<stack_alloc_t>();
            stack_alloc->setup(stack_alloc_mem, 1 * cMB);
        }

        UNITTEST_FIXTURE_TEARDOWN()
        {
            Allocator->deallocate(stack_alloc_mem);
            Allocator->deallocate(stack_alloc);
        }

        UNITTEST_TEST(allocN_freeN)
        {
            for (s32 i = 0; i < 12; ++i)
            {
                stack_alloc_scope_t scope(stack_alloc);

                void* mem1 = stack_alloc->allocate(512, 8);
                void* mem2 = stack_alloc->allocate(1024, 16);
                void* mem3 = stack_alloc->allocate(512, 32);
                void* mem4 = stack_alloc->allocate(1024, 256);
                void* mem5 = stack_alloc->allocate(256, 32);
                CHECK_NOT_NULL(mem1);
                CHECK_NOT_NULL(mem2);
                CHECK_NOT_NULL(mem3);
                CHECK_NOT_NULL(mem4);
                CHECK_NOT_NULL(mem5);

                stack_alloc->deallocate(mem4);
                mem4 = nullptr;

                {
                    stack_alloc_scope_t scope2(stack_alloc);
                    void* mem6 = stack_alloc->allocate(8, 8);
                    CHECK_NOT_NULL(mem6);
                    stack_alloc->deallocate(mem6);
                    mem6 = nullptr;
                }

                void* mem7 = stack_alloc->allocate(2048, 256);
                void* mem8 = stack_alloc->allocate(1024, 256);
                CHECK_NOT_NULL(mem7);
                CHECK_NOT_NULL(mem8);

                stack_alloc->deallocate(mem1);
                mem1 = nullptr;
                stack_alloc->deallocate(mem3);
                mem3 = nullptr;
                stack_alloc->deallocate(mem2);
                mem2 = nullptr;

                void* mem9 = stack_alloc->allocate(16, 8);
                CHECK_NOT_NULL(mem9);

                stack_alloc->deallocate(mem7);
                mem7 = nullptr;
                stack_alloc->deallocate(mem5);
                mem5 = nullptr;

                // This should wrap around
                void* mema = stack_alloc->allocate(2048, 8);
                CHECK_NOT_NULL(mema);

                stack_alloc->deallocate(mem9);
                mem9 = nullptr;
                stack_alloc->deallocate(mem8);
                mem8 = nullptr;
                stack_alloc->deallocate(mema);
                mema = nullptr;
            }
        }
    }
}
UNITTEST_SUITE_END
