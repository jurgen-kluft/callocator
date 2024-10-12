#ifndef __C_TLSF_ALLOCATOR_H__
#define __C_TLSF_ALLOCATOR_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "ccore/c_allocator.h"

namespace ncore
{
    alloc_t* g_create_tlsf(void* mem, int_t mem_size);

}; // namespace ncore

#endif /// __C_TLSF_ALLOCATOR_H__
