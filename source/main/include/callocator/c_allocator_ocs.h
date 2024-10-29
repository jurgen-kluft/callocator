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
    namespace nocs
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
        struct allocator_t
        {
            inline allocator_t() : m_allocator(nullptr), m_objects(nullptr), m_max_object_types(0) {}

            bool setup(alloc_t* allocator, u32 max_object_types);
            void teardown();

            DCORE_CLASS_PLACEMENT_NEW_DELETE

            // --------------------------------------------------------------------------------------------------
            // objects

            // Iterate over objects, first call 'begin' and then 'next' until 'next' returns nullptr
            template <typename T> T* begin() const { return (T*)iterate_objects_begin(T::NOBJECT_OBJECT_INDEX); }
            template <typename T> T* next(T const* iter) const { return (T*)iterate_objects_next(T::NOBJECT_OBJECT_INDEX, iter); }

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
            template <typename C, typename T> bool     has_component(T const* object) const { return has_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }
            template <typename C, typename T> C*       add_component(T* object) { return (C*)add_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }
            template <typename C, typename T> C*       add_component(T const* object) const { return (C*)add_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }
            template <typename C, typename T> C*       get_component(T* object) { return (C*)get_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }
            template <typename C, typename T> C const* get_component(T const* object) const { return (C*)get_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }
            template <typename C, typename T> void     rem_component(T const* object) { rem_cp(T::NOBJECT_OBJECT_INDEX, object, C::NOBJECT_COMPONENT_INDEX); }

            // Get component name
            const char* get_component_name(u32 cp_index) const;

            // Tags
            template <typename T> bool has_tag(T const* object, s16 tg_index) const { return has_tag(T::NOBJECT_OBJECT_INDEX, object, tg_index); }
            template <typename T> void add_tag(T const* object, s16 tg_index) { add_tag(T::NOBJECT_OBJECT_INDEX, object, tg_index); }
            template <typename T> void rem_tag(T const* object, s16 tg_index) { rem_tag(T::NOBJECT_OBJECT_INDEX, object, tg_index); }

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

            void* iterate_objects_begin(u32 object_index) const;
            void* iterate_objects_next(u32 object_index, void const* object_ptr) const;

            bool        has_cp(u32 object_index, void const* object, u32 cp_index) const;
            void*       add_cp(u32 object_index, void* object, u32 cp_index);
            void*       add_cp(u32 object_index, void const* object, u32 cp_index) const;
            void        rem_cp(u32 object_index, void const* object, u32 cp_index);
            void*       get_cp(u32 object_index, void* object, u32 cp_index);
            void const* get_cp(u32 object_index, void const* object, u32 cp_index) const;

            bool has_tag(u32 object_index, void const* object, s16 tg_index) const;
            void add_tag(u32 object_index, void const* object, s16 tg_index);
            void rem_tag(u32 object_index, void const* object, s16 tg_index);
        };
    } // namespace nocs
} // namespace ncore

#endif
