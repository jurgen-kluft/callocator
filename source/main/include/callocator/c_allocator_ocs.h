#ifndef __C_ALLOCATOR_OBJECT_COMPONENT_H__
#define __C_ALLOCATOR_OBJECT_COMPONENT_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_debug.h"
#include "ccore/c_allocator.h"

namespace ncore
{
    class alloc_t;

    namespace nocs
    {
#define D_OCS_OBJECT static u16 const __ocs_object__
#define D_OCS_OBJECT_SET(i) static u16 const __ocs_object__ = i
#define D_OCS_COMPONENT static u16 const __ocs_component__
#define D_OCS_COMPONENT_SET(i) static u16 const __ocs_component__ = i

        struct allocator_t
        {
            inline allocator_t() : m_allocator(nullptr), m_objects(nullptr), m_max_object_types(0) {}

            bool setup(alloc_t* allocator, u16 max_object_types);
            void teardown();

            // --------------------------------------------------------------------------------------------------
            // objects

            // Iterate over objects, first call 'begin' and then 'next' until 'next' returns nullptr
            template <typename T> T* begin() const { return (T*)iterate_objects_begin(T::__ocs_object__); }
            template <typename T> T* next(T const* iter) const { return (T*)iterate_objects_next(T::__ocs_object__, iter); }

            // Register object / component
            template <typename T> bool             register_object(u16 max_object_instances, u16 max_components, u16 max_tags) { return register_object(T::__ocs_object__, sizeof(T), max_object_instances, max_components, max_tags); }
            template <typename T> bool             is_object_registered() const { return T::__ocs_object__ < m_max_object_types && m_objects[T::__ocs_object__] != nullptr; }
            template <typename T, typename C> bool register_component(u16 max_component_instances, const char* name) { return register_component(T::__ocs_object__, max_component_instances, C::__ocs_component__, sizeof(C), alignof(C), name); }
            template <typename T, typename C> bool is_component_registered() const { return is_component_registered(T::__ocs_object__, C::__ocs_component__); }

            // Create and destroy objects
            template <typename T> T* create_object()
            {
                void* mem = create_object(T::__ocs_object__);
                return new (mem) T();
            }
            template <typename T> void destroy_object(T* e)
            {
                e->~T();
                destroy_object(T::__ocs_object__, e);
            }

            // Get number of object instances
            template <typename T> u16 get_number_of_instances() const { return get_number_of_instances(T::__ocs_object__); }

            // Objects
            template <typename T, typename C> T* get_object(C const* component) const { return get_object(T::__ocs_object__, C::__ocs_component__, component); }

            // Components
            template <typename C, typename T> bool has_component(T const* object) const { return has_cp(T::__ocs_object__, object, C::__ocs_component__); }
            template <typename C, typename T> C*   add_component(T const* object)
            {
                void* mem = add_cp(T::__ocs_object__, object, C::__ocs_component__);
                return new (mem) C();
            }
            template <typename C, typename T> C*       get_component(T* object) { return (C*)get_cp(T::__ocs_object__, object, C::__ocs_component__); }
            template <typename C, typename T> C const* get_component(T const* object) const { return (C*)get_cp(T::__ocs_object__, object, C::__ocs_component__); }
            template <typename C, typename T> void     rem_component(T const* object)
            {
                C* cp = rem_cp(T::__ocs_object__, object, C::__ocs_component__);
                if (cp)
                {
                    cp->~C();
                }
            }

            // Component -> Component
            template <typename T, typename C1, typename C2> bool has_component(C1 const* cp1) { return get_cp(T::__ocs_object__, C1::__ocs_component__, cp1, C2::__ocs_component__) != nullptr; }
            template <typename T, typename C1, typename C2> C2*  add_component(C1 const* cp1) const
            {
                void* mem = add_cp(T::__ocs_object__, C1::__ocs_component__, cp1, C2::__ocs_component__);
                return new (mem) C2();
            }
            template <typename T, typename C1, typename C2> C2*       get_component(C1 const* cp1) { return (C2*)get_cp(T::__ocs_object__, C1::__ocs_component__, cp1, C2::__ocs_component__); }
            template <typename T, typename C1, typename C2> C2 const* get_component(C1 const* cp1) const { return (C2*)get_cp(T::__ocs_object__, C1::__ocs_component__, cp1, C2::__ocs_component__); }
            template <typename T, typename C1, typename C2> void      rem_component(C1 const* cp1)
            {
                C2* cp = (C2*)rem_cp(T::__ocs_object__, C1::__ocs_component__, cp1, C2::__ocs_component__);
                if (cp)
                {
                    cp->~C2();
                }
            }

            // Tags
            template <typename T> bool has_tag(T const* object, u16 tg_index) const { return has_tag(T::__ocs_object__, object, 0xFFFF, nullptr, tg_index); }
            template <typename T> void add_tag(T const* object, u16 tg_index) { add_tag(T::__ocs_object__, object, 0xFFFF, nullptr, tg_index); }
            template <typename T> void rem_tag(T const* object, u16 tg_index) { rem_tag(T::__ocs_object__, object, 0xFFFF, nullptr, tg_index); }

            template <typename T, typename C> bool has_tag(T const* object, C const* component, u16 tg_index) const { return has_tag(T::__ocs_object__, object, C::__ocs_component__, component, tg_index); }
            template <typename T, typename C> void add_tag(T const* object, C const* component, u16 tg_index) { add_tag(T::__ocs_object__, object, C::__ocs_component__, component, tg_index); }
            template <typename T, typename C> void rem_tag(T const* object, C const* component, u16 tg_index) { rem_tag(T::__ocs_object__, object, C::__ocs_component__, component, tg_index); }

            struct object_t;

            DCORE_CLASS_PLACEMENT_NEW_DELETE

        private:
            alloc_t*   m_allocator;
            object_t** m_objects;
            u32        m_max_object_types;

            bool register_object(u16 object_index, u32 sizeof_object, u16 max_object_instances, u16 max_components, u16 max_tags);
            bool register_component(u16 object_index, u16 max_components, u16 cp_index, u32 cp_sizeof, u32 cp_alignof);
            bool is_component_registered(u16 object_index, u16 cp_index) const;

            void* create_object(u16 cp_index);
            void  destroy_object(u16 cp_index, void* object);
            u16   get_number_of_instances(u16 cp_index) const;

            void* iterate_objects_begin(u16 object_index) const;
            void* iterate_objects_next(u16 object_index, void const* object_ptr) const;

            void* get_object(u16 object_index, u16 component_index, void const* component);

            bool        has_cp(u16 object_index, void const* object, u16 cp_index) const;
            bool        has_cp(u16 object_index, u16 cp1_index, void const* cp1, u16 cp2_index) const;
            void*       add_cp(u16 object_index, void const* object, u16 cp_index);
            void*       add_cp(u16 object_index, u16 cp1_index, void const* cp1, u16 cp2_index);
            void*       rem_cp(u16 object_index, void const* object, u16 cp_index);
            void*       rem_cp(u16 object_index, u16 cp1_index, void const* cp1, u16 cp2_index);
            void*       get_cp(u16 object_index, void* object, u16 cp_index);
            void const* get_cp(u16 object_index, void const* object, u16 cp_index) const;
            void*       get_cp(u16 object_index, u16 cp1_index, void* cp1, u16 cp2_index);
            void const* get_cp(u16 object_index, u16 cp1_index, void const* cp1, u16 cp2_index) const;

            bool has_tag(u16 object_index, void const* object, u16 component_index, void const* component, u16 tg_index) const;
            void add_tag(u16 object_index, void const* object, u16 component_index, void const* component, u16 tg_index);
            void rem_tag(u16 object_index, void const* object, u16 component_index, void const* component, u16 tg_index);
        };
    } // namespace nocs
} // namespace ncore

#endif
