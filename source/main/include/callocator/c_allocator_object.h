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

    typedef u32     nhandle_t;
    const nhandle_t c_invalid_nhandle = 0xFFFFFFFF;
    namespace nobject
    {
        struct array_t // 16 bytes
        {
            inline array_t() : m_memory(nullptr), m_num_max(0), m_sizeof(0) {}
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            byte* m_memory;
            u32   m_sizeof;
            u32   m_num_max;

            void setup(alloc_t* allocator, u32 max_num_components, u32 sizeof_component);
            void teardown(alloc_t* allocator);

            D_INLINE void*       get_access(u32 index) { return &m_memory[index * m_sizeof]; }
            D_INLINE const void* get_access(u32 index) const { return &m_memory[index * m_sizeof]; }
            D_INLINE u32         ptr_to_index(const void* ptr) const { return (u32)(((byte*)ptr - m_memory) / m_sizeof); }

            template <typename T> D_INLINE T* get_access_as(u32 index)
            {
                ASSERT(sizeof(T) <= m_sizeof);
                return (T*)get_access(index);
            }

            template <typename T> D_INLINE const T* get_access_as(u32 index) const
            {
                ASSERT(sizeof(T) <= m_sizeof);
                return (const T*)get_access(index);
            }
        };

        // An inventory is using array_t but it has an additional bit array to mark if an item is used or free.
        struct inventory_t // 24 bytes
        {
            D_INLINE inventory_t() : m_bitarray(nullptr), m_array() {}
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            void setup(alloc_t* allocator, u32 max_num_components, u32 sizeof_component);
            void teardown(alloc_t* allocator);

            D_INLINE void allocate(u32 index)
            {
                ASSERT(is_free(index));
                set_used(index);
            }

            D_INLINE void deallocate(u32 index)
            {
                ASSERT(is_used(index));
                set_free(index);
            }

            D_INLINE void deallocate(void* ptr)
            {
                u32 index = m_array.ptr_to_index(ptr);
                ASSERT(is_used(index));
                set_free(index);
            }

            void free_all();

            template <typename T> D_INLINE void construct(u32 index)
            {
                allocate(index);
                void* ptr = get_access(index);
                new (ptr) T();
            }

            template <typename T> D_INLINE void destruct(u32 index)
            {
                deallocate(index);
                void* ptr = get_access(index);
                ((T*)ptr)->~T();
            }

            D_INLINE bool        is_free(u32 index) const { return (m_bitarray[index >> 5] & (1 << (index & 31))) == 0; }
            D_INLINE bool        is_used(u32 index) const { return (m_bitarray[index >> 5] & (1 << (index & 31))) != 0; }
            D_INLINE void        set_free(u32 index) { m_bitarray[index >> 5] &= ~(1 << (index & 31)); }
            D_INLINE void        set_used(u32 index) { m_bitarray[index >> 5] |= (1 << (index & 31)); }
            D_INLINE void*       get_access(u32 index) { return m_array.get_access(index); }
            D_INLINE const void* get_access(u32 index) const { return m_array.get_access(index); }
            D_INLINE u32         ptr_to_index(const void* ptr) const { return m_array.ptr_to_index(ptr); }

            template <typename T> D_INLINE T* get_access_as(u32 index)
            {
                ASSERT(sizeof(T) <= m_array.m_sizeof);
                return (T*)get_access(index);
            }

            template <typename T> D_INLINE T* get_access_as(u32 index) const
            {
                ASSERT(sizeof(T) <= m_array.m_sizeof);
                return (T const*)get_access(index);
            }

            u32*    m_bitarray;
            array_t m_array;
        };

        struct pool_t // 48 bytes
        {
            inline pool_t() : m_object_array(), m_free_resource_map() {}
            DCORE_CLASS_PLACEMENT_NEW_DELETE

            void setup(array_t& object_array, alloc_t* allocator);
            void teardown(alloc_t* allocator);
            void free_all();

            D_INLINE u32 allocate()
            {
                s32 const index = m_free_resource_map.find_and_set();
                ASSERTS(index >= 0, "Error: no more resources left!");
                return index;
            }

            void deallocate(u32 index) { m_free_resource_map.set_free(index); }

            template <typename T> D_INLINE u32 construct()
            {
                const u32 index = allocate();
                void*     ptr   = get_access(index);
                new (ptr) T();
                return index;
            }

            template <typename T> D_INLINE void destruct(u32 index)
            {
                void* ptr = get_access(index);
                ((T*)ptr)->~T();
                deallocate(index);
            }

            D_INLINE u32 ptr_to_index(const void* ptr) const { return m_object_array.ptr_to_index(ptr); }

            D_INLINE void* get_access(u32 index)
            {
                ASSERT(index != c_invalid_nhandle);
                ASSERTS(m_free_resource_map.is_used(index), "Error: resource is not marked as being in use!");
                return &m_object_array.m_memory[index * m_object_array.m_sizeof];
            }

            D_INLINE const void* get_access(u32 index) const
            {
                ASSERT(index != c_invalid_nhandle);
                ASSERTS(m_free_resource_map.is_used(index), "Error: resource is not marked as being in use!");
                return &m_object_array.m_memory[index * m_object_array.m_sizeof];
            }

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

        namespace nobjects_with_components
        {

#define D_DECLARE_OBJECT_TYPE(_index) \
    enum                              \
    {                                 \
        NOBJECT_OBJECT_INDEX = _index \
    }
#define D_DECLARE_COMPONENT_TYPE(_index) \
    enum                                 \
    {                                    \
        NOBJECT_COMPONENT_INDEX = _index \
    }
            struct pool_t
            {
                inline pool_t() : m_allocator(nullptr), m_objects(nullptr), m_max_object_types(0) {}

                bool setup(alloc_t* allocator, u32 max_object_types);
                void teardown();

                DCORE_CLASS_PLACEMENT_NEW_DELETE

                // --------------------------------------------------------------------------------------------------
                // objects

                // Iterate over objects, first call 'begin' and then 'next' until 'next' returns nullptr
                template <typename T> T* begin() const
                {
                    // todo
                }
                template <typename T> T* next(T const* iter) const
                {
                    // todo
                }

                // Iterate over components of an object type, first call 'begin' and then 'next' until 'next' returns nullptr
                template <typename T, typename U> U* begin() const
                {
                    // todo
                }
                template <typename T, typename U> U* next(U* component) const
                {
                    // todo
                }

                // Register object / component
                template <typename T> bool             register_object(u32 max_object_instances, u32 max_components, u32 max_tags) { return register_object(T::NOBJECT_OBJECT_INDEX, sizeof(T), max_object_instances, max_components, max_tags); }
                template <typename T> bool             is_object_registered() const { return T::NOBJECT_OBJECT_INDEX < m_max_object_types && m_objects[T::NOBJECT_OBJECT_INDEX] != nullptr; }
                template <typename T, typename C> bool register_component(u32 max_component_instances, const char* name) { return register_component(T::NOBJECT_OBJECT_INDEX, max_component_instances, C::NOBJECT_COMPONENT_INDEX, sizeof(C), alignof(C), name); }
                template <typename T, typename C> bool is_component_registered() const { return is_component_registered(T::NOBJECT_OBJECT_INDEX, C::NOBJECT_COMPONENT_INDEX); }

                // Create and destroy objects
                template <typename T> T*   create_object() { return (T*)create_object(T::NOBJECT_OBJECT_INDEX); }
                template <typename T> void destroy_object(T* e) { destroy_object(T::NOBJECT_OBJECT_INDEX, e); }

                // Get number of object instances
                template <typename T> u32 get_number_of_instances() const { return get_number_of_instances(T::NOBJECT_OBJECT_INDEX); }

                // Components
                template <typename C, typename T> bool has_component(T* object) const { return has_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }
                template <typename C, typename T> C*   add_component(T* object) { return (C*)add_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }
                template <typename C, typename T> C*   get_component(T* object) { return (C*)get_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }
                template <typename C, typename T> void rem_component(T* object) { rem_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }

                // Tags
                template <typename T> bool has_tag(T* object, s16 tg_index) const { return has_tg(T::NOBJECT_OBJECT_INDEX, object, tg_index); }
                template <typename T> void add_tag(T* object, s16 tg_index) { return add_tg(T::NOBJECT_OBJECT_INDEX, object, tg_index); }
                template <typename T> void rem_tag(T* object, s16 tg_index) { return rem_tg(T::NOBJECT_OBJECT_INDEX, object, tg_index); }

                struct object_t;

            private:
                alloc_t*   m_allocator;
                object_t** m_objects;
                u32        m_max_object_types;

                bool register_object(u32 object_index, u32 sizeof_object, u32 max_object_instances, u32 max_components, u32 max_tags);
                bool register_component(u32 object_index, u32 max_components, u32 cp_index, s32 cp_sizeof, s32 cp_alignof, const char* cp_name);
                bool is_component_registered(u32 object_index, u32 cp_index) const;

                void* create_object(s16 cp_index);
                void  destroy_object(s16 cp_index, void* object);
                u32   get_number_of_instances(s16 cp_index) const;

                bool  has_cp(u32 object_index, void* object, u32 cp_index) const;
                void* add_cp(u32 object_index, void* object, u32 cp_index);
                void  rem_cp(u32 object_index, void* object, u32 cp_index);
                void* get_cp(u32 object_index, void* object, u32 cp_index);

                bool has_tag(u32 object_index, void* object, s16 tg_index) const;
                void add_tag(u32 object_index, void* object, s16 tg_index);
                void rem_tag(u32 object_index, void* object, s16 tg_index);
            };

        } // namespace nobjects_with_components

    } // namespace nobject
} // namespace ncore

#endif
