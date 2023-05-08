#include "ccore/c_target.h"

#include "callocator/c_allocator.h"
#include "callocator/c_allocator_tlsf.h"

namespace ncore
{
    alloc_t* gCreateHeapAllocator(void* mem_begin, u32 mem_size) { return gCreateTlsfAllocator(mem_begin, mem_size); }

}; // namespace ncore
