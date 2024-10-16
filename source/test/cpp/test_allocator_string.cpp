#include "ccore/c_allocator.h"
#include "cbase/c_runes.h"

#include "callocator/c_allocator_string.h"

#include "cunittest/cunittest.h"

using namespace ncore;

UNITTEST_SUITE_BEGIN(nstring)
{
    UNITTEST_FIXTURE(utf8)
    {
        UNITTEST_ALLOCATOR;

        UNITTEST_FIXTURE_SETUP() {}
        UNITTEST_FIXTURE_TEARDOWN() {}

        UNITTEST_TEST(setup_teardown)
        {
            void* mem = Allocator->allocate(1024 * 1024);
            nstring::storage_t* storage = nstring::g_create_storage_utf8(mem, 1024 * 1024);

            nstring::g_destroy_storage(storage);
            Allocator->deallocate(mem);
        }

        UNITTEST_TEST(put_get)
        {
            void* mem = Allocator->allocate(1024 * 1024);
            nstring::storage_t* storage = nstring::g_create_storage_utf8(mem, 1024 * 1024);

            crunes_t str = make_crunes("Hello, World!", 0, 13, 13);
            nstring::id_t id = storage->put(str);
            CHECK_EQUAL(0, id);
            crunes_t str2 = storage->get(id);
            CHECK_EQUAL(0, nrunes::compare(str, str2));

            nstring::g_destroy_storage(storage);
            Allocator->deallocate(mem);
        }

        UNITTEST_TEST(put_get_multiple)
        {
            void* mem = Allocator->allocate(1024 * 1024);
            nstring::storage_t* storage = nstring::g_create_storage_utf8(mem, 1024 * 1024);

            crunes_t str = make_crunes("Hello, World!", 0, 13, 13);
            nstring::id_t id = storage->put(str);
            CHECK_EQUAL(0, id);
            crunes_t str2 = storage->get(id);
            CHECK_EQUAL(0, nrunes::compare(str, str2));

            nstring::g_destroy_storage(storage);
            Allocator->deallocate(mem);
        }

    }
}
UNITTEST_SUITE_END
