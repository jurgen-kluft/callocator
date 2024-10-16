#ifndef __C_STRING_ALLOCATOR_H__
#define __C_STRING_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"
#include "cbase/c_runes.h"

namespace ncore
{
    namespace nstring
    {
        typedef u32 id_t;

        class storage_t
        {
        public:
            virtual ~storage_t() {}

            id_t     put(crunes_t const& str) { return v_put(str); }
            crunes_t get(id_t ptr) const { return v_get(ptr); }

        protected:
            virtual id_t     v_put(crunes_t const& str) = 0;
            virtual crunes_t v_get(id_t ptr) const      = 0;
        };

        // Create a string allocator that is backed by a single fixed memory block.
        // The allocator stores all strings as UTF-8.
        storage_t* g_create_storage_utf8(void* mem, size_t mem_size);
        void g_destroy_storage(storage_t* storage);

        // Note: It is very straightforward to implement a string allocator that is backed by a
        // virtual memory system. This would allow the allocator to grow and shrink as needed.

    } // namespace nstring

}; // namespace ncore

#endif /// __C_TLSF_ALLOCATOR_H__
