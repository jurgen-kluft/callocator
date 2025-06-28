#include "ccore/c_allocator.h"
#include "ccore/c_vmem.h"

#include "callocator/c_allocator_stack.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(stack)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        vmem_arena_t*      arena       = nullptr;
        stack_allocator_t* stack_alloc = nullptr;

        UNITTEST_FIXTURE_SETUP()
        {
            vmem_arena_t a;
            a.reserved(1 * cMB);
            a.committed(64 * cKB);
            arena = (vmem_arena_t*)a.commit(sizeof(vmem_arena_t));

            stack_alloc     = g_construct<stack_allocator_t>(Allocator);
            stack_alloc->setup(arena);
        }

        UNITTEST_FIXTURE_TEARDOWN()
        {
            Allocator->deallocate(stack_alloc);
            stack_alloc = nullptr;
            arena->release();
            arena = nullptr;
        }

        UNITTEST_TEST(allocN_freeN)
        {
            CHECK_TRUE(stack_alloc->m_allocation_count == 0);

            for (s32 i = 0; i < 12; ++i)
            {
                stack_alloc_scope_t scope(stack_alloc);

                void* mem1 = scope.allocate(512, 8);
                void* mem2 = scope.allocate(1024, 16);
                void* mem3 = scope.allocate(512, 32);
                void* mem4 = scope.allocate(1024, 256);
                void* mem5 = scope.allocate(256, 32);
                CHECK_NOT_NULL(mem1);
                CHECK_NOT_NULL(mem2);
                CHECK_NOT_NULL(mem3);
                CHECK_NOT_NULL(mem4);
                CHECK_NOT_NULL(mem5);

                scope.deallocate(mem4);
                mem4 = nullptr;

                {
                    stack_alloc_scope_t scope2(stack_alloc);
                    void*               mem6 = scope.allocate(8, 8);
                    CHECK_NOT_NULL(mem6);
                    scope.deallocate(mem6);
                    mem6 = nullptr;
                }

                void* mem7 = scope.allocate(2048, 256);
                void* mem8 = scope.allocate(1024, 256);
                CHECK_NOT_NULL(mem7);
                CHECK_NOT_NULL(mem8);

                scope.deallocate(mem1);
                mem1 = nullptr;
                scope.deallocate(mem3);
                mem3 = nullptr;
                scope.deallocate(mem2);
                mem2 = nullptr;

                void* mem9 = scope.allocate(16, 8);
                CHECK_NOT_NULL(mem9);

                scope.deallocate(mem7);
                mem7 = nullptr;
                scope.deallocate(mem5);
                mem5 = nullptr;

                // This should wrap around
                void* mema = scope.allocate(2048, 8);
                CHECK_NOT_NULL(mema);

                scope.deallocate(mem9);
                mem9 = nullptr;
                scope.deallocate(mem8);
                mem8 = nullptr;
                scope.deallocate(mema);
                mema = nullptr;
            }

            CHECK_TRUE(stack_alloc->m_allocation_count == 0);
        }

        UNITTEST_TEST(construct_destruct)
        {
            CHECK_TRUE(stack_alloc->m_allocation_count == 0);

            for (s32 i = 0; i < 12; ++i)
            {
                stack_alloc_scope_t scope(stack_alloc);

                struct test_t
                {
                    DCORE_CLASS_PLACEMENT_NEW_DELETE
                    s32 a;
                    s32 b;
                    s32 c;
                    s32 d;
                };

                test_t* test = g_construct<test_t>(&scope);
                CHECK_NOT_NULL(test);
                test->a = 1;
                test->b = 2;
                test->c = 3;
                test->d = 4;
                g_destruct(&scope, test);
            }

            CHECK_TRUE(stack_alloc->m_allocation_count == 0);
        }
    }
}
UNITTEST_SUITE_END
