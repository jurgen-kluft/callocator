#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "cbase/c_duomap.h"

#include "callocator/c_allocator_cs.h"

namespace ncore
{
    namespace ncs
    {
        // ------------------------------------------------------------------------------------------------
        typedef u32       index_t;
        constexpr index_t c_null_index = 0xFFFFFFFF;

        struct component_type_t
        {
            u32      m_free_index;
            u32      m_sizeof_component;
            byte*    m_cp_data;
            index_t* m_map;       // instance index -> component index
            index_t* m_unmap;     // component index -> instance index
            binmap_t m_occupancy; // 32 bytes
        };

        struct allocator_t::object_t
        {
            DCORE_CLASS_PLACEMENT_NEW_DELETE
            alloc_t*          m_allocator;
            u32               m_num_instances;
            u32               m_max_instances;
            u32               m_max_component_types;
            u32               m_max_tag_types;
            u32               m_component_occupancy_sizeof; // per object, u32[]
            u32               m_tag_data_sizeof;            // per object, u32[]
            u32*              m_per_instance_component_occupancy;
            u32*              m_per_instance_tag_data;
            component_type_t* m_a_component;
            duomap_t          m_object_state;
        };
        typedef allocator_t::object_t object_t;

        static void s_teardown(alloc_t* allocator, component_type_t* container)
        {
            allocator->deallocate(container->m_cp_data);
            allocator->deallocate(container->m_map);
            allocator->deallocate(container->m_unmap);
            container->m_occupancy.release(allocator);
            container->m_free_index       = 0;
            container->m_sizeof_component = 0;
        }

        static object_t* s_create_object(alloc_t* allocator, u32 max_instances, u32 max_component_types, u32 max_tags)
        {
            object_t* object = allocator->construct<object_t>();

            object->m_allocator                  = allocator;
            object->m_num_instances              = 0;
            object->m_max_instances              = max_instances;
            object->m_max_component_types        = max_component_types;
            object->m_max_tag_types              = max_tags;
            object->m_component_occupancy_sizeof = (max_component_types + 31) >> 5;
            object->m_tag_data_sizeof            = (max_tags + 31) >> 5;

            object->m_per_instance_component_occupancy = g_allocate_array_and_memset<u32>(allocator, max_instances * object->m_component_occupancy_sizeof, 0);
            object->m_per_instance_tag_data            = g_allocate_array_and_memset<u32>(allocator, max_instances * object->m_tag_data_sizeof, 0);

            object->m_a_component = g_allocate_array_and_memset<component_type_t>(allocator, max_component_types, 0);

            duomap_t::config_t cfg = duomap_t::config_t::compute(max_instances);
            object->m_object_state.init_all_free(cfg, allocator);

            return object;
        }

        static void s_destroy_object(object_t* object)
        {
            alloc_t* allocator = object->m_allocator;

            for (u32 i = 0; i < object->m_max_component_types; ++i)
            {
                component_type_t* container = &object->m_a_component[i];
                if (container->m_sizeof_component > 0)
                    s_teardown(allocator, container);
            }

            allocator->deallocate(object->m_a_component);
            allocator->deallocate(object->m_per_instance_tag_data);
            allocator->deallocate(object->m_per_instance_component_occupancy);

            object->m_object_state.release(allocator);

            allocator->deallocate(object);
        }

        bool allocator_t::setup(alloc_t* allocator, u32 max_object_instances, u16 max_components, u8 max_tags)
        {
            if (m_object != nullptr)
                return false;
            m_object = s_create_object(allocator, max_object_instances, max_components, max_tags);
            return true;
        }

        void allocator_t::teardown()
        {
            if (m_object == nullptr)
                return;
            s_destroy_object(m_object);
            m_object = nullptr;
        }

        static s32 s_create_instance(object_t* object)
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

        static u32 s_instance_index(object_t const* object, u16 const cp_type_index, void const* cp_ptr)
        {
            component_type_t const& cptype         = object->m_a_component[cp_type_index];
            u32 const               local_cp_index = (u32)(((u32 const*)cp_ptr - (u32 const*)cptype.m_cp_data) / cptype.m_sizeof_component);
            return cptype.m_unmap[local_cp_index];
        }

        static void s_destroy_instance(object_t* object, u32 global_index)
        {
            if (object->m_object_state.set_free(global_index))
                object->m_num_instances--;
        }

        static bool s_register_component(object_t* object, u32 max_instances, u16 cp_index, s32 cp_sizeof, s32 cp_alignof)
        {
            // See if the component container is present, if not we need to initialize it
            if (cp_index < object->m_max_component_types && object->m_a_component[cp_index].m_sizeof_component == 0)
            {
                component_type_t* container   = &object->m_a_component[cp_index];
                container->m_free_index       = 0;
                container->m_sizeof_component = cp_sizeof;
                container->m_cp_data          = g_allocate_array<byte>(object->m_allocator, cp_sizeof * max_instances);
                container->m_map              = g_allocate_array_and_memset<index_t>(object->m_allocator, object->m_max_instances, 0xFFFFFFFF);
                container->m_unmap            = g_allocate_array_and_memset<index_t>(object->m_allocator, object->m_max_instances, 0xFFFFFFFF);

                binmap_t::config_t const cfg = binmap_t::config_t::compute(max_instances);
                container->m_occupancy.init_all_free_lazy(cfg, object->m_allocator);
                return true;
            }
            return false;
        }

        static void s_unregister_component(object_t* object, u16 cp_index)
        {
            component_type_t* container = &object->m_a_component[cp_index];
            if (container->m_sizeof_component > 0)
                s_teardown(object->m_allocator, container);
        }

        static bool s_has_cp(object_t* object, u32 global_index, u16 cp_index)
        {
            u32 const* component_occupancy = &object->m_per_instance_component_occupancy[global_index * object->m_component_occupancy_sizeof];
            return (component_occupancy[cp_index >> 5] & (1 << (cp_index & 31))) != 0;
        }

        static inline void* s_add_cp(object_t* object, u32 global_index, u16 cp_index)
        {
            ASSERT(cp_index < object->m_max_component_types);
            component_type_t* container = &object->m_a_component[cp_index];
            ASSERT(container->m_sizeof_component > 0); // component must be registered
            if (container->m_map[global_index] == c_null_index)
            {
                s32 local_cp_index = container->m_occupancy.find();
                if (local_cp_index == -1)
                {
                    if (container->m_free_index >= container->m_occupancy.size())
                        return nullptr;
                    container->m_occupancy.tick_all_free_lazy(container->m_free_index);
                    local_cp_index = container->m_free_index++;
                }
                container->m_occupancy.set_used(local_cp_index);

                // map and unmap
                container->m_map[global_index]     = (index_t)local_cp_index;
                container->m_unmap[local_cp_index] = (index_t)global_index;

                u32* component_occupancy = &object->m_per_instance_component_occupancy[global_index * object->m_component_occupancy_sizeof];
                component_occupancy[cp_index >> 5] |= (1 << (cp_index & 31));
                return &container->m_cp_data[local_cp_index * container->m_sizeof_component];
            }
            else
            {
                u32 const local_cp_index = container->m_map[global_index];
                return &container->m_cp_data[local_cp_index * container->m_sizeof_component];
            }
        }

        static void* s_rem_cp(object_t* object, u32 global_index, u16 cp_index)
        {
            ASSERT(cp_index < object->m_max_component_types);
            component_type_t* container = &object->m_a_component[cp_index];
            ASSERT(container->m_sizeof_component > 0);
            if (container->m_map[global_index] != c_null_index)
            {
                index_t const local_cp_index       = container->m_map[global_index];
                container->m_map[global_index]     = c_null_index;
                container->m_unmap[local_cp_index] = c_null_index;
                container->m_occupancy.set_free(local_cp_index);

                u32* component_occupancy = &object->m_per_instance_component_occupancy[global_index * object->m_component_occupancy_sizeof];
                component_occupancy[cp_index >> 5] &= ~(1 << (cp_index & 31));
                return &container->m_cp_data[local_cp_index * container->m_sizeof_component];
            }
            return nullptr;
        }

        static void* s_get_cp(object_t* object, u16 global_index, u16 cp_index)
        {
            ASSERT(cp_index < object->m_max_component_types);
            component_type_t* container = &object->m_a_component[cp_index];
            ASSERT(container->m_sizeof_component > 0);
            index_t const local_cp_index = container->m_map[global_index];
            if (local_cp_index != c_null_index)
                return &container->m_cp_data[local_cp_index * container->m_sizeof_component];
            return nullptr;
        }

        static void const* s_get_cp(object_t const* object, u16 global_index, u16 cp_index)
        {
            ASSERT(cp_index < object->m_max_component_types);
            component_type_t const* container = &object->m_a_component[cp_index];
            ASSERT(container->m_sizeof_component > 0);
            if (container->m_map[global_index] != c_null_index)
                return &container->m_cp_data[container->m_map[global_index] * container->m_sizeof_component];
            return nullptr;
        }

        static void* s_get_cp2(object_t* object, u16 cp1_index, void* cp1_ptr, u16 cp2_index)
        {
            component_type_t const& cp1type         = object->m_a_component[cp1_index];
            u32 const               local_cp1_index = (u32)(((u32 const*)cp1_ptr - (u32 const*)cp1type.m_cp_data) / cp1type.m_sizeof_component);
            u32 const               global_index    = cp1type.m_unmap[local_cp1_index];
            component_type_t&       cp2type         = object->m_a_component[cp2_index];
            ASSERT(cp2type.m_sizeof_component > 0);
            index_t const local_cp2_index = cp2type.m_map[global_index];
            if (local_cp2_index != c_null_index)
                return &cp2type.m_cp_data[local_cp2_index * cp2type.m_sizeof_component];
            return nullptr;
        }

        static void const* s_get_cp2(object_t const* object, u16 cp1_index, void const* cp1_ptr, u16 cp2_index)
        {
            component_type_t const& cp1type         = object->m_a_component[cp1_index];
            u32 const               local_cp1_index = (u32)(((u32 const*)cp1_ptr - (u32 const*)cp1type.m_cp_data) / cp1type.m_sizeof_component);
            u32 const               global_index    = cp1type.m_unmap[local_cp1_index];
            component_type_t const& cp2type         = object->m_a_component[cp2_index];
            ASSERT(cp2type.m_sizeof_component > 0);
            index_t const local_cp2_index = cp2type.m_map[global_index];
            if (local_cp2_index != c_null_index)
                return &cp2type.m_cp_data[local_cp2_index * cp2type.m_sizeof_component];
            return nullptr;
        }

        static bool s_has_tag(object_t* object, u16 global_index, u8 tg_index)
        {
            ASSERT(tg_index < object->m_max_tag_types);
            u32 const* tag_occupancy = &object->m_per_instance_tag_data[global_index * object->m_tag_data_sizeof];
            return (tag_occupancy[tg_index >> 5] & (1 << (tg_index & 31))) != 0;
        }

        static void s_add_tag(object_t* object, u16 global_index, u8 tg_index)
        {
            ASSERT(tg_index < object->m_max_tag_types);
            u32* tag_occupancy = &object->m_per_instance_tag_data[global_index * object->m_tag_data_sizeof];
            tag_occupancy[tg_index >> 5] |= (1 << (tg_index & 31));
        }

        static void s_rem_tag(object_t* object, u16 global_index, u8 tg_index)
        {
            ASSERT(tg_index < object->m_max_tag_types);
            u32* tag_occupancy = &object->m_per_instance_tag_data[global_index * object->m_tag_data_sizeof];
            tag_occupancy[tg_index >> 5] &= ~(1 << (tg_index & 31));
        }

        void* allocator_t::create_instance(u16 cp_index)
        {
            s32 const global_index = s_create_instance(m_object);
            return s_add_cp(m_object, global_index, cp_index);
        }

        void allocator_t::destroy_instance(u16 cp1_index, void* cp1)
        {
            ASSERT(cp1_index < m_object->m_max_component_types);
            if (cp1 != nullptr)
            {
                ASSERT(is_component_registered(cp1_index));
                u32 const global_index = s_instance_index(m_object, cp1_index, cp1);
                s_destroy_instance(m_object, global_index);
            }
        }

        u32  allocator_t::get_number_of_instances(u16 cp_index) const { return m_object->m_num_instances; }
        bool allocator_t::register_component(u16 cp_index, u32 max_instances, u32 cp_sizeof, u32 cp_alignof) { return s_register_component(m_object, max_instances, cp_index, cp_sizeof, cp_alignof); }
        bool allocator_t::is_component_registered(u16 cp_index) const { return (cp_index < m_object->m_max_component_types) && m_object->m_a_component[cp_index].m_sizeof_component > 0; }

        bool allocator_t::has_cp(u16 cp1_index, void const* cp1, u16 cp2_index) const
        {
            ASSERT(is_component_registered(cp1_index) && is_component_registered(cp2_index));
            u32 const global_index = s_instance_index(m_object, cp1_index, cp1);
            return s_has_cp(m_object, global_index, cp2_index);
        }

        void* allocator_t::add_cp(u16 cp1_index, void const* cp1, u16 cp2_index)
        {
            ASSERT(is_component_registered(cp1_index) && is_component_registered(cp2_index));
            u32 const global_index = s_instance_index(m_object, cp1_index, cp1);
            return s_add_cp(m_object, global_index, cp2_index);
        }

        void* allocator_t::rem_cp(u16 cp1_index, void const* cp1, u16 cp2_index)
        {
            ASSERT(is_component_registered(cp1_index) && is_component_registered(cp2_index));
            u32 const global_index = s_instance_index(m_object, cp1_index, cp1);
            return s_rem_cp(m_object, global_index, cp2_index);
        }

        void* allocator_t::get_cp(u16 cp1_index, void* cp1_ptr, u16 cp2_index)
        {
            ASSERT(is_component_registered(cp1_index) && is_component_registered(cp2_index));
            return s_get_cp2(m_object, cp1_index, cp1_ptr, cp2_index);
        }

        void const* allocator_t::get_cp(u16 cp1_index, void const* cp1_ptr, u16 cp2_index) const
        {
            ASSERT(is_component_registered(cp1_index) && is_component_registered(cp2_index));
            return s_get_cp2(m_object, cp1_index, cp1_ptr, cp2_index);
        }

        bool allocator_t::has_tag(u16 cp_index, void const* cp_ptr, u8 tg_index) const
        {
            if (cp_ptr != nullptr)
            {
                ASSERT(is_component_registered(cp_index));
                u32 const global_index = s_instance_index(m_object, cp_index, cp_ptr);
                return !s_has_tag(m_object, global_index, tg_index);
            }

            return false;
        }

        void allocator_t::add_tag(u16 cp_index, void const* cp_ptr, u8 tg_index)
        {
            if (cp_ptr != nullptr)
            {
                ASSERT(is_component_registered(cp_index));
                u32 const global_index = s_instance_index(m_object, cp_index, cp_ptr);
                s_add_tag(m_object, global_index, tg_index);
            }
        }

        void allocator_t::rem_tag(u16 cp_index, void const* cp_ptr, u8 tg_index)
        {
            if (cp_ptr != nullptr)
            {
                ASSERT(is_component_registered(cp_index));
                u32 const global_index = s_instance_index(m_object, cp_index, cp_ptr);
                s_rem_tag(m_object, global_index, tg_index);
            }
        }

        void* allocator_t::iterate_begin(u16 cp_index) const
        {
            s32 const global_index = m_object->m_object_state.find_used();
            if (global_index >= 0)
            {
                ASSERT(is_component_registered(cp_index));
                return s_get_cp(m_object, global_index, cp_index);
            }
            return nullptr;
        }

        void* allocator_t::iterate_next(u16 cp_index, void const* cp_ptr) const
        {
            ASSERT(is_component_registered(cp_index));
            u32 const global_index = s_instance_index(m_object, cp_index, cp_ptr);
            s32 const next_index   = m_object->m_object_state.next_used_up(global_index + 1);
            if (next_index >= 0)
            {
                return s_get_cp(m_object, next_index, cp_index);
            }
            return nullptr;
        }

    } // namespace ncs
} // namespace ncore
