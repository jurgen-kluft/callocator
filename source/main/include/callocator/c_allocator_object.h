#ifndef __C_ALLOCATOR_OBJECT_COMPONENT_H__
#define __C_ALLOCATOR_OBJECT_COMPONENT_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_debug.h"
#include "ccore/c_allocator.h"
#include "cbase/c_binmap.h"
#include "cbase/c_integer.h"

namespace ncore
{
    class alloc_t;

    namespace nobject
    {
        struct handle_t // 8 bytes
        {
            u32 index;
            u16 type[2];

            inline bool operator==(const handle_t& other) const { return index == other.index && type[0] == other.type[0] && type[1] == other.type[1]; }
            inline bool operator!=(const handle_t& other) const { return index != other.index || type[0] != other.type[0] || type[1] != other.type[1]; }
        };
        const handle_t c_invalid_handle = {0xFFFFFFFF, 0xFFFF, 0xFFFF};

        struct array_t // 16 bytes
        {
            inline array_t() : m_memory(nullptr), m_num_max(0), m_sizeof(0) {}
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            byte* m_memory;
            u32   m_sizeof;
            u32   m_num_max;

            void setup(alloc_t* allocator, u32 max_num_components, u32 sizeof_component);
            void teardown(alloc_t* allocator);

            void*       get_access(u32 index) { return &m_memory[index * m_sizeof]; }
            const void* get_access(u32 index) const { return &m_memory[index * m_sizeof]; }

            inline u32 ptr_to_index(const void* ptr) const { return (u32)(((byte*)ptr - m_memory) / m_sizeof); }

            template <typename T> T* get_access_as(u32 index)
            {
                ASSERT(sizeof(T) <= m_sizeof);
                return (T*)get_access(index);
            }

            template <typename T> const T* get_access_as(u32 index) const
            {
                ASSERT(sizeof(T) <= m_sizeof);
                return (const T*)get_access(index);
            }
        };

        // An inventory is using array_t but it has an additional bit array to mark if an item is used or free.
        struct inventory_t // 24 bytes
        {
            inline inventory_t() : m_bitarray(nullptr), m_array() {}
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            void setup(alloc_t* allocator, u32 max_num_components, u32 sizeof_component);
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

            void* find_next(void* ptr)
            {
                u32 const index = (ptr == nullptr) ? 0 : m_array.ptr_to_index(ptr) + 1;

                const u32 nw = (m_array.m_num_max + 31) >> 5;
                const u32 iw = index >> 5;
                const u32 ib = index & 31;

                // Check the first word directly since we require masking
                u32 bits = m_bitarray[iw] & (0xffffffff << ib);
                if (bits != 0)
                {
                    u32 bit = math::findFirstBit(bits);
                    if (bit < m_array.m_num_max)
                        return m_array.get_access(bit);
                    return nullptr;
                }

                for (u32 i = iw + 1; i < nw; ++i)
                {
                    u32 bits = m_bitarray[i];
                    if (bits != 0)
                    {
                        u32 bit = math::findFirstBit(bits);
                        if (bit < m_array.m_num_max)
                            return m_array.get_access(bit + (i << 5));
                        return nullptr;
                    }
                }
                return nullptr;
            }

            void free_all();

            template <typename T> void construct(u32 index)
            {
                allocate(index);
                void* ptr = get_access(index);
                new (ptr) T();
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

            u32*    m_bitarray;
            array_t m_array;
        };

        struct pool_t // 48 bytes
        {
            inline pool_t() : m_object_array(), m_free_resource_map() {}
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            void setup(array_t& object_array, alloc_t* allocator);
            void teardown(alloc_t* allocator);

            u32  allocate(); //
            void deallocate(u32 index);
            void free_all();

            template <typename T> u32 construct()
            {
                const u32 index = allocate();
                void*     ptr   = get_access(index);
                new (ptr) T();
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

            array_t  m_object_array;
            binmap_t m_free_resource_map;
        };

        namespace ntyped
        {
            template <typename T> struct pool_t
            {
                void setup(alloc_t* allocator, u32 max_num_components);
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
                nobject::pool_t m_object_pool;
                alloc_t*        m_allocator = nullptr;
            };

            template <typename T> inline void pool_t<T>::setup(alloc_t* allocator, u32 max_num_components)
            {
                m_allocator = allocator;
                array_t array;
                array.setup(m_allocator, max_num_components, sizeof(T));
                m_object_pool.setup(array, m_allocator);
            }

            template <typename T> inline void pool_t<T>::teardown() { m_object_pool.teardown(m_allocator); }

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
                new (ptr) T();
                return index;
            }

            template <typename T> inline void pool_t<T>::destruct(u32 index)
            {
                void* ptr = m_object_pool.get_access(index);
                ((T*)ptr)->~T();
                m_object_pool.deallocate(index);
            }

            template <typename T> inline T*       pool_t<T>::get_access(u32 index) { return (T*)m_object_pool.get_access(index); }
            template <typename T> inline const T* pool_t<T>::get_access(u32 index) const { return (const T*)m_object_pool.get_access(index); }

        } // namespace ntyped

        // A multi resource pool, where an item is of a specific resource type and the pool holds multiple resource pools.
        // We can allocate a specific resource and the index encodes the resource type so that we know which pool it belongs to.

        // Pool that holds multiple component pools
        namespace ncomponents
        {
            struct pool_t // 32 bytes
            {
                inline pool_t() : m_a_map(nullptr), m_a_pool(nullptr), m_num_pools(0), m_max_pools(0), m_allocator(nullptr) {}
                DCORE_CLASS_PLACEMENT_NEW_DELETE

                void setup(alloc_t* allocator, u16 max_num_types_locally, u16 max_num_types_globally);
                void teardown();

                template <typename T> T* get(handle_t handle, u16 _component_type_index = 0xFFFF)
                {
                    handle.type[1] = (_component_type_index == 0xFFFF) ? handle.type[1] : _component_type_index;
                    return (T*)get_access_raw(handle);
                }
                template <typename T> const T* get(handle_t handle, u16 _component_type_index = 0xFFFF) const
                {
                    handle.type[1] = (_component_type_index == 0xFFFF) ? handle.type[1] : _component_type_index;
                    return (const T*)get_access_raw(handle);
                }

                // Register 'resource' by type
                template <typename T> bool register_component(u16 _component_type_index, u32 max_num_components) { return register_component_pool(_component_type_index, max_num_components, sizeof(T)); }
                bool                       is_component_type(handle_t handle) const { return handle != c_invalid_handle; }

                template <typename T> handle_t allocate(u16 _component_type_index)
                {
                    u16 const type_index       = _component_type_index;
                    const u16 local_type_index = m_a_map[type_index];
                    u32 const index            = m_a_pool[local_type_index].allocate();
                    return make_handle(type_index, index);
                }

                void deallocate(handle_t handle)
                {
                    const u16 type_index       = get_type_index(handle);
                    const u32 index            = get_index(handle);
                    const u16 local_type_index = m_a_map[type_index];
                    m_a_pool[local_type_index].deallocate(index);
                }

                template <typename T> handle_t construct(u16 _component_type_index)
                {
                    u16 const type_index       = _component_type_index;
                    u16 const local_type_index = m_a_map[type_index];
                    u32 const index            = m_a_pool[local_type_index].allocate();
                    void*     ptr              = m_a_pool[local_type_index].get_access(index);
                    new (ptr) T();
                    return make_handle(type_index, index);
                }

                template <typename T> void destruct(handle_t handle)
                {
                    const u16 type_index       = get_type_index(handle);
                    u16 const local_type_index = m_a_map[type_index];
                    const u32 index            = get_index(handle);
                    void*     ptr              = m_a_pool[local_type_index].get_access(index);
                    ((T*)ptr)->~T();
                    m_a_pool[local_type_index].deallocate(index);
                }

            private:
                static inline u32      get_index(handle_t handle) { return handle.index; }
                static inline u16      get_type_index(handle_t handle) { return handle.type[1]; }
                static inline handle_t make_handle(u16 type_index, u32 index)
                {
                    handle_t handle;
                    handle.type[0] = 0;
                    handle.type[1] = type_index;
                    handle.index   = index;
                    return handle;
                }

                void*       get_access_raw(handle_t handle);
                const void* get_access_raw(handle_t handle) const;
                bool        register_component_pool(u16 type_index, u32 max_num_components, u32 sizeof_component);

                u16*             m_a_map;
                nobject::pool_t* m_a_pool;
                u32              m_num_pools;
                u32              m_max_pools;
                alloc_t*         m_allocator;
            };

#define DECLARE_COMPONENT_TYPE(N) static const u16 s_component_type_index = N;

        } // namespace ncomponents

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
                    handle.type[1] = 0; // Objects have a component index of 0
                    return (T*)get_object_raw(handle);
                }

                template <typename T> T* get_object(handle_t handle) const
                {
                    handle.type[1] = 0; // Objects have a component index of 0
                    return (const T*)get_object_raw(handle);
                }

                template <typename T> T* get_component(handle_t handle, u16 const _component_type_index = 0xFFFF)
                {
                    // handle can be an object handle or any component handle, the user is
                    // asking here for a component of type T, so we need to set the component type index
                    handle.type[1] = (_component_type_index == 0xFFFF) ? handle.type[1] : (_component_type_index + 1);
                    if (handle.type[1] == 0)
                        return nullptr;
                    return (T*)get_object_raw(handle);
                }
                template <typename T> T* get_component(handle_t handle, u16 const _component_type_index = 0xFFFF) const
                {
                    // handle can be an object handle or any component handle, the user is
                    // asking here for a component of type T, so we need to set the component type index
                    handle.type[1] = _component_type_index == 0xFFFF ? handle.type[1] : (_component_type_index + 1);
                    if (handle.type[1] == 0)
                        return nullptr;
                    return (const T*)get_object_raw(handle);
                }

                // Register 'object' by type
                template <typename T> bool register_object_type(u16 object_type_index, u32 max_instances, u32 max_components) { return register_object_type(object_type_index, max_instances, sizeof(T), max_components, m_max_component_types); }
                bool                       is_object(handle_t handle) const { return is_handle_an_object(handle); }

                handle_t                   allocate_object(u16 object_type_index)
                {
                    ASSERT(object_type_index < m_max_object_types);
                    const u32 object_index = m_objects[object_type_index].m_objects_free_map.find_and_set();
                    return make_object_handle(object_type_index, object_index);
                }

                template <typename T> handle_t construct_object(u16 object_type_index)
                {
                    handle_t handle = allocate_object(object_type_index);
                    void*    ptr    = get_object_raw(handle);
                    new (ptr) T();
                    return handle;
                }

                void deallocate_object(handle_t handle)
                {
                    const u32 object_type_index = get_object_type_index(handle);
                    ASSERT(object_type_index < m_max_object_types);
                    const u32 object_index = get_object_index(handle);
                    m_objects[object_type_index].m_objects_free_map.set_free(object_index);
                }

                template <typename T> void destruct_object(handle_t handle)
                {
                    const u32 object_type_index = get_object_type_index(handle);
                    ASSERT(object_type_index < m_max_object_types);
                    const u32 object_index = get_object_index(handle);
                    void*     ptr          = m_objects[object_type_index].m_a_component[0].get_access(object_index);
                    ((T*)ptr)->~T();
                    m_objects[object_type_index].m_objects_free_map.set_free(object_index);
                }

                // Iterate over objects or components, start with ptr = nullptr, and continue until nullptr is returned
                template <typename T> T* iterate_object(u16 const object_type_index, T* ptr) { return m_objects[object_type_index].m_a_component[0].find_next(ptr); }
                template <typename T> T* iterate_component(u16 const object_type_index, u16 const component_type_index, T* ptr) { return m_objects[object_type_index].m_a_component[component_type_index + 1].find_next(ptr); }

                // Register 'component' by type
                template <typename T> bool register_component_type(u16 const _type_index, u16 const _component_type_index) { return register_component_type(_type_index, _component_type_index + 1, sizeof(T)); }
                bool                       is_component(handle_t handle) const { return is_handle_a_component(handle); }
                handle_t                   allocate_component(handle_t object_handle, u16 _component_type_index)
                {
                    const u16 component_type_index  = _component_type_index + 1;
                    const u32 object_type_index     = get_object_type_index(object_handle);
                    const u32 object_index          = get_object_index(object_handle);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF); // component hasn't been registered
                    m_objects[object_type_index].m_a_component[local_component_index].allocate(object_index);
                    return make_component_handle(object_type_index, component_type_index, object_index);
                }
                template <typename T> handle_t construct_component(handle_t object_handle, u16 _component_type_index)
                {
                    const u16 component_type_index = _component_type_index + 1;
                    const u32 object_type_index    = get_object_type_index(object_handle);
                    const u32 object_index         = get_object_index(object_handle);
                    ASSERT(m_objects[object_type_index].m_a_component[component_type_index] != nullptr);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF); // component hasn't been registered
                    m_objects[object_type_index].m_a_component[local_component_index].construct<T>(object_index);
                    return make_component_handle(object_type_index, component_type_index, object_index);
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

                bool has_component(handle_t object_handle, u16 _component_type_index) const
                {
                    const u16 component_type_index = _component_type_index + 1;
                    const u32 object_type_index    = get_object_type_index(object_handle);
                    ASSERT(object_type_index < m_max_object_types);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF); // component hasn't been registered
                    return local_component_index != 0xFFFF && m_objects[object_type_index].m_a_component[local_component_index].is_used(get_object_index(object_handle));
                }

            private:
                static D_FORCEINLINE handle_t make_object_handle(u16 object_type_index, u32 object_index)
                {
                    handle_t handle;
                    handle.type[0] = object_type_index;
                    handle.type[1] = 0;
                    handle.index   = object_index;
                    return handle;
                }

                static D_FORCEINLINE handle_t make_component_handle(u16 object_type_index, u16 component_type_index, u16 component_index)
                {
                    handle_t handle;
                    handle.type[0] = object_type_index;
                    handle.type[1] = component_type_index;
                    handle.index   = component_index;
                    return handle;
                }

                static D_FORCEINLINE u16  get_object_type_index(handle_t handle) { return handle.type[0]; }
                static D_FORCEINLINE u16  get_component_type_index(handle_t handle) { return handle.type[1]; }
                static D_FORCEINLINE u32  get_object_index(handle_t handle) { return handle.index; }
                static D_FORCEINLINE u32  get_component_index(handle_t handle) { return handle.index; }
                static D_FORCEINLINE u16  get_handle_type(handle_t handle) { return handle.type[1] == 0 ? 0 : 1; }
                static D_FORCEINLINE bool is_handle_an_object(handle_t handle) { return get_handle_type(handle) == 0; }
                static D_FORCEINLINE bool is_handle_a_component(handle_t handle) { return get_handle_type(handle) == 1; }

                bool register_object_type(u16 object_type_index, u32 max_num_objects, u32 sizeof_object, u32 max_num_components_local, u32 max_num_components_global);
                bool register_component_type(u16 object_type_index, u16 component_type_index, u32 sizeof_component);

                void* get_object_raw(handle_t handle)
                {
                    const u32 index                = get_component_index(handle);
                    const u16 object_type_index    = get_object_type_index(handle);
                    const u16 component_type_index = get_component_type_index(handle);
                    ASSERT(object_type_index < m_max_object_types);
                    ASSERT(component_type_index < m_max_component_types);
                    const u16 local_component_index = m_objects[object_type_index].m_a_component_map[component_type_index];
                    ASSERT(local_component_index != 0xFFFF && local_component_index < m_objects[object_type_index].m_num_components);
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
                    ASSERT(local_component_index != 0xFFFF && local_component_index < m_objects[object_type_index].m_num_components);
                    return m_objects[object_type_index].m_a_component[local_component_index].get_access(index);
                }

                struct object_t
                {
                    binmap_t              m_objects_free_map; // tracks free objects
                    nobject::inventory_t* m_a_component;      // m_a_component[max components for this object], first inventory_t is for object
                    u16*                  m_a_component_map;  // m_a_component_map[m_max_components globally], index 0 is for object
                    u32                   m_max_components;   // max components for this object
                    u32                   m_num_components;   // current number of components for this object, excluding the object itself
                };

                object_t* m_objects;
                alloc_t*  m_allocator;
                u16       m_max_object_types;
                u16       m_max_component_types;
            };

#define DECLARE_OBJECT_TYPE(N) static const u16 s_object_type_index = N
#define DECLARE_TAG_TYPE(N) static const u16 s_tag_type_index = N

        } // namespace nobjects_with_components

    } // namespace nobject
} // namespace ncore

#endif
