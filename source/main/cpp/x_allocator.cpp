#include "xbase/x_target.h"

#include "xallocator/x_allocator.h"
#include "xallocator/x_allocator_tlsf.h"

namespace xcore
{
    alloc_t* gCreateHeapAllocator(void* mem_begin, u32 mem_size) { return gCreateTlsfAllocator(mem_begin, mem_size); }

}; // namespace xcore
