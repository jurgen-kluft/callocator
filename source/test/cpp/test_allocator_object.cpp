#include "ccore/c_allocator.h"
#include "callocator/c_allocator_object.h"

#include "cunittest/cunittest.h"

using namespace ncore;

namespace ncore
{
    namespace nobject
    {
        enum EObjectTypes
        {
            kObjectA = 0,
            kObjectB = 1,
        };

        enum EComponentTypes
        {
            kComponentA = 0,
            kComponentB = 1,
            kComponentC = 2,
        };

        struct object_a_t
        {
            D_DECLARE_OBJECT_TYPE(kObjectA);
            int   a;
            int   b;
            float c;
        };

        struct object_b_t
        {
            D_DECLARE_OBJECT_TYPE(kObjectB);
            int   a;
            int   b;
            float c;
        };

        struct component_a_t
        {
            D_DECLARE_COMPONENT_TYPE(kComponentA);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        struct component_b_t
        {
            D_DECLARE_COMPONENT_TYPE(kComponentB);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        struct component_c_t
        {
            D_DECLARE_COMPONENT_TYPE(kComponentC);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

    } // namespace nobject
} // namespace ncore

UNITTEST_SUITE_BEGIN(nobject)
{
    // Test the 'C' style array
    UNITTEST_FIXTURE(array)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(setup_teardown)
        {
            nobject::array_t array;
            array.setup(Allocator, 1024, 32);
            array.teardown(Allocator);
        }

        struct my_object_t
        {
            int   ints[4];
            float floats[4];
            void  setup(u32 i) { ints[0] = ints[1] = ints[2] = ints[3] = floats[0] = floats[1] = floats[2] = floats[3] = i; }
            bool  verify(u32 i) { return ints[0] == i && ints[1] == i && ints[2] == i && ints[3] == i && floats[0] == i && floats[1] == i && floats[2] == i && floats[3] == i; }
        };

        UNITTEST_TEST(get_access)
        {
            nobject::array_t array;
            array.setup(Allocator, 1024, sizeof(my_object_t));

            for (u32 i = 0; i < 1024; ++i)
            {
                my_object_t* ptr = array.get_access_as<my_object_t>(i);
                CHECK_NOT_NULL(ptr);
                ptr->setup(i);
            }

            for (u32 i = 0; i < 1024; ++i)
            {
                my_object_t* ptr = array.get_access_as<my_object_t>(i);
                CHECK_NOT_NULL(ptr);
                CHECK_TRUE(ptr->verify(i));
            }

            array.teardown(Allocator);
        }
    }

    // Test the 'C' style resource pool
    UNITTEST_FIXTURE(pool)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(test_init_shutdown)
        {
            nobject::array_t array;
            array.setup(Allocator, 1024, 32);
            nobject::pool_t pool;
            pool.setup(array, Allocator);

            pool.teardown(Allocator);
        }

        UNITTEST_TEST(obtain_release)
        {
            nobject::array_t array;
            array.setup(Allocator, 1024, 32);
            nobject::pool_t pool;
            pool.setup(array, Allocator);

            u32 r = pool.allocate();
            CHECK_EQUAL(0, r);
            pool.deallocate(r);

            pool.teardown(Allocator);
        }
    }

    // Test the typed pool
    UNITTEST_FIXTURE(types)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        struct myobject
        {
            int   a;
            int   b;
            float c;
        };

        UNITTEST_TEST(test_init_shutdown)
        {
            nobject::ntyped::pool_t<myobject> pool;
            pool.setup(Allocator, 32);
            pool.teardown();
        }

        UNITTEST_TEST(obtain_release)
        {
            nobject::ntyped::pool_t<myobject> pool;
            pool.setup(Allocator, 32);

            myobject* r1 = pool.obtain_access();
            r1->a        = 1;
            r1->b        = 2;
            r1->c        = 3.14f;
            myobject* r2 = pool.obtain_access();
            r2->a        = 4;
            r2->b        = 5;
            r2->c        = 6.28f;

            myobject* r11 = pool.get_access(0);
            CHECK_EQUAL(1, r11->a);
            CHECK_EQUAL(2, r11->b);
            CHECK_EQUAL(3.14f, r11->c);

            myobject* r22 = pool.get_access(1);
            CHECK_EQUAL(4, r22->a);
            CHECK_EQUAL(5, r22->b);
            CHECK_EQUAL(6.28f, r22->c);

            pool.deallocate(0);
            pool.deallocate(1);

            pool.teardown();
        }
    }

    // Test the object components pool
    UNITTEST_FIXTURE(object_components)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(test_init_shutdown)
        {
            nobject::nobjects_with_components::pool_t pool;
            pool.setup(Allocator, 32);
            pool.teardown();
        }

        UNITTEST_TEST(register_object_and_component_types)
        {
            nobject::nobjects_with_components::pool_t pool;
            pool.setup(Allocator, 30);

            bool obj_a = pool.register_object<nobject::object_a_t>(40, 10, 10);
            CHECK_TRUE(obj_a);

            bool obj_b = pool.register_object<nobject::object_b_t>(40, 10, 10);
            CHECK_TRUE(obj_b);

            CHECK_TRUE(pool.is_object_registered<nobject::object_a_t>());
            CHECK_TRUE(pool.is_object_registered<nobject::object_b_t>());

            bool obj_a_res_a = pool.register_component<nobject::object_a_t, nobject::component_a_t>(10, "a");
            CHECK_TRUE(obj_a_res_a);

            bool obj_a_res_b = pool.register_component<nobject::object_a_t, nobject::component_b_t>(10, "b");
            CHECK_TRUE(obj_a_res_b);

            bool obj_a_res_c = pool.register_component<nobject::object_a_t, nobject::component_c_t>(10, "c");
            CHECK_TRUE(obj_a_res_c);

            bool obj_b_res_a = pool.register_component<nobject::object_b_t, nobject::component_a_t>(10, "a");
            CHECK_TRUE(obj_b_res_a);

            bool obj_a_cp_a_registered = pool.is_component_registered<nobject::object_a_t, nobject::component_a_t>();
            CHECK_TRUE(obj_a_cp_a_registered);
            bool obj_a_cp_b_registered = pool.is_component_registered<nobject::object_a_t, nobject::component_b_t>();
            CHECK_TRUE(obj_a_cp_b_registered);
            bool obj_a_cp_c_registered = pool.is_component_registered<nobject::object_a_t, nobject::component_c_t>();
            CHECK_TRUE(obj_a_cp_c_registered);

            nobject::object_a_t* oa1 = pool.create_object<nobject::object_a_t>();
            nobject::object_a_t* oa2 = pool.create_object<nobject::object_a_t>();
            nobject::object_b_t* ob1 = pool.create_object<nobject::object_b_t>();

            nobject::component_a_t* oaca = pool.add_component<nobject::component_a_t>(oa1);
            nobject::component_b_t* oacb = pool.add_component<nobject::component_b_t>(oa1);
            nobject::component_c_t* oacc = pool.add_component<nobject::component_c_t>(oa1);

            nobject::component_a_t* obca = pool.add_component<nobject::component_a_t>(ob1);
            nobject::component_b_t* obcb = pool.add_component<nobject::component_b_t>(ob1);

            nobject::object_b_t*    objb   = pool.create_object<nobject::object_b_t>();
            nobject::component_a_t* objbca = pool.add_component<nobject::component_a_t>(objb);
            nobject::component_b_t* objbcb = pool.add_component<nobject::component_b_t>(objb);
            pool.rem_component<nobject::component_a_t>(objb);
            pool.rem_component<nobject::component_b_t>(objb);
            pool.destroy_object<nobject::object_b_t>(objb);

            bool obj_a_has_cp_a = pool.has_component<nobject::component_a_t>(oa1);
            CHECK_TRUE(obj_a_has_cp_a);
            bool obj_a_has_cp_b = pool.has_component<nobject::component_b_t>(oa1);
            CHECK_TRUE(obj_a_has_cp_b);
            bool obj_a_has_cp_c = pool.has_component<nobject::component_c_t>(oa1);
            CHECK_TRUE(obj_a_has_cp_c);
            nobject::component_a_t* cpa1 = pool.get_component<nobject::component_a_t>(oa1);
            CHECK_NOT_NULL(cpa1);

            pool.rem_component<nobject::component_a_t>(oa1);
            pool.rem_component<nobject::component_b_t>(oa1);
            pool.rem_component<nobject::component_c_t>(oa1);

            pool.rem_component<nobject::component_a_t>(ob1);
            pool.rem_component<nobject::component_b_t>(ob1);

            pool.destroy_object<nobject::object_a_t>(oa1);
            pool.destroy_object<nobject::object_a_t>(oa2);
            pool.destroy_object<nobject::object_b_t>(ob1);

            pool.teardown();
        }
    }
}
UNITTEST_SUITE_END
