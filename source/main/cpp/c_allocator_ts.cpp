#include "ccore/c_memory.h"
#include "ccore/c_allocator.h"

#include "callocator/c_allocator_pk.h"

namespace ncore
{
    namespace nts
    {
        void process_sequence(PkDuration sequence_duration, allocation_t* allocations, u32 num_allocations, alloc_t* allocator, u32* out_addresses)
        {
            // ...
            // sort the allocations by free_time and duration
        }

    } // namespace npk

} // namespace ncore
