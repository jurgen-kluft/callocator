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

        namespace ncomponents
        {
            // components pool

            void pool_t::setup(alloc_t* allocator, u16 max_num_types_locally, u16 max_num_types_globally)
            {
                m_allocator = allocator;
                m_num_pools = 0;
                m_max_pools = max_num_types_locally;
                m_a_map     = (u16*)g_allocate_and_memset(allocator, max_num_types_globally * sizeof(u16), 0xFFFFFFFF);
                m_a_pool    = (nobject::pool_t*)g_allocate_and_memset(allocator, max_num_types_locally * sizeof(nobject::pool_t), 0);
            }

            void pool_t::teardown()
            {
                for (u32 i = 0; i < m_num_pools; i++)
                {
                    m_a_pool[i].teardown(m_allocator);
                }
                m_allocator->deallocate(m_a_pool);
                m_allocator->deallocate(m_a_map);
            }

            void* pool_t::get_access_raw(u16 global_type_index, nhandle_t handle)
            {
                u16 const local_type_index = m_a_map[global_type_index];
                ASSERT(local_type_index != 0xFFFF && local_type_index < m_num_pools);
                return m_a_pool[local_type_index].get_access(handle);
            }

            const void* pool_t::get_access_raw(u16 global_type_index, nhandle_t handle) const
            {
                u16 const local_type_index = m_a_map[global_type_index];
                ASSERT(local_type_index != 0xFFFF && local_type_index < m_num_pools);
                return m_a_pool[local_type_index].get_access(handle);
            }

            bool pool_t::register_component_pool(u16 type_index, u32 max_num_components, u32 sizeof_component)
            {
                if (m_a_map[type_index] == 0xFFFF && m_num_pools < m_max_pools)
                {
                    u16 const local_type_index = m_num_pools++;
                    m_a_map[type_index]        = local_type_index;
                    nobject::array_t array;
                    array.setup(m_allocator, max_num_components, sizeof_component);
                    m_a_pool[local_type_index].setup(array, m_allocator);
                    return true;
                }
                return false;
            }
        } // namespace ncomponents

        namespace nobjects_with_components
        {
            // objects with components pool

            void pool_t::setup(alloc_t* allocator, u32 max_num_object_types, u32 max_num_resource_types)
            {
                m_allocator           = allocator;
                m_object_types        = (object_type_t*)g_allocate_and_memset(allocator, max_num_object_types * sizeof(object_type_t), 0);
                m_max_object_types    = max_num_object_types;
                m_max_component_types = max_num_resource_types + 1; // +1 for object
            }

            void pool_t::teardown()
            {
                for (u32 i = 0; i < m_max_object_types; i++)
                {
                    if (m_object_types[i].m_max_objects > 0)
                    {
                        for (u32 j = 0; j < m_object_types[i].m_num_components; j++)
                        {
                            m_object_types[i].m_a_component[j].teardown(m_allocator);
                        }
                        m_allocator->deallocate(m_object_types[i].m_a_component);
                        m_allocator->deallocate(m_object_types[i].m_a_component_map);
                        m_object_types[i].m_objects_map.release(m_allocator);
                    }
                }
                m_allocator->deallocate(m_object_types);
            }

            bool pool_t::register_object_type(u16 object_type_index, u32 max_num_objects, u32 sizeof_object, u32 max_num_components_local, u32 max_num_components_global)
            {
                ASSERT(m_object_types[object_type_index].m_objects_map.size() == 0);
                if (m_object_types[object_type_index].m_max_objects == 0)
                {
                    ASSERT(object_type_index < m_max_object_types);
                    object_type_t&     o   = m_object_types[object_type_index];
                    binmap_t::config_t cfg = binmap_t::config_t::compute(max_num_objects);
                    o.m_objects_map.init_all_free(cfg, m_allocator);
                    o.m_a_component = (nobject::inventory_t*)g_allocate_and_memset(m_allocator, (max_num_components_local + 1) * sizeof(nobject::inventory_t), 0);
                    o.m_a_component[0].setup(m_allocator, max_num_objects, sizeof_object);
                    o.m_a_component_map    = (u16*)g_allocate_and_memset(m_allocator, max_num_components_global * sizeof(u16), 0xFFFFFFFF);
                    o.m_a_component_map[0] = 0;
                    o.m_num_components     = 1;
                    o.m_num_objects        = 0;
                    o.m_max_components     = max_num_components_local + 1;
                    o.m_max_objects        = max_num_objects;
                    return true;
                }

                // Object was already registered
                return false;
            }

            bool pool_t::register_component_type(u16 object_type_index, u16 component_type_index, u32 sizeof_component)
            {
                ASSERT(m_object_types[object_type_index].m_a_component_map[component_type_index] == 0xFFFF);
                if (m_object_types[object_type_index].m_a_component_map[component_type_index] == 0xFFFF)
                {
                    ASSERT(object_type_index < m_max_object_types);
                    ASSERT(component_type_index < m_max_component_types);
                    object_type_t& o                          = m_object_types[object_type_index];
                    u16 const      local_component_index      = o.m_num_components++;
                    o.m_a_component_map[component_type_index] = local_component_index;
                    o.m_a_component[local_component_index].setup(m_allocator, o.m_max_objects, sizeof_component);
                    return true;
                }

                // Component was already registered
                return false;
            }

            s32 pool_t::iterate_begin(const u16 object_type_index) const
            {
                object_type_t const* object_type  = &m_object_types[object_type_index];
                s32 const            object_index = object_type->m_objects_map.find_used();
                return object_index;
            }

            s32 pool_t::iterate_begin(const u16 object_type_index, const u16 component_type_index) const
            {
                object_type_t const* object_type      = &m_object_types[object_type_index];
                u16 const            local_type_index = object_type->m_a_component_map[component_type_index];
                if (local_type_index == 0xFFFF)
                    return c_invalid_nhandle;
                s32                object_index = object_type->m_objects_map.find_used();
                inventory_t const* inventory    = &object_type->m_a_component[local_type_index];
                while (object_index >= 0 && inventory->is_used(object_index) == false)
                {
                    object_index = object_type->m_objects_map.next_used_up(object_index + 1);
                }

                if (object_index < 0)
                    return c_invalid_nhandle;

                return object_index;
            }

            s32 pool_t::iterate_next(const u16 object_type_index, const u16 component_type_index, const u32 index) const
            {
                // determine the index of the current object, then find the next object from there
                object_type_t const* object_type = &m_object_types[object_type_index];
                s32                  next_index  = object_type->m_objects_map.next_used_up(index + 1);
                if (component_type_index > 0)
                {
                    // Find the next object that has the component marked as used
                    u16 const          local_type_index = object_type->m_a_component_map[component_type_index];
                    inventory_t const* inventory        = &object_type->m_a_component[local_type_index];
                    while (next_index >= 0 && inventory->is_used(next_index) == false)
                    {
                        next_index = object_type->m_objects_map.next_used_up(next_index + 1);
                    }
                }

                if (next_index < 0)
                    return c_invalid_nhandle;

                return next_index;
            }

        } // namespace nobjects_with_components
    } // namespace nobject
} // namespace ncore
