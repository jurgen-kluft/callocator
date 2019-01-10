#ifndef __X_VIRTUAL_MEMORY_INTERFACE_H__
#define __X_VIRTUAL_MEMORY_INTERFACE_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

namespace xcore
{
    struct xpage_info;

    class xvirtual_memory
    {
    public:
        virtual bool reserve(u64 address_range, u32 page_size, u32 attributes, void*& baseptr) = 0;
        virtual bool release(void* baseptr)                                                    = 0;

        virtual bool commit(void* page_address, u32 page_count)   = 0;
        virtual bool decommit(void* page_address, u32 page_count) = 0;
    };

    extern xvirtual_memory* gGetVirtualMemory();

    class xpage_alloc
    {
    public:
        virtual void* allocate(u32& size) = 0;
        virtual void  deallocate(void*)   = 0;

        virtual bool info(void* ptr, void*& page_addr, xpage_info *& page_info) = 0;

        virtual void release() = 0;
    };

    xpage_alloc* gCreateVMemPageAllocator(xalloc* allocator, u64 address_range, u32 page_size, u32 page_attrs,
                                       u32 protection_attrs, xvirtual_memory* vmem);

}; // namespace xcore

#endif /// __X_VIRTUAL_MEMORY_INTERFACE_H__