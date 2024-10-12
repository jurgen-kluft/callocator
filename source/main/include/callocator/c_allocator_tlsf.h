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
        struct tlsf_block_t;

        enum ELevelCounts
        {
#if __SIZE_WIDTH__ == 64
            TLSF_SL_COUNT = 16,
            TLSF_FL_COUNT = 32,
            TLSF_FL_MAX   = 38,
#else
            TLSF_SL_COUNT = 16,
            TLSF_FL_COUNT = 25,
            TLSF_FL_MAX   = 30,
#endif
        };
    } // namespace ntlsf

    class tlsf_alloc_t : public alloc_t
    {
    public:
        u32                  fl, sl[ntlsf::TLSF_FL_COUNT];
        ntlsf::tlsf_block_t* block[ntlsf::TLSF_FL_COUNT][ntlsf::TLSF_SL_COUNT];
        u64                  size;

        tlsf_alloc_t();
        virtual ~tlsf_alloc_t();

        virtual void* v_allocate(u32 size, u32 alignment) final;
        virtual void  v_deallocate(void* ptr) final;

        virtual bool  v_check(const char*& error_description) const;
        virtual void* v_resize(u64 size) = 0;

        DCORE_CLASS_PLACEMENT_NEW_DELETE
    };

    // Create a TLSF allocator that is backed by a single fixed memory block.
    alloc_t* g_create_tlsf(void* mem, int_t mem_size);

    // Note: It is very straightforward to implement a TLSF allocator that is backed by a
    // virtual memory system. This would allow the allocator to grow and shrink as needed.

}; // namespace ncore

#endif /// __C_TLSF_ALLOCATOR_H__
