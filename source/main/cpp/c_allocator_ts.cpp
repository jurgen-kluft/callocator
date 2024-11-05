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

        const u32 N = 64;
        struct binnode_t // N is dynamic: 2(32) / 4(64) / 8(128) / 16(256) / 32(512) / 64(1024) nodes
        {
            u64        m_bins[N];
            binnode_t* m_children[N];
        };

        struct binleaf_t // 512 bytes, 4096 bits
        {
            u64 m_bins[64];
        };

        struct bintree_t
        {
            u32        m_size;
            u8         m_levels;
            u8         m_padding;
            u16        m_padding;
            u64        m_l0;
            binnode_t* m_root;
        };

        //         16    = 8 + 16 + 64 + 64 = 152 bits max
        //      64 * 64  = 4096 bits max (num binnodes = 0, num leafnodes = 1)
        //     64 * 4096 = 262144 bits max (num binnodes = 1, num leafnodes = 16)
        //   64 * 64 * 4096 = 16777216 bits max (num binnodes = 64, num leafnodes = 4096)

    } // namespace nts
} // namespace ncore
