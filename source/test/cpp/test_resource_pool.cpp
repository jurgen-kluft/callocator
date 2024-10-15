#include "ccore/c_allocator.h"
#include "callocator/c_resource_pool.h"

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

        enum EResourceTypes
        {
            kComponentA = 0,
            kComponentB = 1,
            kComponentC = 2,
        };

        enum ETagTypes
        {
            kTagA = 0,
            kTagB = 1,
        };

        struct object_a_t
        {
            DECLARE_OBJECT_TYPE(kObjectA);
            int   a;
            int   b;
            float c;
        };

        struct object_b_t
        {
            DECLARE_OBJECT_TYPE(kObjectB);
            int   a;
            int   b;
            float c;
        };

        struct component_a_t
        {
            DECLARE_COMPONENT_TYPE(kComponentA);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        struct component_b_t
        {
            DECLARE_COMPONENT_TYPE(kComponentB);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        struct component_c_t
        {
            DECLARE_COMPONENT_TYPE(kComponentC);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        struct tag_a_t
        {
            DECLARE_TAG_TYPE(kTagA);
        };

        struct tag_b_t
        {
            DECLARE_TAG_TYPE(kTagB);
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
            pool.setup(&array, Allocator);

            pool.teardown(Allocator);
            array.teardown(Allocator);
        }

        UNITTEST_TEST(obtain_release)
        {
            nobject::array_t array;
            array.setup(Allocator, 1024, 32);
            nobject::pool_t pool;
            pool.setup(&array, Allocator);

            u32 r = pool.allocate();
            CHECK_EQUAL(0, r);
            pool.deallocate(r);

            pool.teardown(Allocator);
            array.teardown(Allocator);
        }
    }

    // Test the typed resource pool
    UNITTEST_FIXTURE(types)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        struct myresource_t
        {
            int   a;
            int   b;
            float c;
        };

        UNITTEST_TEST(test_init_shutdown)
        {
            nobject::ntyped::pool_t<myresource_t> pool;
            pool.setup(Allocator, 32);
            pool.teardown();
        }

        UNITTEST_TEST(obtain_release)
        {
            nobject::ntyped::pool_t<myresource_t> pool;
            pool.setup(Allocator, 32);

            myresource_t* r1 = pool.obtain_access();
            r1->a            = 1;
            r1->b            = 2;
            r1->c            = 3.14f;
            myresource_t* r2 = pool.obtain_access();
            r2->a            = 4;
            r2->b            = 5;
            r2->c            = 6.28f;

            myresource_t* r11 = pool.get_access(0);
            CHECK_EQUAL(1, r11->a);
            CHECK_EQUAL(2, r11->b);
            CHECK_EQUAL(3.14f, r11->c);

            myresource_t* r22 = pool.get_access(1);
            CHECK_EQUAL(4, r22->a);
            CHECK_EQUAL(5, r22->b);
            CHECK_EQUAL(6.28f, r22->c);

            pool.deallocate(0);
            pool.deallocate(1);

            pool.teardown();
        }
    }

    // Test the resources pool
    UNITTEST_FIXTURE(resources)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(test_init_shutdown)
        {
            nobject::ncomponents::pool_t pool;
            pool.setup(Allocator, 32);
            pool.teardown();
        }

        UNITTEST_TEST(register_resource_types)
        {
            nobject::ncomponents::pool_t pool;
            pool.setup(Allocator, 4);

            CHECK_TRUE(pool.register_resource<nobject::component_a_t>(32));
            CHECK_TRUE(pool.register_resource<nobject::component_b_t>(32));
            CHECK_TRUE(pool.register_resource<nobject::component_c_t>(32));

            nobject::handle_t h1 = pool.allocate<nobject::component_a_t>();
            CHECK_EQUAL(0, h1.index);
            CHECK_EQUAL(0, h1.type[0]);
            nobject::handle_t h2 = pool.allocate<nobject::component_b_t>();
            CHECK_EQUAL(0, h2.index);
            CHECK_EQUAL(1, h2.type[0]);
            nobject::handle_t h3 = pool.construct<nobject::component_c_t>();
            CHECK_EQUAL(0, h3.index);
            CHECK_EQUAL(2, h3.type[0]);

            CHECK_TRUE(pool.is_resource_type<nobject::component_a_t>(h1));
            CHECK_FALSE(pool.is_resource_type<nobject::component_b_t>(h1));
            CHECK_FALSE(pool.is_resource_type<nobject::component_c_t>(h1));

            CHECK_TRUE(pool.is_resource_type<nobject::component_b_t>(h2));
            CHECK_FALSE(pool.is_resource_type<nobject::component_a_t>(h2));
            CHECK_FALSE(pool.is_resource_type<nobject::component_c_t>(h2));

            CHECK_TRUE(pool.is_resource_type<nobject::component_c_t>(h3));
            CHECK_FALSE(pool.is_resource_type<nobject::component_a_t>(h3));
            CHECK_FALSE(pool.is_resource_type<nobject::component_b_t>(h3));

            pool.deallocate(h1);
            pool.deallocate(h2);
            pool.destruct<nobject::component_c_t>(h3);

            nobject::handle_t h4 = pool.construct<nobject::component_a_t>();
            CHECK_EQUAL(0, h4.index);
            CHECK_EQUAL(0, h4.type[0]);
            pool.destruct<nobject::component_a_t>(h4);

            pool.teardown();
        }
    }

    // Test the object resources pool
    UNITTEST_FIXTURE(object_resources)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(test_init_shutdown)
        {
            nobject::nobjects_with_components::pool_t pool;
            pool.setup(Allocator, 32, 4);
            pool.teardown();
        }

        UNITTEST_TEST(register_object_and_resource_types)
        {
            nobject::nobjects_with_components::pool_t pool;
            pool.setup(Allocator, 30, 30);

            bool obj_a = pool.register_object_type<nobject::object_a_t>(40, 10);
            CHECK_TRUE(obj_a);

            bool obj_b = pool.register_object_type<nobject::object_b_t>(40, 10);
            CHECK_TRUE(obj_b);

            bool obj_a_res_a = pool.register_component_type<nobject::object_a_t, nobject::component_a_t>();
            CHECK_TRUE(obj_a_res_a);

            bool obj_a_res_b = pool.register_component_type<nobject::object_a_t, nobject::component_b_t>();
            CHECK_TRUE(obj_a_res_b);

            bool obj_a_res_c = pool.register_component_type<nobject::object_a_t, nobject::component_c_t>();
            CHECK_TRUE(obj_a_res_c);

            nobject::handle_t oa1 = pool.allocate_object<nobject::object_a_t>();
            CHECK_EQUAL(0, oa1.index & 0x0FFFFFFF);
            CHECK_EQUAL(0, oa1.type[1]);

            nobject::handle_t oa2 = pool.allocate_object<nobject::object_a_t>();
            CHECK_EQUAL(1, oa2.index & 0x0FFFFFFF);
            CHECK_EQUAL(0, oa2.type[1]);

            nobject::handle_t ob1 = pool.allocate_object<nobject::object_b_t>();
            CHECK_EQUAL(0, ob1.index & 0x0FFFFFFF);
            CHECK_EQUAL(1, ob1.type[0]);
            CHECK_EQUAL(0, ob1.type[1]);

            nobject::handle_t h1 = pool.allocate_component<nobject::component_a_t>(oa1);
            CHECK_EQUAL(0, h1.index & 0x00FFFFFF);
            CHECK_EQUAL(0, h1.type[0]);
            CHECK_EQUAL(1, h1.type[1]);
            nobject::handle_t h2 = pool.construct_component<nobject::component_b_t>(oa1);
            CHECK_EQUAL(0, h2.index & 0x00FFFFFF);
            CHECK_EQUAL(0, h2.type[0]);
            CHECK_EQUAL(2, h2.type[1]);
            nobject::handle_t h3 = pool.allocate_component<nobject::component_c_t>(oa1);
            CHECK_EQUAL(0, h3.index & 0x00FFFFFF);
            CHECK_EQUAL(3, h3.type[1]);

            CHECK_TRUE(pool.is_object<nobject::object_a_t>(oa1));
            CHECK_FALSE(pool.is_object<nobject::object_b_t>(oa1));
            CHECK_TRUE(pool.is_object<nobject::object_b_t>(ob1));
            CHECK_FALSE(pool.is_object<nobject::object_a_t>(ob1));

            CHECK_TRUE(pool.is_component<nobject::component_a_t>(h1));
            CHECK_FALSE(pool.is_component<nobject::component_b_t>(h1));
            CHECK_FALSE(pool.is_component<nobject::component_c_t>(h1));

            CHECK_TRUE(pool.is_component<nobject::component_b_t>(h2));
            CHECK_FALSE(pool.is_component<nobject::component_a_t>(h2));
            CHECK_FALSE(pool.is_component<nobject::component_c_t>(h2));

            CHECK_TRUE(pool.is_component<nobject::component_c_t>(h3));
            CHECK_FALSE(pool.is_component<nobject::component_a_t>(h3));
            CHECK_FALSE(pool.is_component<nobject::component_b_t>(h3));

            // You can get a component through the handle of the object or the handle of the component.
            // The handle of the component knows the type of the component but also the type of the object.
            CHECK_TRUE(pool.has_component<nobject::component_a_t>(oa1));
            CHECK_TRUE(pool.has_component<nobject::component_b_t>(oa1));
            CHECK_TRUE(pool.has_component<nobject::component_c_t>(oa1));
            nobject::component_a_t* cpa1 = pool.get_component<nobject::component_a_t>(oa1);
            CHECK_NOT_NULL(cpa1);
            nobject::component_a_t* cpa2 = pool.get_component<nobject::component_a_t>(h1);
            CHECK_NOT_NULL(cpa2);
            CHECK_EQUAL(cpa1, cpa2);

            pool.add_tag<nobject::tag_a_t>(oa1);
            pool.add_tag<nobject::tag_b_t>(oa1);
            pool.add_tag<nobject::tag_b_t>(oa2);
            pool.add_tag<nobject::tag_a_t>(ob1);

            CHECK_TRUE(pool.has_tag<nobject::tag_a_t>(oa1));
            CHECK_TRUE(pool.has_tag<nobject::tag_b_t>(oa1));
            CHECK_FALSE(pool.has_tag<nobject::tag_a_t>(oa2));
            CHECK_TRUE(pool.has_tag<nobject::tag_b_t>(oa2));
            CHECK_TRUE(pool.has_tag<nobject::tag_a_t>(ob1));
            CHECK_FALSE(pool.has_tag<nobject::tag_b_t>(ob1));

            pool.rem_tag<nobject::tag_a_t>(oa1);
            pool.rem_tag<nobject::tag_b_t>(oa1);
            pool.rem_tag<nobject::tag_b_t>(oa2);
            pool.rem_tag<nobject::tag_a_t>(ob1);

            CHECK_FALSE(pool.has_tag<nobject::tag_a_t>(oa1));
            CHECK_FALSE(pool.has_tag<nobject::tag_b_t>(oa1));
            CHECK_FALSE(pool.has_tag<nobject::tag_a_t>(oa2));
            CHECK_FALSE(pool.has_tag<nobject::tag_b_t>(oa2));
            CHECK_FALSE(pool.has_tag<nobject::tag_a_t>(ob1));
            CHECK_FALSE(pool.has_tag<nobject::tag_b_t>(ob1));

            pool.deallocate_component(h1);
            pool.destruct_component<nobject::component_b_t>(h2);
            pool.deallocate_component(h3);

            pool.deallocate_object(oa1);
            pool.deallocate_object(oa2);
            pool.destruct_object<nobject::object_b_t>(ob1);

            pool.teardown();
        }
    }
}
UNITTEST_SUITE_END
