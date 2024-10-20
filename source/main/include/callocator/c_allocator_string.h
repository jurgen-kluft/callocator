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
        struct str_t
        {
            utf8::pcrune m_str;
            u32          m_hash;
            u32          m_len;
        };
    } // namespace nstring

    typedef nstring::str_t const* cpstr_t;

    namespace nstring
    {
        class storage_t
        {
        public:
            virtual ~storage_t() {}

            cpstr_t put(const char* str) { return v_put(str); }
            cpstr_t put(crunes_t const& str) { return v_put(str); }

        protected:
            virtual str_t const* v_put(const char* str)     = 0;
            virtual str_t const* v_put(crunes_t const& str) = 0;
        };

        // Create a string allocator that is backed by a single fixed memory block.
        storage_t* g_create_storage_ascii(void* mem, size_t mem_size); // This allocator stores all strings as ASCII.
        storage_t* g_create_storage_utf8(void* mem, size_t mem_size);  // This allocator stores all strings as UTF-8.
        void       g_destroy_storage(storage_t* storage);

        // Note: It is very straightforward to implement a string allocator that is backed by a
        // virtual memory system. This would allow the allocator to grow and shrink as needed.

    } // namespace nstring

}; // namespace ncore

#endif /// __C_TLSF_ALLOCATOR_H__
