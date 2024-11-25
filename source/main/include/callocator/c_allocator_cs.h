#ifndef __C_ALLOCATOR_COMPONENTS_H__
#define __C_ALLOCATOR_COMPONENTS_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_debug.h"
#include "ccore/c_allocator.h"

namespace ncore
{
    class alloc_t;

    namespace ncs
    {
#define D_CS_COMPONENT static u16 const __ncs_index__
#define D_CS_COMPONENT_SET(i) static u16 const __ncs_index__ = i

        // TODO: When registering a component, also pass a callback_t that is a destructor for the component
        //       When destroy_instance is called, call the destructor for every component.

        struct allocator_t
        {
            inline allocator_t() : m_object(nullptr) {}

            bool setup(alloc_t* allocator, u32 max_object_instances, u16 max_components, u8 max_tags);
            void teardown();

            // --------------------------------------------------------------------------------------------------
            // instance

            // Iterate over a specific component, all instances, first call 'begin' and then 'next' until 'next' returns nullptr
            template <typename T> T* begin() const { return (T*)iterate_begin(T::__ncs_index__); }
            template <typename T> T* next(T const* iter) const { return (T*)iterate_next(T::__ncs_index__, iter); }

            template <typename T> T* new_instance()
            {
                void* mem = create_instance(T::__ncs_index__);
                return new (mem) T();
            }

            template <typename T> void destroy_instance(T* e)
            {
                e->~T();
                destroy_instance(T::__ncs_index__, e);
            }

            // Components
            template <typename C> bool              register_component(u16 max_components) { return register_component(C::__ncs_index__, max_components, sizeof(C), alignof(C)); }
            template <typename C> bool              is_component_registered() const { return is_component_registered(C::__ncs_index__); }
            template <typename C1, typename C2> C2* create_component(C1 const* cp1)
            {
                void* mem = add_cp(C1::__ncs_index__, cp1, C2::__ncs_index__);
                return new (mem) C2();
            }
            template <typename C1, typename C2> void destroy_component(C1 const* cp1)
            {
                C2* cp = (C2*)rem_cp(C1::__ncs_index__, cp1, C2::__ncs_index__);
                if (cp)
                {
                    cp->~C2();
                }
            }
            template <typename C1, typename C2> bool      has_component(C1 const* cp1) { return get_cp(C1::__ncs_index__, cp1, C2::__ncs_index__) != nullptr; }
            template <typename C1, typename C2> C2*       get_component(C1 const* cp1) { return (C2*)get_cp(C1::__ncs_index__, cp1, C2::__ncs_index__); }
            template <typename C1, typename C2> C2 const* get_component(C1 const* cp1) const { return (C2*)get_cp(C1::__ncs_index__, cp1, C2::__ncs_index__); }

            // Tags
            template <typename C> bool has_tag(C const* cp, u8 tg_index) const { return has_tag(C::__ncs_index__, cp, tg_index); }
            template <typename C> void add_tag(C const* cp, u8 tg_index) { add_tag(C::__ncs_index__, cp, tg_index); }
            template <typename C> void rem_tag(C const* cp, u8 tg_index) { rem_tag(C::__ncs_index__, cp, tg_index); }

            struct object_t;

            DCORE_CLASS_PLACEMENT_NEW_DELETE

        private:
            object_t* m_object;

            bool register_component(u16 cp_index, u32 max_instances, u32 cp_sizeof, u32 cp_alignof);
            bool is_component_registered(u16 cp_index) const;

            void* create_instance(u16 cp_index);
            void  destroy_instance(u16 cp_index, void* cp);
            u32   get_number_of_instances(u16 cp_index) const;

            void* iterate_begin(u16 cp_index) const;
            void* iterate_next(u16 cp1_index, void const* cp1_ptr) const;

            bool        has_cp(u16 cp1_index, void const* cp1_ptr, u16 cp_index) const;
            void*       add_cp(u16 cp1_index, void const* cp1_ptr, u16 cp_index);
            void*       rem_cp(u16 cp1_index, void const* cp1_ptr, u16 cp_index);
            void*       get_cp(u16 cp1_index, void* cp1_ptr, u16 cp_index);
            void const* get_cp(u16 cp1_index, void const* cp1_ptr, u16 cp_index) const;

            bool has_tag(u16 cp1_index, void const* cp1_ptr, u8 tg_index) const;
            void add_tag(u16 cp1_index, void const* cp1_ptr, u8 tg_index);
            void rem_tag(u16 cp1_index, void const* cp1_ptr, u8 tg_index);
        };
    } // namespace ncs
} // namespace ncore

#endif
