#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "cbase/c_duomap.h"

#include "callocator/c_allocator_cs.h"

namespace ncore
{
    namespace ncs
    {
        // ------------------------------------------------------------------------------------------------
        struct component_container_t
        {
            u32      m_free_index;
            u32      m_sizeof_component;
            byte*    m_cp_data;
            u16*     m_map;       // instance index -> component index
            u16*     m_unmap;     // component index -> instance index
            binmap_t m_occupancy; // 32 bytes
        };

        struct allocator_t::object_t
        {
            DCORE_CLASS_PLACEMENT_NEW_DELETE
            alloc_t*               m_allocator;
            u32                    m_num_instances;
            u32                    m_max_instances;
            u32                    m_max_component_types;
            u32                    m_max_tag_types;
            u32                    m_component_occupancy_sizeof; // per object, u32[]
            u32                    m_tag_data_sizeof;            // per object, u32[]
            u32*                   m_per_instance_component_occupancy;
            u32*                   m_per_instance_tag_data;
            component_container_t* m_a_component;
            duomap_t               m_object_state;
        };
        typedef allocator_t::object_t object_t;

        static void s_teardown(alloc_t* allocator, component_container_t* container)
        {
            allocator->deallocate(container->m_cp_data);
            allocator->deallocate(container->m_map);
            container->m_occupancy.release(allocator);
            container->m_free_index       = 0;
            container->m_sizeof_component = 0;
        }

        static object_t* g_create_object(alloc_t* allocator, u32 sizeof_object, u32 max_instances, u32 max_components, u32 max_tags)
        {
            object_t* object = allocator->construct<object_t>();

            object->m_allocator                  = allocator;
            object->m_num_instances              = 0;
            object->m_max_instances              = max_instances;
            object->m_max_component_types        = max_components;
            object->m_max_tag_types              = max_tags;
            object->m_component_occupancy_sizeof = (max_components + 31) >> 5;
            object->m_tag_data_sizeof            = (max_tags + 31) >> 5;

            object->m_per_instance_component_occupancy = g_allocate_array_and_memset<u32>(allocator, max_instances * object->m_component_occupancy_sizeof, 0);
            object->m_per_instance_tag_data            = g_allocate_array_and_memset<u32>(allocator, max_instances * object->m_tag_data_sizeof, 0);

            object->m_a_component = g_allocate_array_and_memset<component_container_t>(allocator, max_components, 0);

            duomap_t::config_t cfg = duomap_t::config_t::compute(max_instances);
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

            allocator->deallocate(object->m_a_component);
            allocator->deallocate(object->m_per_instance_tag_data);
            allocator->deallocate(object->m_per_instance_component_occupancy);

            object->m_object_state.release(allocator);

            allocator->deallocate(object);
        }

        bool allocator_t::setup(alloc_t* allocator, u16 max_object_instances, u16 max_components, u16 max_tags)
        {
            if (m_object != nullptr)
                return false;
            m_allocator = allocator;
            m_object    = g_allocate<object_t>(allocator);
            return true;
        }

        void allocator_t::teardown()
        {
            if (m_object == nullptr)
                return;
            g_destroy_object(m_object);
            m_object = nullptr;
        }

        static s32 g_create_instance(object_t* object)
        {
            s32 const index = object->m_object_state.find_free_and_set_used();
            if (index >= 0)
            {
                // Clear the component occupancy
                u32 const offset = index * object->m_component_occupancy_sizeof;
                for (u32 i = 0; i < object->m_component_occupancy_sizeof; ++i)
                    object->m_per_instance_component_occupancy[offset + i] = 0;

                // Clear the tag occupancy
                u32 const tag_offset = index * object->m_tag_data_sizeof;
                for (u32 i = 0; i < object->m_tag_data_sizeof; ++i)
                    object->m_per_instance_tag_data[tag_offset + i] = 0;

                object->m_num_instances++;
            }
            return index;
        }

        static u32 g_instance_index(object_t const* object, u16 const component_type_index, void const* component_ptr)
        {
            component_container_t const& cptype      = object->m_a_component[component_type_index];
            u32 const                    local_index = (u32)(((u32 const*)component_ptr - (u32 const*)cptype.m_cp_data) / cptype.m_sizeof_component);
            return cptype.m_unmap[local_index];
        }

        static void g_destroy_instance(object_t* object, u32 instance_index)
        {
            if (object->m_object_state.set_free(instance_index))
                object->m_num_instances--;
        }

        static void g_register_component(object_t* object, u32 max_components, u16 cp_index, s32 cp_sizeof, s32 cp_alignof)
        {
            // See if the component container is present, if not we need to initialize it
            if (object->m_a_component[cp_index].m_sizeof_component == 0)
            {
                component_container_t* container = &object->m_a_component[cp_index];
                container->m_free_index          = 0;
                container->m_sizeof_component    = cp_sizeof;
                container->m_cp_data             = g_allocate_array<byte>(object->m_allocator, cp_sizeof * max_components);
                container->m_map                 = g_allocate_array_and_memset<u16>(object->m_allocator, object->m_max_instances, 0xFFFFFFFF);
                container->m_unmap               = g_allocate_array_and_memset<u16>(object->m_allocator, object->m_max_instances, 0xFFFFFFFF);

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
            u32 const* component_occupancy = &object->m_per_instance_component_occupancy[instance_index * object->m_component_occupancy_sizeof];
            return (component_occupancy[cp_index >> 5] & (1 << (cp_index & 31))) != 0;
        }

        static void* g_add_cp(object_t* object, u32 instance_index, u16 cp_index)
        {
            if (cp_index >= object->m_max_component_types)
                return nullptr;

            component_container_t* container = &object->m_a_component[cp_index];
            if (container->m_sizeof_component == 0)
                return nullptr;

            if (container->m_map[instance_index] != 0xFFFF)
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

                // map and unmap
                container->m_map[instance_index]          = (u16)local_component_index;
                container->m_unmap[local_component_index] = (u16)instance_index;

                u32* component_occupancy = &object->m_per_instance_component_occupancy[instance_index * object->m_component_occupancy_sizeof];
                component_occupancy[cp_index >> 5] |= (1 << (cp_index & 31));
                return &container->m_cp_data[local_component_index * container->m_sizeof_component];
            }
            else
            {
                u32 const local_component_index = container->m_map[instance_index];
                return &container->m_cp_data[local_component_index * container->m_sizeof_component];
            }
        }

        static void* g_rem_cp(object_t* object, u32 instance_index, u16 cp_index)
        {
            if (cp_index >= object->m_max_component_types)
                return nullptr;

            component_container_t* container = &object->m_a_component[cp_index];
            if (container->m_sizeof_component == 0)
                return nullptr;

            if (container->m_map[instance_index] != 0xFFFF)
            {
                s32 const local_component_index           = container->m_map[instance_index];
                container->m_map[instance_index]          = 0xFFFF;
                container->m_unmap[local_component_index] = 0xFFFF;
                container->m_occupancy.set_free(local_component_index);

                u32* component_occupancy = &object->m_per_instance_component_occupancy[instance_index * object->m_component_occupancy_sizeof];
                component_occupancy[cp_index >> 5] &= ~(1 << (cp_index & 31));
                return &container->m_cp_data[local_component_index * container->m_sizeof_component];
            }
            return nullptr;
        }

        static void* g_get_cp(object_t* object, u16 instance_index, u16 cp_index)
        {
            if (cp_index >= object->m_max_component_types)
                return nullptr;

            component_container_t* container = &object->m_a_component[cp_index];
            if (container->m_sizeof_component == 0)
                return nullptr;

            s32 const local_component_index = container->m_map[instance_index];
            if (local_component_index != 0xFFFF)
                return &container->m_cp_data[local_component_index * container->m_sizeof_component];
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
            if (container->m_map[entity_index] != 0xFFFF)
                return &container->m_cp_data[container->m_map[entity_index] * container->m_sizeof_component];
            return nullptr;
        }

        static bool g_has_tag(object_t* object, u16 instance_index, u16 tg_index)
        {
            if (tg_index >= object->m_max_tag_types)
                return false;
            u32 const* tag_occupancy = &object->m_per_instance_tag_data[instance_index * object->m_tag_data_sizeof];
            return (tag_occupancy[tg_index >> 5] & (1 << (tg_index & 31))) != 0;
        }

        static void g_add_tag(object_t* object, u16 instance_index, u16 tg_index)
        {
            if (tg_index >= object->m_max_tag_types)
                return;
            u32* tag_occupancy = &object->m_per_instance_tag_data[instance_index * object->m_tag_data_sizeof];
            tag_occupancy[tg_index >> 5] |= (1 << (tg_index & 31));
        }

        static void g_rem_tag(object_t* object, u16 instance_index, u16 tg_index)
        {
            if (tg_index >= object->m_max_tag_types)
                return;
            u32* tag_occupancy = &object->m_per_instance_tag_data[instance_index * object->m_tag_data_sizeof];
            tag_occupancy[tg_index >> 5] &= ~(1 << (tg_index & 31));
        }

        void* allocator_t::create_instance(u16 cp_index)
        {
            if (cp_index < m_object->m_max_component_types)
            {
                s32 const instance_index = g_create_instance(m_object);
                return g_add_cp(m_object, instance_index, cp_index);
            }
            return nullptr;
        }

        void allocator_t::destroy_instance(u16 cp_index, void* cp_ptr)
        {
            if (cp_index < m_object->m_max_component_types && cp_ptr != nullptr)
            {
                u32 const instance_index = g_instance_index(m_object, cp_index, cp_ptr);
                g_destroy_instance(m_object, instance_index);
            }
        }

        u16 allocator_t::get_number_of_instances(u16 cp_index) const { return m_object->m_num_instances; }

        bool allocator_t::register_component(u16 max_components, u16 cp_index, u32 cp_sizeof, u32 cp_alignof)
        {
            object_t* object = (cp_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return false;
            g_register_component(object, max_components, cp_index, cp_sizeof, cp_alignof);
            return true;
        }

        bool allocator_t::is_component_registered(u16 cp_index) const
        {
            object_t* object = (cp_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return false;
            return object->m_a_component[cp_index].m_sizeof_component > 0;
        }

        bool allocator_t::has_cp(u16 cp_index, void const* cp_ptr, u16 cp1_index) const
        {
            object_t* object = (cp_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return false;
            u32 const instance_index = g_instance_index(object, cp_index, cp_ptr);
            return g_has_cp(object, instance_index, cp_index);
        }

        void* allocator_t::add_cp(u16 cp1_index, void const* cp1, u16 cp2_index)
        {
            object_t* object = (cp1_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return nullptr;
            u32 const instance_index = g_instance_index(object, cp1_index, cp1);
            return g_add_cp(object, instance_index, cp2_index);
        }

        void* allocator_t::rem_cp(u16 cp1_index, void const* cp1, u16 cp2_index)
        {
            object_t* object = (cp1_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return nullptr;
            u32 const instance_index = g_instance_index(object, cp1_index, cp1);
            return g_rem_cp(object, instance_index, cp2_index);
        }

        void* allocator_t::get_cp(u16 cp1_index, void* cp1_ptr, u16 cp2_index)
        {
            object_t* object = (cp1_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return nullptr;

            u16 const instance_index = g_instance_index(object, cp1_index, cp1_ptr);
            return g_get_cp(object, instance_index, cp2_index);
        }

        void const* allocator_t::get_cp(u16 cp1_index, void const* cp1_ptr, u16 cp2_index) const
        {
            object_t const* object = (cp1_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return nullptr;

            u16 const instance_index = g_instance_index(object, cp1_index, cp1_ptr);
            return g_get_cp(object, instance_index, cp2_index);
        }

        bool allocator_t::has_tag(u16 cp_index, void const* cp_ptr, u16 tg_index) const
        {
            object_t* object = (cp_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return false;

            if (cp_ptr != nullptr)
            {
                u32 const instance_index = g_instance_index(object, cp_index, cp_ptr);
                return !g_has_tag(object, instance_index, tg_index);
            }

            return false;
        }

        void allocator_t::add_tag(u16 cp_index, void const* cp_ptr, u16 tg_index)
        {
            object_t* object = (cp_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return;

            if (cp_ptr != nullptr)
            {
                u32 const instance_index = g_instance_index(object, cp_index, cp_ptr);
                g_add_tag(object, instance_index, tg_index);
            }
        }

        void allocator_t::rem_tag(u16 cp_index, void const* cp_ptr, u16 tg_index)
        {
            object_t* object = (cp_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return;

            if (cp_ptr != nullptr)
            {
                u32 const instance_index = g_instance_index(object, cp_index, cp_ptr);
                g_rem_tag(object, instance_index, tg_index);
            }
        }

        void* allocator_t::iterate_begin(u16 cp_index) const
        {
            object_t* object         = (cp_index < m_object->m_max_component_types) ? m_object : nullptr;
            s32 const instance_index = (object != nullptr) ? object->m_object_state.find_used() : -1;
            if (instance_index >= 0)
            {
                return g_get_cp(object, instance_index, cp_index);
            }
            return nullptr;
        }

        void* allocator_t::iterate_next(u16 cp_index, void const* cp_ptr) const
        {
            object_t* object = (cp_index < m_object->m_max_component_types) ? m_object : nullptr;
            if (object == nullptr)
                return nullptr;
            u32 const instance_index = g_instance_index(object, cp_index, cp_ptr);
            s32 const next_index     = object->m_object_state.next_used_up(instance_index + 1);
            if (next_index >= 0)
            {
                return g_get_cp(object, next_index, cp_index);
            }
            return nullptr;
        }

    } // namespace ncs
} // namespace ncore
