#include "ccore/c_allocator.h"
#include "callocator/c_allocator_object.h"

#include "cunittest/cunittest.h"

using namespace ncore;

namespace ncore
{
    namespace nocs
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

    } // namespace nocs
} // namespace ncore

UNITTEST_SUITE_BEGIN(nocs)
{
    // Test the object components pool
    UNITTEST_FIXTURE(object_components)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(test_init_shutdown)
        {
            nocs::allocator_t pool;
            pool.setup(Allocator, 32);
            pool.teardown();
        }

        UNITTEST_TEST(register_object_and_component_types)
        {
            nocs::allocator_t pool;
            pool.setup(Allocator, 30);

            bool obj_a = pool.register_object<nocs::object_a_t>(40, 10, 10);
            CHECK_TRUE(obj_a);

            bool obj_b = pool.register_object<nocs::object_b_t>(40, 10, 10);
            CHECK_TRUE(obj_b);

            CHECK_TRUE(pool.is_object_registered<nocs::object_a_t>());
            CHECK_TRUE(pool.is_object_registered<nocs::object_b_t>());

            bool obj_a_res_a = pool.register_component<nocs::object_a_t, nocs::component_a_t>(10, "a");
            CHECK_TRUE(obj_a_res_a);

            bool obj_a_res_b = pool.register_component<nocs::object_a_t, nocs::component_b_t>(10, "b");
            CHECK_TRUE(obj_a_res_b);

            bool obj_a_res_c = pool.register_component<nocs::object_a_t, nocs::component_c_t>(10, "c");
            CHECK_TRUE(obj_a_res_c);

            bool obj_b_res_a = pool.register_component<nocs::object_b_t, nocs::component_a_t>(10, "a");
            CHECK_TRUE(obj_b_res_a);

            bool obj_a_cp_a_registered = pool.is_component_registered<nocs::object_a_t, nocs::component_a_t>();
            CHECK_TRUE(obj_a_cp_a_registered);
            bool obj_a_cp_b_registered = pool.is_component_registered<nocs::object_a_t, nocs::component_b_t>();
            CHECK_TRUE(obj_a_cp_b_registered);
            bool obj_a_cp_c_registered = pool.is_component_registered<nocs::object_a_t, nocs::component_c_t>();
            CHECK_TRUE(obj_a_cp_c_registered);

            nocs::object_a_t* oa1 = pool.create_object<nocs::object_a_t>();
            nocs::object_a_t* oa2 = pool.create_object<nocs::object_a_t>();
            nocs::object_b_t* ob1 = pool.create_object<nocs::object_b_t>();

            nocs::component_a_t* oaca = pool.add_component<nocs::component_a_t>(oa1);
            nocs::component_b_t* oacb = pool.add_component<nocs::component_b_t>(oa1);
            nocs::component_c_t* oacc = pool.add_component<nocs::component_c_t>(oa1);

            nocs::component_a_t* obca = pool.add_component<nocs::component_a_t>(ob1);
            nocs::component_b_t* obcb = pool.add_component<nocs::component_b_t>(ob1);

            nocs::object_b_t*    objb   = pool.create_object<nocs::object_b_t>();
            nocs::component_a_t* objbca = pool.add_component<nocs::component_a_t>(objb);
            nocs::component_b_t* objbcb = pool.add_component<nocs::component_b_t>(objb);
            pool.rem_component<nocs::component_a_t>(objb);
            pool.rem_component<nocs::component_b_t>(objb);
            pool.destroy_object<nocs::object_b_t>(objb);

            bool obj_a_has_cp_a = pool.has_component<nocs::component_a_t>(oa1);
            CHECK_TRUE(obj_a_has_cp_a);
            bool obj_a_has_cp_b = pool.has_component<nocs::component_b_t>(oa1);
            CHECK_TRUE(obj_a_has_cp_b);
            bool obj_a_has_cp_c = pool.has_component<nocs::component_c_t>(oa1);
            CHECK_TRUE(obj_a_has_cp_c);
            nobject::component_a_t* cpa1 = pool.get_component<nocs::component_a_t>(oa1);
            CHECK_NOT_NULL(cpa1);

            pool.rem_component<nocs::component_a_t>(oa1);
            pool.rem_component<nocs::component_b_t>(oa1);
            pool.rem_component<nocs::component_c_t>(oa1);

            pool.rem_component<nocs::component_a_t>(ob1);
            pool.rem_component<nocs::component_b_t>(ob1);

            pool.destroy_object<nocs::object_a_t>(oa1);
            pool.destroy_object<nocs::object_a_t>(oa2);
            pool.destroy_object<nocs::object_b_t>(ob1);

            pool.teardown();
        }
    }
}
UNITTEST_SUITE_END
