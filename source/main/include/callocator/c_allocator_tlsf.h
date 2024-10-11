#ifndef __C_TLSF_ALLOCATOR_H__
#define __C_TLSF_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    class tlsf_alloc_t : public alloc_t
    {
        void*  mPool;
        int_t mPoolSize;

    public:
        tlsf_alloc_t();
        virtual ~tlsf_alloc_t() {}

        void init(void* mem, int_t mem_size);

        virtual void* v_allocate(u32 size, u32 alignment);
        virtual void v_deallocate(void* ptr);

        void* operator new(uint_t num_bytes) { return nullptr; }
        void* operator new(uint_t num_bytes, void* mem) { return mem; }
        void  operator delete(void* pMem) {}
        void  operator delete(void* pMem, void*) {}
    };

}; // namespace ncore

#endif /// __C_TLSF_ALLOCATOR_H__
