#include "ccore/c_allocator.h"
#include "ccore/c_arena.h"

#include "callocator/c_allocator_stack.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(stack)
{
    UNITTEST_FIXTURE(main)
    {
        // UNITTEST_ALLOCATOR;

        int_t          stack_initial_size  = 1 * cMB;  // Initial size of the stack allocator
        int_t          stack_reserved_size = 64 * cMB; // Reserved size for the stack allocator
        stack_alloc_t* stack_alloc         = nullptr;

        UNITTEST_FIXTURE_SETUP() { stack_alloc = g_create_stack_allocator(stack_initial_size, stack_reserved_size); }

        UNITTEST_FIXTURE_TEARDOWN() { g_destroy_stack_allocator(stack_alloc); }

        UNITTEST_TEST(allocN_freeN)
        {
            CHECK_TRUE(*((int_t*)stack_alloc->save_point()) == 0);

            for (s32 i = 0; i < 3; ++i)
            {
                defer_t scope(stack_alloc);

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
                    defer_t scope2(stack_alloc);
                    void*   mem6 = stack_alloc->allocate(8, 8);
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

            CHECK_TRUE(*((int_t*)stack_alloc->save_point()) == 0);
        }

        UNITTEST_TEST(construct_destruct)
        {
            CHECK_TRUE(*((int_t*)stack_alloc->save_point()) == 0);

            for (s32 i = 0; i < 12; ++i)
            {
                defer_t scope(stack_alloc);

                struct test_t
                {
                    DCORE_CLASS_PLACEMENT_NEW_DELETE
                    s32 a;
                    s32 b;
                    s32 c;
                    s32 d;
                };

                test_t* test = g_construct<test_t>(stack_alloc);
                CHECK_NOT_NULL(test);
                test->a = 1;
                test->b = 2;
                test->c = 3;
                test->d = 4;
                g_destruct(stack_alloc, test);
            }

            CHECK_TRUE(*((int_t*)stack_alloc->save_point()) == 0);
        }
    }
}
UNITTEST_SUITE_END
