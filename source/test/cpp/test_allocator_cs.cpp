#include "ccore/c_allocator.h"
#include "callocator/c_allocator_cs.h"

#include "cunittest/cunittest.h"

using namespace ncore;

namespace ncore
{
    namespace ncs
    {
        enum EComponentTypes
        {
            kObjectA    = 0,
            kObjectB    = 1,
            kComponentA = 2,
            kComponentB = 3,
            kComponentC = 4,
        };

        struct object_a_t
        {
            D_CS_COMPONENT_SET(kObjectA);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        struct object_b_t
        {
            D_CS_COMPONENT_SET(kObjectB);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        struct component_a_t
        {
            D_CS_COMPONENT_SET(kComponentA);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        struct component_b_t
        {
            D_CS_COMPONENT_SET(kComponentB);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        struct component_c_t
        {
            D_CS_COMPONENT_SET(kComponentC);
            int   a;
            int   b;
            float c;
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

    } // namespace ncs
} // namespace ncore

UNITTEST_SUITE_BEGIN(cs)
{
    UNITTEST_FIXTURE(main)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(test_init_shutdown)
        {
            cs_alloc_t pool;
            pool.setup(Allocator, 32, 10, 10);
            pool.teardown();
        }

        UNITTEST_TEST(register_object_and_component_types)
        {
            cs_alloc_t poola;
            poola.setup(Allocator, 30, 10, 10);
            cs_alloc_t poolb;
            poolb.setup(Allocator, 30, 10, 10);

            bool obj_a = poola.register_component<ncs::object_a_t>(40);
            CHECK_TRUE(obj_a);

            bool obj_b = poolb.register_component<ncs::object_b_t>(40);
            CHECK_TRUE(obj_b);

            bool obj_a_res_a = poola.register_component<ncs::component_a_t>(10);
            CHECK_TRUE(obj_a_res_a);

            bool obj_a_res_b = poola.register_component<ncs::component_b_t>(10);
            CHECK_TRUE(obj_a_res_b);

            bool obj_a_res_c = poola.register_component<ncs::component_c_t>(10);
            CHECK_TRUE(obj_a_res_c);

            bool obj_b_res_a = poolb.register_component< ncs::component_a_t>(10);
            CHECK_TRUE(obj_b_res_a);
            bool obj_b_res_b = poolb.register_component< ncs::component_b_t>(10);
            CHECK_TRUE(obj_b_res_b);

            bool obj_a_cp_a_registered = poola.is_component_registered<ncs::component_a_t>();
            CHECK_TRUE(obj_a_cp_a_registered);
            bool obj_a_cp_b_registered = poola.is_component_registered<ncs::component_b_t>();
            CHECK_TRUE(obj_a_cp_b_registered);
            bool obj_a_cp_c_registered = poola.is_component_registered< ncs::component_c_t>();
            CHECK_TRUE(obj_a_cp_c_registered);

            ncs::object_a_t* oa1 = poola.new_instance<ncs::object_a_t>();
            ncs::object_a_t* oa2 = poola.new_instance<ncs::object_a_t>();
            ncs::object_b_t* ob1 = poolb.new_instance<ncs::object_b_t>();

            /*ncs::component_a_t* oaca = */poola.create_component<ncs::object_a_t, ncs::component_a_t>(oa1);
            /*ncs::component_b_t* oacb = */poola.create_component<ncs::object_a_t, ncs::component_b_t>(oa1);
            /*ncs::component_c_t* oacc = */poola.create_component<ncs::object_a_t, ncs::component_c_t>(oa1);

            /*ncs::component_a_t* obca = */poolb.create_component<ncs::object_b_t, ncs::component_a_t>(ob1);
            /*ncs::component_b_t* obcb = */poolb.create_component<ncs::object_b_t, ncs::component_b_t>(ob1);

            ncs::object_b_t*    objb   = poolb.new_instance<ncs::object_b_t>();
            /*ncs::component_a_t* objbca = */poolb.create_component<ncs::object_b_t, ncs::component_a_t>(objb);
            /*ncs::component_b_t* objbcb = */poolb.create_component<ncs::object_b_t, ncs::component_b_t>(objb);
            poolb.destroy_component<ncs::object_b_t, ncs::component_a_t>(objb);
            poolb.destroy_component<ncs::object_b_t, ncs::component_b_t>(objb);
            poolb.destroy_instance<ncs::object_b_t>(objb);

            bool obj_a_has_cp_a = poola.has_component<ncs::object_a_t, ncs::component_a_t>(oa1);
            CHECK_TRUE(obj_a_has_cp_a);
            bool obj_a_has_cp_b = poola.has_component<ncs::object_a_t, ncs::component_b_t>(oa1);
            CHECK_TRUE(obj_a_has_cp_b);
            bool obj_a_has_cp_c = poola.has_component<ncs::object_a_t, ncs::component_c_t>(oa1);
            CHECK_TRUE(obj_a_has_cp_c);
            ncs::component_a_t* cpa1 = poola.get_component<ncs::object_a_t, ncs::component_a_t>(oa1);
            CHECK_NOT_NULL(cpa1);

            poola.destroy_component<ncs::object_a_t, ncs::component_a_t>(oa1);
            poola.destroy_component<ncs::object_a_t, ncs::component_b_t>(oa1);
            poola.destroy_component<ncs::object_a_t, ncs::component_c_t>(oa1);

            poolb.destroy_component<ncs::object_b_t, ncs::component_b_t>(ob1);
            poolb.destroy_component<ncs::object_b_t, ncs::component_a_t>(ob1);

            poola.destroy_instance<ncs::object_a_t>(oa1);
            poola.destroy_instance<ncs::object_a_t>(oa2);
            poolb.destroy_instance<ncs::object_b_t>(ob1);

            poola.teardown();
            poolb.teardown();
        }
    }
}
UNITTEST_SUITE_END
