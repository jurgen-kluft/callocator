#ifndef __C_DL_ALLOCATOR_H__
#define __C_DL_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif
namespace ncore
{
    /// Forward declares
    class alloc_t;
    class dlmalloc_mem_heap_t;

    class dlmalloc_t : public alloc_t
    {
        dlmalloc_mem_heap_t* m_heap;

    public:
        void init(void* mem, s32 mem_size);
        void exit();

        virtual void* v_allocate(u32 size, u32 alignment);
        virtual void v_deallocate(void* ptr);

        void* operator new(uint_t num_bytes) { return nullptr; }
        void* operator new(uint_t num_bytes, void* mem) { return mem; }
        void  operator delete(void* pMem) {}
        void  operator delete(void* pMem, void*) {}
    };

}; // namespace ncore

#endif /// __C_DL_ALLOCATOR_H__
