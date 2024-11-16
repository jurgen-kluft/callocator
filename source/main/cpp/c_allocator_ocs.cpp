#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "cbase/c_duomap.h"

#include "callocator/c_allocator_ocs.h"

namespace ncore
{
    namespace nocs
    {
        // ------------------------------------------------------------------------------------------------
        struct component_container_t
        {
            u32      m_free_index;
            u32      m_sizeof_component;
            byte*    m_component_data;
            s32*     m_redirect;
            binmap_t m_occupancy; // 32 bytes
        };

        struct allocator_t::object_t
        {
            DCORE_CLASS_PLACEMENT_NEW_DELETE
            alloc_t*               m_allocator;
            u32                    m_num_objects;
            u32                    m_max_objects;
            u32                    m_max_component_types;
            u32                    m_max_tag_types;
            u32                    m_component_occupancy_sizeof; // per object, u32[]
            u32                    m_tag_data_sizeof;            // per object, u32[]
            u32                    m_instance_data_sizeof;       // per object, u32[]
            u32*                   m_per_object_component_occupancy;
            u32*                   m_per_object_tag_data;
            u32*                   m_per_object_instance_data;
            component_container_t* m_a_component;
            const char**           m_a_component_name; // Debug only ?
            duomap_t               m_object_state;
        };
        typedef allocator_t::object_t object_t;

        static void s_teardown(alloc_t* allocator, component_container_t* container)
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
            object->m_max_component_types        = max_components;
            object->m_max_tag_types              = max_tags;
            object->m_component_occupancy_sizeof = (max_components + 31) >> 5;
            object->m_tag_data_sizeof            = (max_tags + 31) >> 5;

            object->m_per_object_component_occupancy = g_allocate_array_and_memset<u32>(allocator, max_objects * object->m_component_occupancy_sizeof, 0);
            object->m_per_object_tag_data            = g_allocate_array_and_memset<u32>(allocator, max_objects * object->m_tag_data_sizeof, 0);

            object->m_a_component      = g_allocate_array_and_memset<component_container_t>(allocator, max_components, 0);
            object->m_a_component_name = g_allocate_array_and_memset<const char*>(allocator, max_components, 0);

            object->m_instance_data_sizeof     = (sizeof_object + 3) >> 2;
            object->m_per_object_instance_data = g_allocate_array<u32>(allocator, max_objects * object->m_instance_data_sizeof);

            duomap_t::config_t cfg = duomap_t::config_t::compute(max_objects);
            object->m_object_state.init_all_free(cfg, allocator);

            return object;
        }

        static void g_destroy_object(object_t* object)
        {
            alloc_t* allocator = object->m_allocator;

            for (u32 i = 0; i < object->m_max_component_types; ++i)
            {
                component_container_t* container = &object->m_a_component[i];
                if (container->m_sizeof_component > 0)
                    s_teardown(allocator, container);
            }

            allocator->deallocate(object->m_a_component_name);
            allocator->deallocate(object->m_a_component);
            allocator->deallocate(object->m_per_object_tag_data);
            allocator->deallocate(object->m_per_object_component_occupancy);
            allocator->deallocate(object->m_per_object_instance_data);

            object->m_object_state.release(allocator);

            allocator->deallocate(object);
        }

        bool allocator_t::setup(alloc_t* allocator, u16 max_object_types)
        {
            if (m_objects != nullptr)
                return false;
            m_allocator        = allocator;
            m_objects          = g_allocate_array_and_memset<object_t*>(allocator, max_object_types, 0);
            m_max_object_types = max_object_types;
            return true;
        }

        void allocator_t::teardown()
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
            s32 const index = object->m_object_state.find_free_and_set_used();
            if (index >= 0)
            {
                // Clear the component occupancy
                u32 const offset = index * object->m_component_occupancy_sizeof;
                for (u32 i = 0; i < object->m_component_occupancy_sizeof; ++i)
                    object->m_per_object_component_occupancy[offset + i] = 0;

                // Clear the tag occupancy
                u32 const tag_offset = index * object->m_tag_data_sizeof;
                for (u32 i = 0; i < object->m_tag_data_sizeof; ++i)
                    object->m_per_object_tag_data[tag_offset + i] = 0;

                object->m_num_objects++;
            }
            return index;
        }

        static u32 g_instance_index(object_t const* object, void const* object_ptr)
        {
            u32 const index = (u32)(((u32 const*)object_ptr - (u32 const*)object->m_per_object_instance_data) / object->m_instance_data_sizeof);
            return index;
        }

        static u32 g_instance_index(object_t const* object, u16 const component_index, void const* component_ptr)
        {
            u32 const index = (u32)(((u32 const*)component_ptr - (u32 const*)object->m_a_component[component_index].m_component_data) / object->m_a_component[component_index].m_sizeof_component);
            return index;
        }

        static void g_destroy_instance(object_t* object, u32 instance_index)
        {
            if (object->m_object_state.set_free(instance_index))
                object->m_num_objects--;
        }

        static void g_register_component(object_t* object, u32 max_components, u16 cp_index, s32 cp_sizeof, s32 cp_alignof, const char* cp_name)
        {
            // See if the component container is present, if not we need to initialize it
            if (object->m_a_component[cp_index].m_sizeof_component == 0)
            {
                component_container_t* container = &object->m_a_component[cp_index];
                container->m_free_index          = 0;
                container->m_sizeof_component    = cp_sizeof;
                container->m_component_data      = g_allocate_array<byte>(object->m_allocator, cp_sizeof * max_components);
                container->m_redirect            = g_allocate_array_and_memset<s32>(object->m_allocator, object->m_max_objects, -1);

                if (object->m_a_component_name != nullptr)
                    object->m_a_component_name[cp_index] = cp_name;

                binmap_t::config_t const cfg = binmap_t::config_t::compute(max_components);
                container->m_occupancy.init_all_free_lazy(cfg, object->m_allocator);
            }
        }

        static void g_unregister_component(object_t* object, u16 cp_index)
        {
            component_container_t* container = &object->m_a_component[cp_index];
            if (container->m_sizeof_component > 0)
                s_teardown(object->m_allocator, container);
        }

        static bool g_has_cp(object_t* object, u32 instance_index, u16 cp_index)
        {
            u32 const* component_occupancy = &object->m_per_object_component_occupancy[instance_index * object->m_component_occupancy_sizeof];
            return (component_occupancy[cp_index >> 5] & (1 << (cp_index & 31))) != 0;
        }

        static void* g_add_cp(object_t* object, u32 instance_index, u16 cp_index)
        {
            if (cp_index >= object->m_max_component_types)
                return nullptr;

            component_container_t* container = &object->m_a_component[cp_index];
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
                u32* component_occupancy              = &object->m_per_object_component_occupancy[instance_index * object->m_component_occupancy_sizeof];
                component_occupancy[cp_index >> 5] |= (1 << (cp_index & 31));
                return &container->m_component_data[local_component_index * container->m_sizeof_component];
            }
            else
            {
                u32 const local_component_index = container->m_redirect[instance_index];
                return &container->m_component_data[local_component_index * container->m_sizeof_component];
            }
        }

        static void* g_rem_cp(object_t* object, u32 instance_index, u16 cp_index)
        {
            if (cp_index >= object->m_max_component_types)
                return nullptr;

            component_container_t* container = &object->m_a_component[cp_index];
            if (container->m_sizeof_component == 0)
                return nullptr;

            if (container->m_redirect[instance_index] >= 0)
            {
                s32 const local_component_index       = container->m_redirect[instance_index];
                container->m_redirect[instance_index] = -1;
                container->m_occupancy.set_free(local_component_index);

                u32* component_occupancy = &object->m_per_object_component_occupancy[instance_index * object->m_component_occupancy_sizeof];
                component_occupancy[cp_index >> 5] &= ~(1 << (cp_index & 31));
                return &container->m_component_data[local_component_index * container->m_sizeof_component];
            }
        }

        static void* g_get_cp(object_t* object, u16 instance_index, u16 cp_index)
        {
            if (cp_index >= object->m_max_component_types)
                return nullptr;

            component_container_t* container = &object->m_a_component[cp_index];
            if (container->m_sizeof_component == 0)
                return nullptr;

            s32 const local_component_index = container->m_redirect[instance_index];
            if (local_component_index >= 0)
                return &container->m_component_data[local_component_index * container->m_sizeof_component];
            return nullptr;
        }

        static void const* g_get_cp(object_t const* object, u16 instance_index, u16 cp_index)
        {
            if (cp_index >= object->m_max_component_types)
                return nullptr;

            component_container_t* container = &object->m_a_component[cp_index];
            if (container->m_sizeof_component == 0)
                return nullptr;

            u32 const entity_index = instance_index;
            if (container->m_redirect[entity_index] >= 0)
                return &container->m_component_data[container->m_redirect[entity_index] * container->m_sizeof_component];
            return nullptr;
        }

        static bool g_has_tag(object_t* object, u16 instance_index, u16 tg_index)
        {
            if (tg_index >= object->m_max_tag_types)
                return false;
            u32 const* tag_occupancy = &object->m_per_object_tag_data[instance_index * object->m_tag_data_sizeof];
            return (tag_occupancy[tg_index >> 5] & (1 << (tg_index & 31))) != 0;
        }

        static void g_add_tag(object_t* object, u16 instance_index, u16 tg_index)
        {
            if (tg_index >= object->m_max_tag_types)
                return;
            u32* tag_occupancy = &object->m_per_object_tag_data[instance_index * object->m_tag_data_sizeof];
            tag_occupancy[tg_index >> 5] |= (1 << (tg_index & 31));
        }

        static void g_rem_tag(object_t* object, u16 instance_index, u16 tg_index)
        {
            if (tg_index >= object->m_max_tag_types)
                return;
            u32* tag_occupancy = &object->m_per_object_tag_data[instance_index * object->m_tag_data_sizeof];
            tag_occupancy[tg_index >> 5] &= ~(1 << (tg_index & 31));
        }

        bool allocator_t::register_object(u16 object_index, u32 sizeof_object, u16 max_object_instances, u16 max_components, u16 max_tags)
        {
            if (object_index < m_max_object_types && m_objects[object_index] == nullptr)
            {
                object_t* object        = g_create_object(m_allocator, sizeof_object, max_object_instances, max_components, max_tags);
                m_objects[object_index] = object;
                return true;
            }
            return false;
        }

        void* allocator_t::create_object(u16 index)
        {
            if (index < m_max_object_types)
            {
                object_t* object = m_objects[index];
                if (object == nullptr)
                    return nullptr;
                s32 instance_index = g_create_instance(object);
                return instance_index >= 0 ? &object->m_per_object_instance_data[instance_index * object->m_instance_data_sizeof] : nullptr;
            }
            return nullptr;
        }

        void allocator_t::destroy_object(u16 index, void* object_ptr)
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

        u16 allocator_t::get_number_of_instances(u16 cp_index) const
        {
            object_t* object = m_objects[cp_index];
            if (object == nullptr)
                return 0;
            return object->m_num_objects;
        }

        bool allocator_t::register_component(u16 object_index, u16 max_components, u16 cp_index, u32 cp_sizeof, u32 cp_alignof, const char* cp_name)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return false;
            g_register_component(object, max_components, cp_index, cp_sizeof, cp_alignof, cp_name);
            return true;
        }

        bool allocator_t::is_component_registered(u16 object_index, u16 cp_index) const
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return false;
            return object->m_a_component[cp_index].m_sizeof_component > 0;
        }

        void* allocator_t::get_object(u16 object_index, u16 component_index, void const* component_ptr)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return nullptr;
            u32 const instance_index = g_instance_index(object, component_index, component_ptr);
            return &object->m_per_object_instance_data[instance_index * object->m_instance_data_sizeof];
        }

        bool allocator_t::has_cp(u16 object_index, void const* object_ptr, u16 cp_index) const
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return false;
            u32 const instance_index = g_instance_index(object, object_ptr);
            return g_has_cp(object, instance_index, cp_index);
        }

        bool allocator_t::has_cp(u16 object_index, u16 cp1_index, void const* cp1, u16 cp2_index) const
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return false;
            u32 const instance_index = g_instance_index(object, cp1_index, cp1);
            return g_has_cp(object, instance_index, cp2_index);
        }

        void* allocator_t::add_cp(u16 object_index, void const* object_ptr, u16 cp_index)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return nullptr;
            u32 const instance_index = g_instance_index(object, object_ptr);
            return g_add_cp(object, instance_index, cp_index);
        }

        void* allocator_t::add_cp(u16 object_index, u16 cp1_index, void const* cp1, u16 cp2_index)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return nullptr;
            u32 const instance_index = g_instance_index(object, cp1_index, cp1);
            return g_add_cp(object, instance_index, cp2_index);
        }

        void* allocator_t::rem_cp(u16 object_index, void const* object_ptr, u16 cp_index)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return;
            u32 const instance_index = g_instance_index(object, object_ptr);
            return g_rem_cp(object, instance_index, cp_index);
        }

        void* allocator_t::rem_cp(u16 object_index, u16 cp1_index, void const* cp1, u16 cp2_index)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return;
            u32 const instance_index = g_instance_index(object, cp1_index, cp1);
            return g_rem_cp(object, instance_index, cp2_index);
        }

        void* allocator_t::get_cp(u16 object_index, void* object_ptr, u16 cp_index)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return nullptr;
            u32 const instance_index = g_instance_index(object, object_ptr);
            return g_get_cp(object, instance_index, cp_index);
        }

        void const* allocator_t::get_cp(u16 object_index, void const* object_ptr, u16 cp_index) const
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return nullptr;
            u32 const instance_index = g_instance_index(object, object_ptr);
            return g_get_cp(object, instance_index, cp_index);
        }

        void* allocator_t::get_cp(u16 object_index, u16 cp1_index, void* cp1_ptr, u16 cp2_index)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return nullptr;

            u16 const instance_index = g_instance_index(object, cp1_index, cp1_ptr);
            return g_get_cp(object, instance_index, cp2_index);
        }

        void const* allocator_t::get_cp(u16 object_index, u16 cp1_index, void const* cp1_ptr, u16 cp2_index) const
        {
            object_t const* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return nullptr;

            u16 const instance_index = g_instance_index(object, cp1_index, cp1_ptr);
            return g_get_cp(object, instance_index, cp2_index);
        }

        const char* allocator_t::get_component_name(u16 cp_index) const
        {
            object_t* object = (cp_index < m_max_object_types) ? m_objects[cp_index] : nullptr;
            if (object != nullptr && object->m_a_component_name != nullptr)
                return object->m_a_component_name[cp_index];
            return "";
        }

        bool allocator_t::has_tag(u16 object_index, void const* object_ptr, u16 component_index, void const* component_ptr, u16 tg_index) const
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return false;

            if (object_ptr != nullptr)
            {
                u32 const instance_index = g_instance_index(object, object_ptr);
                return !g_has_tag(object, instance_index, tg_index);
            }
            else if (component_ptr != nullptr && component_index < object->m_max_component_types)
            {
                u32 const instance_index = g_instance_index(object, component_index, component_ptr);
                return !g_has_tag(object, instance_index, tg_index);
            }
            return false;
        }

        void allocator_t::add_tag(u16 object_index, void const* object_ptr, u16 component_index, void const* component_ptr, u16 tg_index)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return;

            if (object_ptr != nullptr)
            {
                u32 const instance_index = g_instance_index(object, object_ptr);
                g_add_tag(object, instance_index, tg_index);
            }
            else if (component_ptr != nullptr && component_index < object->m_max_component_types)
            {
                u32 const instance_index = g_instance_index(object, component_index, component_ptr);
                g_add_tag(object, instance_index, tg_index);
            }
        }

        void allocator_t::rem_tag(u16 object_index, void const* object_ptr, u16 component_index, void const* component_ptr, u16 tg_index)
        {
            object_t* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return;

            if (object_ptr != nullptr)
            {
                u32 const instance_index = g_instance_index(object, object_ptr);
                g_rem_tag(object, instance_index, tg_index);
            }
            else if (component_ptr != nullptr && component_index < object->m_max_component_types)
            {
                u32 const instance_index = g_instance_index(object, component_index, component_ptr);
                g_rem_tag(object, instance_index, tg_index);
            }
        }

        void* allocator_t::iterate_objects_begin(u16 object_index) const
        {
            object_t const* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            s32 const       index  = (object != nullptr) ? object->m_object_state.find_used() : -1;
            return (index >= 0) ? &object->m_per_object_instance_data[index * object->m_instance_data_sizeof] : nullptr;
        }

        void* allocator_t::iterate_objects_next(u16 object_index, void const* object_ptr) const
        {
            object_t const* object = (object_index < m_max_object_types) ? m_objects[object_index] : nullptr;
            if (object == nullptr)
                return nullptr;
            u32 const instance_index = g_instance_index(object, object_ptr);
            s32 const next_index     = object->m_object_state.next_used_up(instance_index + 1);
            return (next_index >= 0) ? &object->m_per_object_instance_data[next_index * object->m_instance_data_sizeof] : nullptr;
        }

    } // namespace nocs
} // namespace ncore
