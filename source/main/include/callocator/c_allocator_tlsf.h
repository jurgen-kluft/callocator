#ifndef __C_TLSF_ALLOCATOR_H__
#define __C_TLSF_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

namespace ncore
{
    /// Forward declares
    class alloc_t;

    /// A custom allocator; 'Two-Level Segregate Fit' allocator
    extern alloc_t* gCreateTlsfAllocator(void* mem, u32 memsize);

}; // namespace ncore

#endif /// __C_TLSF_ALLOCATOR_H__
