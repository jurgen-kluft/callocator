#ifndef __C_ALLOCATOR_RESOURCE_POOL_H__
#define __C_ALLOCATOR_RESOURCE_POOL_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_debug.h"
#include "ccore/c_allocator.h"
#include "cbase/c_hbb.h"

namespace ncore
{
    class alloc_t;

    namespace ngfx
    {
        struct handle_t
        {
            u32 index;
            u16 type[2];
        };

        namespace nobject
        {
            struct array_t
            {
                array_t();

                byte* m_memory;
                u32   m_sizeof;
                u32   m_num_max;

                void        setup(alloc_t* allocator, u32 max_num_resources, u32 sizeof_component);
                void        teardown(alloc_t* allocator);
                void*       get_access(u32 index) { return &m_memory[index * m_sizeof]; }
                const void* get_access(u32 index) const { return &m_memory[index * m_sizeof]; }

                DCORE_CLASS_PLACEMENT_NEW_DELETE
            };

            // An inventory is using array_t but it has an additional bit array to mark if an item is used or free.
            struct inventory_t
            {
                inventory_t();

                void setup(alloc_t* allocator, u32 max_num_resources, u32 sizeof_component);
                void teardown(alloc_t* allocator);

                inline void allocate(u32 index)
                {
                    ASSERT(is_free(index));
                    set_used(index);
                }

                inline void deallocate(u32 index)
                {
                    ASSERT(is_used(index));
                    set_free(index);
                }

                void free_all();

                template <typename T> void construct(u32 index)
                {
                    allocate(index);
                    void* ptr = get_access(index);
                    new ( ptr) T();
                }

                template <typename T> void destruct(u32 index)
                {
                    deallocate(index);
                    void* ptr = get_access(index);
                    ((T*)ptr)->~T();
                }

                inline bool        is_free(u32 index) const { return (m_bitarray[index >> 5] & (1 << (index & 31))) == 0; }
                inline bool        is_used(u32 index) const { return (m_bitarray[index >> 5] & (1 << (index & 31))) != 0; }
                inline void        set_free(u32 index) { m_bitarray[index >> 5] &= ~(1 << (index & 31)); }
                inline void        set_used(u32 index) { m_bitarray[index >> 5] |= (1 << (index & 31)); }
                inline void*       get_access(u32 index) { return m_array.get_access(index); }
                inline const void* get_access(u32 index) const { return m_array.get_access(index); }

                DCORE_CLASS_PLACEMENT_NEW_DELETE

                u32*    m_bitarray;
                array_t m_array;
            };

            struct pool_t
            {
                pool_t();

                void setup(array_t* object_array, alloc_t* allocator);
                void teardown(alloc_t* allocator);

                u32  allocate(); //
                void deallocate(u32 index);
                void free_all();

                template <typename T> u32 construct()
                {
                    const u32 index = allocate();
                    void*     ptr   = get_access(index);
                    new (signature_t(), ptr) T();
                    return index;
                }

                template <typename T> void destruct(u32 index)
                {
                    void* ptr = get_access(index);
                    ((T*)ptr)->~T();
                    deallocate(index);
                }

                void*       get_access(u32 index);
                const void* get_access(u32 index) const;

                DCORE_CLASS_PLACEMENT_NEW_DELETE

                static const u32 c_invalid_handle = 0xFFFFFFFF;

                array_t* m_object_array;
                binmap_t m_free_resource_map;
            };

            namespace ntyped
            {
                template <typename T> struct pool_t
                {
                    void setup(alloc_t* allocator, u32 max_num_resources);
                    void teardown();

                    u32  allocate();
                    void deallocate(u32 index);

                    u32  construct();
                    void destruct(u32 index);

                    T*       get_access(u32 index);
                    const T* get_access(u32 index) const;
                    T*       obtain_access()
                    {
                        u32 index = allocate();
                        return get_access(index);
                    }

                    DCORE_CLASS_PLACEMENT_NEW_DELETE

                protected:
                    nobject::array_t m_object_array;
                    nobject::pool_t  m_object_pool;
                    alloc_t*         m_allocator = nullptr;
                };

                template <typename T> inline void pool_t<T>::setup(alloc_t* allocator_, u32 max_num_resources)
                {
                    m_allocator = allocator_;
                    m_object_array.setup(m_allocator, max_num_resources, sizeof(T));
                    m_object_pool.setup(&m_object_array, m_allocator);
                }

                template <typename T> inline void pool_t<T>::teardown()
                {
                    m_object_pool.teardown(m_allocator);
                    m_object_array.teardown(m_allocator);
                }

                template <typename T> inline u32 pool_t<T>::allocate()
                {
                    const u32 index = m_object_pool.allocate();
                    return index;
                }

                template <typename T> inline void pool_t<T>::deallocate(u32 index) { m_object_pool.deallocate(index); }

                template <typename T> inline u32 pool_t<T>::construct()
                {
                    const u32 index = m_object_pool.allocate();
                    void*     ptr   = m_object_pool.get_access(index);
                    new (signature_t(), ptr) T();
                    return index;
                }

                template <typename T> inline void pool_t<T>::destruct(u32 index)
                {
                    void* ptr = m_object_pool.get_access(index);
                    ((T*)ptr)->~T();
                    m_object_pool.deallocate(index);
                }

                template <typename T> inline T* pool_t<T>::get_access(u32 index) { return (T*)m_object_pool.get_access(index); }

                template <typename T> inline const T* pool_t<T>::get_access(u32 index) const { return (const T*)m_object_pool.get_access(index); }

            } // namespace ntyped
        } // namespace nobject

        // A multi resource pool, where an item is of a specific resource type and the pool holds multiple resource pools.
        // We can allocate a specific resource and the index encodes the resource type so that we know which pool it belongs to.

        // Pool that holds multiple resource pools
        namespace nresources
        {
            struct pool_t
            {
                void setup(alloc_t* allocator, u16 max_num_types);
                void teardown();

                DCORE_CLASS_PLACEMENT_NEW_DELETE

                template <typename T> T*       get_access(handle_t handle) { return (T*)get_access_raw(handle); }
                template <typename T> const T* get_access(handle_t handle) const { return (const T*)get_access_raw(handle); }

                // Register 'resource' by type
                template <typename T> bool register_resource(u32 max_num_resources) { return register_resource_pool(T::s_component_type_index, max_num_resources, sizeof(T)); }
                template <typename T> bool is_resource_type(handle_t handle) const 
                { 
                    return handle.type[0] == T::s_component_type_index; 
                }

                template <typename T> handle_t allocate()
                {
                    u32 const index = m_pools[T::s_component_type_index]->allocate();
                    return make_handle(T::s_component_type_index, index);
                }

                void deallocate(handle_t handle)
                {
                    const u32 type_index = get_object_type_index(handle);
                    const u32 res_index  = get_object_index(handle);
                    void*     ptr        = m_pools[type_index]->get_access(res_index);
                    m_pools[type_index]->deallocate(res_index);
                }

                template <typename T> handle_t construct()
                {
                    u16 const component_type_index = T::s_component_type_index;
                    u32 const index                = m_pools[component_type_index]->allocate();
                    void*     ptr                  = m_pools[component_type_index]->get_access(index);
                    new (ptr) T();
                    return make_handle(component_type_index, index);
                }

                template <typename T> void destruct(handle_t handle)
                {
                    const u32 type_index = get_object_type_index(handle);
                    ASSERT(T::s_component_type_index == type_index);
                    const u32 res_index = get_object_index(handle);
                    void*     ptr       = m_pools[type_index]->get_access(res_index);
                    ((T*)ptr)->~T();
                    m_pools[type_index]->deallocate(res_index);
                }

                static const handle_t c_invalid_handle;

            private:
                inline u32 get_object_type_index(handle_t handle) const { return handle.type[0]; }
                inline u32 get_object_index(handle_t handle) const { return handle.index; }

                inline handle_t make_handle(u32 type_index, u32 index) const
                {
                    handle_t handle;
                    handle.type[0] = type_index;
                    handle.index   = index;
                    return handle;
                }

                void*       get_access_raw(handle_t handle);
                const void* get_access_raw(handle_t handle) const;
                bool        register_resource_pool(s16 type_index, u32 max_num_resources, u32 sizeof_component);

                nobject::pool_t** m_pools;
                u32               m_num_pools;
                alloc_t*          m_allocator;
            };

#define DECLARE_COMPONENT_TYPE(N) static const u16 s_component_type_index = N;

        } // namespace nresources

        namespace nobjects_with_components
        {
            // Limitations:
            // - more than 60.000 object types (0 to 65535)
            // - more than 60.000 resource types (0 to 65535-1)
            // - max number of components per object type
            // - max 128 tag types (0 to 127)
            // - 1 billion objects (2^30)
            struct pool_t
            {
                void setup(alloc_t* allocator, u32 max_num_object_types, u32 max_num_resource_types);
                void teardown();

                DCORE_CLASS_PLACEMENT_NEW_DELETE

                template <typename T> T* get_object(handle_t handle)
                {
                    ASSERT(T::s_object_type_index == get_object_type_index(handle));
                    ASSERT(0 == get_component_type_index(handle));
                    return (T*)get_object_raw(handle);
                }
                template <typename T> const T* get_object(handle_t handle) const
                {
                    ASSERT(T::s_object_type_index == get_object_type_index(handle));
                    ASSERT(0 == get_component_type_index(handle));
                    return (const T*)get_object_raw(handle);
                }

                template <typename T> T* get_component(handle_t handle)
                {
                    ASSERT(get_component_type_index(handle) > 0);
                    ASSERT((T::s_component_type_index + 1) == get_component_type_index(handle));
                    return (T*)get_object_raw(handle);
                }
                template <typename T> const T* get_component(handle_t handle) const
                {
                    ASSERT(get_component_type_index(handle) > 0);
                    ASSERT((T::s_component_type_index + 1) == get_component_type_index(handle));
                    return (const T*)get_object_raw(handle);
                }

                // Register 'object' by type
                template <typename T> bool     register_object_type(u32 max_instances, u32 max_components) { return register_object_type(T::s_object_type_index, max_instances, sizeof(T), max_components, m_max_component_types); }
                template <typename T> bool     is_object(handle_t handle) const { return is_handle_an_object(handle) && get_object_type_index(handle) == T::s_object_type_index; }
                template <typename T> handle_t allocate_object() { return allocate_object(T::s_object_type_index); }
                template <typename T> handle_t construct_object()
                {
                    handle_t handle = allocate_object(T::s_object_type_index);
                    void*    ptr    = get_access_raw(handle);
                    new (signature_t(), ptr) T();
                    return handle;
                }

                void deallocate_object(handle_t handle)
                {
                    const u32 object_type_index = get_object_type_index(handle);
                    ASSERT(object_type_index < m_max_object_types);
                    const u32 object_index = get_object_index(handle);
                    m_objects[object_type_index].m_object_map.set_free(object_index);
                }

                template <typename T> void destruct_object(handle_t handle)
                {
                    const u32 object_type_index = get_object_type_index(handle);
                    ASSERT(object_type_index == T::s_object_type_index);
                    ASSERT(object_type_index < m_max_object_types);
                    const u32 object_index = get_object_index(handle);
                    void*     ptr          = m_objects[object_type_index].m_a_component[0].get_access(object_index);
                    ((T*)ptr)->~T();
                    m_objects[object_type_index].m_object_map.set_free(object_index);
                }

                // Register 'component' by type
                template <typename T, typename R> bool register_component_type() { return register_component_type(T::s_object_type_index, R::s_component_type_index, sizeof(R)); }
                template <typename T> bool             is_component(handle_t handle) const { return is_handle_a_component(handle) && get_component_type_index(handle) == T::s_component_type_index; }
                template <typename T> handle_t         allocate_component(handle_t object_handle)
                {
                    const u16 component_type_index = T::s_component_type_index;
                    const u32 object_type_index    = get_object_type_index(object_handle);
                    const u32 object_index         = get_object_index(object_handle);
                    ASSERT(m_objects[object_type_index].m_a_component[component_type_index + 1] != nullptr);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    m_objects[object_type_index].m_a_component[local_component_index].allocate(object_index);
                    return make_component_handle(get_object_type_index(object_handle), component_type_index, object_index);
                }
                template <typename T> handle_t construct_component(handle_t object_handle)
                {
                    const u16 component_type_index = T::s_component_type_index;
                    const u32 object_type_index    = get_object_type_index(object_handle);
                    const u32 object_index         = get_object_index(object_handle);
                    ASSERT(m_objects[object_type_index].m_a_component[component_type_index + 1] != nullptr);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF); // component hasn't been registered
                    m_objects[object_type_index].m_a_component[local_component_index].construct<T>(object_index);
                    return make_component_handle(get_object_type_index(object_handle), component_type_index, object_index);
                }

                void deallocate_component(handle_t handle)
                {
                    const u32 object_type_index    = get_object_type_index(handle);
                    const u32 component_type_index = get_component_type_index(handle);
                    const u32 component_index      = get_component_index(handle);
                    ASSERT(object_type_index < m_max_object_types);
                    ASSERT(component_type_index < m_max_component_types);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF); // component hasn't been registered
                    m_objects[object_type_index].m_a_component[local_component_index].deallocate(component_index);
                }

                template <typename T> void destruct_component(handle_t handle)
                {
                    const u32 object_type_index    = get_object_type_index(handle);
                    const u32 component_type_index = get_component_type_index(handle);
                    const u32 component_index      = get_component_index(handle);
                    ASSERT(object_type_index < m_max_object_types);
                    ASSERT(component_type_index < m_max_component_types);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF); // component hasn't been registered
                    m_objects[object_type_index].m_a_component[local_component_index].destruct<T>(component_index);
                }

                template <typename T> bool has_component(handle_t object_handle) const
                {
                    const u16 component_type_index = T::s_component_type_index;
                    const u32 object_type_index    = get_object_type_index(object_handle);
                    ASSERT(object_type_index < m_max_object_types);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF); // component hasn't been registered
                    return local_component_index != 0xFFFF && m_objects[object_type_index].m_a_component[local_component_index].is_used(get_object_index(object_handle));
                }

                // Tags
                template <typename T> void add_tag(handle_t object_handle)
                {
                    const u16 tag_type_index    = T::s_tag_type_index;
                    const u32 object_type_index = get_object_type_index(object_handle);
                    const u32 object_index      = get_object_index(object_handle);
                    ASSERT(m_objects[object_type_index].m_a_tags != nullptr);
                    m_objects[object_type_index].m_a_tags[object_index].add_tag(tag_type_index);
                }

                template <typename T> void rem_tag(handle_t object_handle)
                {
                    const u16 tag_type_index    = T::s_tag_type_index;
                    const u32 object_type_index = get_object_type_index(object_handle);
                    const u32 object_index      = get_object_index(object_handle);
                    ASSERT(m_objects[object_type_index].m_a_tags != nullptr);
                    m_objects[object_type_index].m_a_tags[object_index].rem_tag(tag_type_index);
                }

                template <typename T> bool has_tag(handle_t object_handle) const
                {
                    ASSERT(is_handle_an_object(object_handle));
                    const u16 tag_type_index    = T::s_tag_type_index;
                    const u32 object_type_index = get_object_type_index(object_handle);
                    const u32 object_index      = get_object_index(object_handle);
                    ASSERT(m_objects[object_type_index].m_a_tags != nullptr);
                    return m_objects[object_type_index].m_a_tags[object_index].has_tag(tag_type_index);
                }

                static const handle_t c_invalid_handle;

            private:
                inline handle_t make_object_handle(u16 object_type_index, u32 object_index) const
                {
                    handle_t handle;
                    handle.type[0] = object_type_index;
                    handle.type[1] = 0;
                    handle.index   = object_index;
                    return handle;
                }

                inline handle_t make_component_handle(u16 object_type_index, u16 component_type_index, u16 component_index) const
                {
                    handle_t handle;
                    handle.type[0] = object_type_index;
                    handle.type[1] = component_type_index + 1;
                    handle.index   = component_index;
                    return handle;
                }

                inline u16  get_object_type_index(handle_t handle) const { return handle.type[0]; }
                inline u16  get_component_type_index(handle_t handle) const { return handle.type[1]; }
                inline u32  get_object_index(handle_t handle) const { return handle.index; }
                inline u32  get_component_index(handle_t handle) const { return handle.index; }
                inline u16  get_handle_type(handle_t handle) const { return handle.type[1] == 0 ? 0 : 1; }
                inline bool is_handle_an_object(handle_t handle) const { return get_handle_type(handle) == 0; }
                inline bool is_handle_a_component(handle_t handle) const { return get_handle_type(handle) == 1; }

                bool     register_object_type(u16 object_type_index, u32 max_num_objects, u32 sizeof_object, u32 max_num_components_local, u32 max_num_components_global);
                bool     register_component_type(u16 object_type_index, u16 component_type_index, u32 sizeof_component);
                handle_t allocate_object(u16 object_type_index);
                handle_t allocate_component(handle_t object_handle, u16 component_type_index);

                void* get_object_raw(handle_t handle)
                {
                    const u32 index                = get_component_index(handle);
                    const u16 object_type_index    = get_object_type_index(handle);
                    const u16 component_type_index = get_component_type_index(handle);
                    ASSERT(object_type_index < m_max_object_types);
                    ASSERT(component_type_index < m_max_component_types);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF || local_component_index < m_objects[object_type_index].m_max_components);
                    return m_objects[object_type_index].m_a_component[local_component_index].get_access(index);
                }

                const void* get_object_raw(handle_t handle) const
                {
                    const u32 index                = get_component_index(handle);
                    const u16 object_type_index    = get_object_type_index(handle);
                    const u16 component_type_index = get_component_type_index(handle);
                    ASSERT(object_type_index < m_max_object_types);
                    ASSERT(component_type_index < m_max_component_types);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF || local_component_index < m_objects[object_type_index].m_max_components);
                    return m_objects[object_type_index].m_a_component[local_component_index].get_access(index);
                }

                struct tags_t
                {
                    inline bool has_tag(u16 tag_type_index) const { return m_a_tags[tag_type_index >> 6] & ((u64)1 << (tag_type_index & 63)); }
                    inline void add_tag(u16 tag_type_index) { m_a_tags[tag_type_index >> 6] |= ((u64)1 << (tag_type_index & 63)); }
                    inline void rem_tag(u16 tag_type_index) { m_a_tags[tag_type_index >> 6] &= ~((u64)1 << (tag_type_index & 63)); }
                    u64         m_a_tags[2];
                };

                struct object_t
                {
                    binmap_t              m_object_map;
                    nobject::inventory_t* m_a_component;     // m_a_component[max components for this object], first inventory_t is for object
                    u16*                  m_a_component_map; // m_a_component_map[m_max_components globally], index 0 is for object
                    tags_t*               m_a_tags;          // m_a_tags, array of tags_t
                    u32                   m_max_components;  // max components for this object
                    u32                   m_num_components;  // current number of components for this object, excluding the object itself
                };

                object_t* m_objects;
                alloc_t*  m_allocator;
                u16       m_max_object_types;
                u16       m_max_component_types;
            };

#define DECLARE_OBJECT_TYPE(N) static const u16 s_object_type_index = N;
#define DECLARE_TAG_TYPE(N) static const u16 s_tag_type_index = N;

        } // namespace nobjects_with_components

    } // namespace ngfx
} // namespace ncore

#endif
