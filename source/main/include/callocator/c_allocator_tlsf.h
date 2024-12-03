#ifndef __C_TLSF_ALLOCATOR_H__
#define __C_TLSF_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    namespace ntlsf
    {
        struct block_t;
        struct context_t;

        class allocator_t : public alloc_t
        {
        public:
            allocator_t(context_t* context);
            virtual ~allocator_t();

            virtual void* v_allocate(u32 size, u32 alignment) final;
            virtual void  v_deallocate(void* ptr) final;

            virtual bool  v_check(const char*& error_description) const;
            virtual void* v_resize(u64 size) = 0;

            DCORE_CLASS_PLACEMENT_NEW_DELETE
            context_t* m_context;
        };

    } // namespace ntlsf

    // Create a TLSF allocator that is backed by a single fixed memory block.
    alloc_t* g_create_tlsf(void* mem, int_t mem_size);

    // Note: It is very straightforward to implement a TLSF allocator that is backed by a
    // virtual memory system. This would allow the allocator to grow and shrink as needed.

}; // namespace ncore

#endif /// __C_TLSF_ALLOCATOR_H__
