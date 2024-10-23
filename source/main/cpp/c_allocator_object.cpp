#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "cbase/c_duomap.h"

#include "callocator/c_allocator_object.h"

namespace ncore
{
    namespace nobject
    {
        // ------------------------------------------------------------------------------------------------
        // array

        void array_t::setup(alloc_t* allocator, u32 max_num_components, u32 sizeof_component)
        {
            ASSERT(sizeof_component >= sizeof(u32)); // Resource size must be at least the size of a u32 since we use it as a linked list.

            u32 const alignment = (u32)sizeof(void*);
            m_sizeof            = (sizeof_component + alignment - 1) & ~(alignment - 1);
            m_memory            = (byte*)allocator->allocate(max_num_components * m_sizeof);
            m_num_max           = max_num_components;
            m_sizeof            = sizeof_component;
        }

        void array_t::teardown(alloc_t* allocator) { allocator->deallocate(m_memory); }

        // ------------------------------------------------------------------------------------------------
        // inventory

        void inventory_t::setup(alloc_t* allocator, u32 max_num_components, u32 sizeof_component)
        {
            m_array.setup(allocator, max_num_components, sizeof_component);
            m_bitarray = (u32*)g_allocate_and_memset(allocator, ((max_num_components + 31) / 32) * sizeof(u32), 0);
        }

        void inventory_t::teardown(alloc_t* allocator)
        {
            if (m_bitarray != nullptr)
            {
                m_array.teardown(allocator);
                allocator->deallocate(m_bitarray);
            }
        }

        void inventory_t::free_all() { nmem::memset(m_bitarray, 0, ((m_array.m_num_max + 31) / 32) * sizeof(u32)); }

        // ------------------------------------------------------------------------------------------------
        // pool

        void pool_t::setup(array_t& object_array, alloc_t* allocator)
        {
            m_object_array         = object_array;
            binmap_t::config_t cfg = binmap_t::config_t::compute(object_array.m_num_max);
            m_free_resource_map.init_all_free(cfg, allocator);
        }

        void pool_t::teardown(alloc_t* allocator)
        {
            m_object_array.teardown(allocator);
            m_free_resource_map.release(allocator);
        }

        void pool_t::free_all() { m_free_resource_map.init_all_free(); }

        namespace nobjects_with_components
        {
            struct components
            {
                u32         m_free_index;
                u32         m_sizeof_component;
                byte*       m_component_data;
                s32*        m_redirect;
                binmap_t    m_occupancy;
                const char* m_name;
            };

            struct pool_t::object_t
            {
                DCORE_CLASS_PLACEMENT_NEW_DELETE
                alloc_t*    m_allocator;
                u32         m_num_objects;
                u32         m_max_objects;
                u32         m_max_components;
                u32         m_max_tags;
                u32         m_component_words_per_object;
                u32         m_tag_words_per_entity;
                u32         m_bytes_per_object;
                u32*        m_per_object_component_occupancy;
                u32*        m_per_object_tags;
                byte*       m_per_object_instance;
                components* m_components;
                duomap_t    m_entity_state;
            };
            typedef pool_t::object_t object_t;

            static void s_teardown(alloc_t* allocator, components* container)
            {
                allocator->deallocate(container->m_component_data);
                allocator->deallocate(container->m_redirect);
                container->m_occupancy.release(allocator);
                container->m_free_index       = 0;
                container->m_sizeof_component = 0;
            }

            static object_t* g_create_object(alloc_t* allocator, u32 sizeof_object, u32 max_objects, u32 max_components, u32 max_tags)
            {
                object_t* object = allocator->construct<object_t>();

                object->m_allocator                  = allocator;
                object->m_num_objects                = 0;
                object->m_max_objects                = max_objects;
                object->m_max_components             = max_components;
                object->m_max_tags                   = max_tags;
                object->m_component_words_per_object = (max_components + 31) >> 5;
                object->m_tag_words_per_entity       = (max_tags + 31) >> 5;

                object->m_per_object_component_occupancy = g_allocate_array_and_memset<u32>(allocator, max_objects * object->m_component_words_per_object, 0);
                object->m_per_object_tags                = g_allocate_array_and_memset<u32>(allocator, max_objects * object->m_tag_words_per_entity, 0);

                object->m_components = g_allocate_array_and_memset<components>(allocator, max_components, 0);

                object->m_bytes_per_object    = sizeof_object;
                object->m_per_object_instance = g_allocate_array<byte>(allocator, max_objects * sizeof_object);

                duomap_t::config_t cfg = duomap_t::config_t::compute(max_objects);
                object->m_entity_state.init_all_free(cfg, allocator);

                return object;
            }

            static void g_destroy_object(object_t* object)
            {
                alloc_t* allocator = object->m_allocator;

                for (u32 i = 0; i < object->m_max_components; ++i)
                {
                    components* container = &object->m_components[i];
                    if (container->m_sizeof_component > 0)
                        s_teardown(allocator, container);
                }
                allocator->deallocate(object->m_components);
                allocator->deallocate(object->m_per_object_tags);
                allocator->deallocate(object->m_per_object_component_occupancy);
                allocator->deallocate(object->m_per_object_instance);

                object->m_entity_state.release(allocator);

                allocator->deallocate(object);
            }

            bool pool_t::setup(alloc_t* allocator, u32 max_object_types)
            {
                if (m_objects != nullptr)
                    return false;
                m_allocator        = allocator;
                m_objects          = g_allocate_array_and_memset<object_t*>(allocator, max_object_types, 0);
                m_max_object_types = max_object_types;
                return true;
            }

            void pool_t::teardown()
            {
                if (m_objects == nullptr)
                    return;

                for (u32 i = 0; i < m_max_object_types; ++i)
                {
                    if (m_objects[i] != nullptr)
                        g_destroy_object(m_objects[i]);
                }
                m_allocator->deallocate(m_objects);
                m_objects          = nullptr;
                m_max_object_types = 0;
            }

            static s32 g_create_instance(object_t* object)
            {
                s32 const index = object->m_entity_state.find_free_and_set_used();
                if (index >= 0)
                {
                    // Clear the component occupancy
                    u32 const offset = index * object->m_component_words_per_object;
                    for (u32 i = 0; i < object->m_component_words_per_object; ++i)
                        object->m_per_object_component_occupancy[offset + i] = 0;

                    // Clear the tag occupancy
                    u32 const tag_offset = index * object->m_tag_words_per_entity;
                    for (u32 i = 0; i < object->m_tag_words_per_entity; ++i)
                        object->m_per_object_tags[tag_offset + i] = 0;

                    object->m_num_objects++;
                }
                return index;
            }

            static u32 g_instance_index(object_t* object, void* object_ptr)
            {
                u32 const index = ((byte const*)object_ptr - object->m_per_object_instance) / object->m_bytes_per_object;
                return index;
            }

            static void g_destroy_instance(object_t* object, u32 instance_index)
            {
                if (object->m_entity_state.set_free(instance_index))
                    object->m_num_objects--;
            }

            static void g_register_component(object_t* object, u32 max_components, u32 cp_index, s32 cp_sizeof, s32 cp_alignof, const char* cp_name)
            {
                // See if the component container is present, if not we need to initialize it
                if (object->m_components[cp_index].m_sizeof_component == 0)
                {
                    components* container         = &object->m_components[cp_index];
                    container->m_free_index       = 0;
                    container->m_sizeof_component = cp_sizeof;
                    container->m_component_data   = g_allocate_array<byte>(object->m_allocator, cp_sizeof * max_components);
                    container->m_redirect         = g_allocate_array_and_memset<s32>(object->m_allocator, object->m_max_objects, -1);
                    container->m_name             = cp_name;

                    binmap_t::config_t const cfg = binmap_t::config_t::compute(max_components);
                    container->m_occupancy.init_all_free_lazy(cfg, object->m_allocator);
                }
            }

            static void g_unregister_component(object_t* object, u32 cp_index)
            {
                components* container = &object->m_components[cp_index];
                if (container->m_sizeof_component > 0)
                    s_teardown(object->m_allocator, container);
            }

            static bool g_has_cp(object_t* object, u32 instance_index, u32 cp_index)
            {
                u32 const* component_occupancy = &object->m_per_object_component_occupancy[instance_index * object->m_component_words_per_object];
                return (component_occupancy[cp_index >> 5] & (1 << (cp_index & 31))) != 0;
            }

            static void* g_add_cp(object_t* object, u32 instance_index, u32 cp_index)
            {
                if (cp_index >= object->m_max_components)
                    return nullptr;

                components* container = &object->m_components[cp_index];
                if (container->m_sizeof_component == 0)
                    return nullptr;

                if (container->m_redirect[instance_index] < 0)
                {
                    s32 local_component_index = container->m_occupancy.find();
                    if (local_component_index == -1)
                    {
                        if (container->m_free_index >= container->m_occupancy.size())
                            return nullptr;
                        container->m_occupancy.tick_all_free_lazy(container->m_free_index);
                        local_component_index = container->m_free_index++;
                    }
                    container->m_occupancy.set_used(local_component_index);

                    container->m_redirect[instance_index] = local_component_index;
                    u32* component_occupancy              = &object->m_per_object_component_occupancy[instance_index * object->m_component_words_per_object];
                    component_occupancy[cp_index >> 5] |= (1 << (cp_index & 31));
                    return &container->m_component_data[local_component_index * container->m_sizeof_component];
                }
                else
                {
                    u32 const local_component_index = container->m_redirect[instance_index];
                    return &container->m_component_data[local_component_index * container->m_sizeof_component];
                }
            }

            static void g_rem_cp(object_t* object, u32 instance_index, u32 cp_index)
            {
                if (cp_index >= object->m_max_components)
                    return;
                components* container = &object->m_components[cp_index];
                if (container->m_sizeof_component == 0)
                    return;
                if (container->m_redirect[instance_index] >= 0)
                {
                    s32 const local_component_index       = container->m_redirect[instance_index];
                    container->m_redirect[instance_index] = -1;
                    container->m_occupancy.set_free(local_component_index);

                    u32* component_occupancy = &object->m_per_object_component_occupancy[instance_index * object->m_component_words_per_object];
                    component_occupancy[cp_index >> 5] &= ~(1 << (cp_index & 31));
                }
            }

            static void* g_get_cp(object_t* object, u32 instance_index, u32 cp_index)
            {
                if (cp_index >= object->m_max_components)
                    return nullptr;

                components* container = &object->m_components[cp_index];
                if (container->m_sizeof_component == 0)
                    return nullptr;

                u32 const entity_index = instance_index;
                if (container->m_redirect[entity_index] >= 0)
                    return &container->m_component_data[container->m_redirect[entity_index] * container->m_sizeof_component];
                return nullptr;
            }

            static bool g_has_tag(object_t* object, u32 instance_index, s16 tg_index)
            {
                if (tg_index >= object->m_max_tags)
                    return false;
                u32 const* tag_occupancy = &object->m_per_object_tags[instance_index * object->m_tag_words_per_entity];
                return (tag_occupancy[tg_index >> 5] & (1 << (tg_index & 31))) != 0;
            }

            static void g_add_tag(object_t* object, u32 instance_index, s16 tg_index)
            {
                if (tg_index >= object->m_max_tags)
                    return;
                u32* tag_occupancy = &object->m_per_object_tags[instance_index * object->m_tag_words_per_entity];
                tag_occupancy[tg_index >> 5] |= (1 << (tg_index & 31));
            }

            static void g_rem_tag(object_t* object, u32 instance_index, s16 tg_index)
            {
                if (tg_index >= object->m_max_tags)
                    return;
                u32* tag_occupancy = &object->m_per_object_tags[instance_index * object->m_tag_words_per_entity];
                tag_occupancy[tg_index >> 5] &= ~(1 << (tg_index & 31));
            }

            bool pool_t::register_object(u32 object_index, u32 sizeof_object, u32 max_object_instances, u32 max_components, u32 max_tags)
            {
                if (object_index < m_max_object_types && m_objects[object_index] == nullptr)
                {
                    object_t* object        = g_create_object(m_allocator, sizeof_object, max_object_instances, max_components, max_tags);
                    m_objects[object_index] = object;
                    return true;
                }
                return false;
            }

            void* pool_t::create_object(s16 index)
            {
                if (index < m_max_object_types)
                {
                    object_t* object = m_objects[index];
                    if (object == nullptr)
                        return nullptr;
                    s32 instance_index = g_create_instance(object);
                    return instance_index >= 0 ? &object->m_per_object_instance[instance_index * object->m_bytes_per_object] : nullptr;
                }
                return nullptr;
            }

            void pool_t::destroy_object(s16 index, void* object_ptr)
            {
                if (index < m_max_object_types && object_ptr != nullptr)
                {
                    object_t* object = m_objects[index];
                    if (object == nullptr)
                        return;
                    u32 const entity_index = g_instance_index(object, object_ptr);
                    g_destroy_instance(object, entity_index);
                }
            }

            u32 pool_t::get_number_of_instances(s16 cp_index) const
            {
                object_t* object = m_objects[cp_index];
                if (object == nullptr)
                    return 0;
                return object->m_num_objects;
            }

            bool pool_t::register_component(u32 object_index, u32 max_components, u32 cp_index, s32 cp_sizeof, s32 cp_alignof, const char* cp_name)
            {
                object_t* object = (object_index < m_max_object_types ) ? m_objects[object_index] : nullptr;
                if (object == nullptr)
                    return false;
                g_register_component(object, max_components, cp_index, cp_sizeof, cp_alignof, cp_name);
                return true;
            }

            bool pool_t::is_component_registered(u32 object_index, u32 cp_index) const
            {
                object_t* object = (object_index < m_max_object_types ) ? m_objects[object_index] : nullptr;
                if (object == nullptr)
                    return false;
                return object->m_components[cp_index].m_sizeof_component > 0;
            }

            bool pool_t::has_cp(u32 object_index, void* object_ptr, u32 cp_index) const
            {
                object_t* object = (object_index < m_max_object_types ) ? m_objects[object_index] : nullptr;
                if (object == nullptr)
                    return false;
                u32 const instance_index = g_instance_index(object, object_ptr);
                return g_has_cp(object, instance_index, cp_index);
            }

            void* pool_t::add_cp(u32 object_index, void* object_ptr, u32 cp_index)
            {
                object_t* object = (object_index < m_max_object_types ) ? m_objects[object_index] : nullptr;
                if (object == nullptr)
                    return nullptr;
                u32 const instance_index = g_instance_index(object, object_ptr);
                return g_add_cp(object, instance_index, cp_index);
            }

            void pool_t::rem_cp(u32 object_index, void* object_ptr, u32 cp_index)
            {
                object_t* object = (object_index < m_max_object_types ) ? m_objects[object_index] : nullptr;
                if (object == nullptr)
                    return;
                u32 const instance_index = g_instance_index(object, object_ptr);
                g_rem_cp(object, instance_index, cp_index);
            }

            void* pool_t::get_cp(u32 object_index, void* object_ptr, u32 cp_index)
            {
                object_t* object = (object_index < m_max_object_types ) ? m_objects[object_index] : nullptr;
                if (object == nullptr)
                    return nullptr;
                u32 const instance_index = g_instance_index(object, object_ptr);
                return g_get_cp(object, instance_index, cp_index);
            }

            bool pool_t::has_tag(u32 object_index, void* object_ptr, s16 tg_index) const
            {
                object_t* object = (object_index < m_max_object_types ) ? m_objects[object_index] : nullptr;
                if (object == nullptr)
                    return false;
                u32 const instance_index = g_instance_index(object, object_ptr);
                return g_has_tag(object, instance_index, tg_index);
            }

            void pool_t::add_tag(u32 object_index, void* object_ptr, s16 tg_index)
            {
                object_t* object = (object_index < m_max_object_types ) ? m_objects[object_index] : nullptr;
                if (object == nullptr)
                    return;
                u32 const instance_index = g_instance_index(object, object_ptr);
                g_add_tag(object, instance_index, tg_index);
            }

            void pool_t::rem_tag(u32 object_index, void* object_ptr, s16 tg_index)
            {
                object_t* object = (object_index < m_max_object_types ) ? m_objects[object_index] : nullptr;
                if (object == nullptr)
                    return;
                u32 const instance_index = g_instance_index(object, object_ptr);
                g_rem_tag(object, instance_index, tg_index);
            }

        } // namespace nobjects_with_components
    } // namespace nobject
} // namespace ncore
