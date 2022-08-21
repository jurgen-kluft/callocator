#ifndef __C_DL_ALLOCATOR_H__
#define __C_DL_ALLOCATOR_H__
#include "cbase/c_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif
namespace ncore
{
    /// Forward declares
    class alloc_t;

    /// A custom allocator; Doug Lea malloc
    extern alloc_t* gCreateDlAllocator(void* mem, u32 memsize);

}; // namespace ncore

#endif /// __C_DL_ALLOCATOR_H__
