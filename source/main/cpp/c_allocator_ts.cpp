#include "ccore/c_memory.h"
#include "ccore/c_qsort.h"
#include "ccore/c_allocator.h"

#include "callocator/c_allocator_ts.h"

namespace ncore
{
    namespace nts
    {
        struct entry_t
        {
            PkTime      alloc_time;
            PkTime      free_time;
            PkAllocSize alloc_size;
            u32         index;
        };

        static s8 s_sort_entries(const void* a, const void* b, const void* user_data)
        {
            entry_t const* const lhs = (entry_t const* const)a;
            entry_t const* const rhs = (entry_t const* const)b;
            if (lhs->free_time < rhs->free_time)
                return -1;
            if (lhs->free_time > rhs->free_time)
                return 1;
            PkDuration const ad = lhs->free_time - lhs->alloc_time;
            PkDuration const bd = rhs->free_time - rhs->alloc_time;
            if (ad < bd)
                return -1;
            if (ad > bd)
                return 1;
            return 0;
        }

        void process_sequence(PkDuration sequence_duration, allocation_t const* const allocations, u32 num_allocations, alloc_t* allocator, u32* out_addresses)
        {
            // Note: It is best if the incoming allocator is a 'stack' allocator
            // sort the allocations by free_time and duration

            // create a mapping array instead of sorting the original array
            entry_t* entries = g_allocate_array_and_clear<entry_t>(allocator, num_allocations);
            for (u32 i = 0; i < num_allocations; ++i)
            {
                entries[i].alloc_time = allocations[i].alloc_time;
                entries[i].free_time  = allocations[i].free_time;
                entries[i].alloc_size = allocations[i].alloc_size;
                entries[i].index      = i;
            }
            g_qsort(entries, num_allocations, sizeof(entry_t), s_sort_entries);
        }

    } // namespace nts
} // namespace ncore
