#include "ccore/c_math.h"
#include "ccore/c_memory.h"
#include "ccore/c_qsort.h"
#include "ccore/c_allocator.h"

#include "callocator/c_allocator_ts.h"

namespace ncore
{
    namespace nts
    {
        static s8 s_sort_entries(const void* a, const void* b, const void* user_data)
        {
            allocation_t const* const lhs = (allocation_t const* const)a;
            allocation_t const* const rhs = (allocation_t const* const)b;
            if (lhs->free_time < rhs->free_time)
                return -1;
            if (lhs->free_time > rhs->free_time)
                return 1;
            if (lhs->alloc_size > rhs->alloc_size)
                return -1;
            if (lhs->alloc_size < rhs->alloc_size)
                return 1;
            return 0;
        }

        struct dsnode_t
        {
            u16 m_parent;
        };

        struct bucket_t
        {
            u16 list_tail;
            u16 list_head;
            u16 max_endpoint;
            u16 num_intervals;
        };

        u32 process_sequence(allocation_t* const allocations, u32 num_allocations, alloc_t* allocator)
        {
            // Note: It is best if the incoming allocator is a 'stack' allocator since the memory allocated here is temporary.

            // Sort the intervals by their end point, and then take each interval in end-point order, and put it into the
            // bucket with the largest right-most end point, such that bucket.max_endpoint < interval.startpoint.
            // If there is no such bucket, then you have to start a new one.
            // If you keep the buckets sorted by max_endpoint, then you can find the best one in log(|buckets|) time,
            // for O(N log N) all together.

            // sort by end-point (free-time)
            g_qsort(allocations, num_allocations, sizeof(allocation_t), s_sort_entries);
            dsnode_t* nodes = g_allocate_array_and_memset<dsnode_t>(allocator, num_allocations, 0xFFFFFFFF);

            // Note: we also need to keep buckets sorted by their end-point (free-time)
            bucket_t* buckets     = g_allocate_array_and_clear<bucket_t>(allocator, num_allocations);
            u32       num_buckets = 0;

            for (u32 i = 0; i < num_allocations; ++i)
            {
                allocation_t& interval = allocations[i];

                // Find the bucket with the largest right-most end point, such that bucket.max_endpoint < interval.startpoint
                // Buckets are sorted by max_endpoint, so we can do a binary search to find the right bucket
                s32 found_index = -1;
                if (num_buckets > 0)
                {
                    u32 left  = 0;
                    u32 right = num_buckets;
                    while (left < right)
                    {
                        u32 const mid = left + ((right - left) >> 1);
                        if (interval.alloc_time >= buckets[mid].max_endpoint)
                        {
                            found_index = mid;
                            left        = mid + 1;
                        }
                        else
                        {
                            right = mid;
                        }
                    }
                }

                if (found_index >= 0)
                {
                    // Add the interval to the bucket
                    bucket_t found_bucket = buckets[found_index];

                    nodes[i].m_parent         = found_bucket.list_tail;
                    found_bucket.list_tail    = i;
                    found_bucket.max_endpoint = interval.free_time;
                    found_bucket.num_intervals += 1;

                    // Find the place we need to move this bucket to, do a binary search on the range [found_index+1, num_buckets)]
                    // Note: Since we increased the end-point of the bucket, we may need to move it more to the end of the array.
                    // 'move' the range [found_index+1, place) to [found_index, place-1)] and insert the bucket at place
                    s32 place = found_index;
                    u32 left  = found_index + 1;
                    u32 right = num_buckets;
                    while (left < right)
                    {
                        u32 const mid = left + ((right - left) >> 1);
                        if (found_bucket.max_endpoint > buckets[mid].max_endpoint)
                        {
                            place = mid + 1;
                            left  = mid + 1;
                        }
                        else
                        {
                            right = mid;
                        }
                    }

                    if (place > found_index) // If the bucket is not already in the right place
                    {
                        // Move the range [found_index + 1, place) to [found, place - 1]
                        for (u32 j = found_index + 1; j <= place; ++j)
                            buckets[j - 1] = buckets[j];

                        // Insert the bucket at (place - 1) since the array has been shifted
                        buckets[place - 1] = found_bucket;
                    }
                    else
                    {
                        // The bucket is already in the right place, just update it
                        buckets[found_index] = found_bucket;
                    }
                }
                else
                {
                    // Start a new bucket
                    bucket_t bucket;
                    bucket.list_tail     = i;
                    bucket.list_head     = i;
                    bucket.max_endpoint  = interval.free_time;
                    bucket.num_intervals = 1;

                    // Find the place we need to move this bucket to, do a binary search on the range [0, num_buckets)]
                    // 'move' the range [0, new_place) to [1, new_place+1)] and insert the bucket at new_place
                    u32 left  = 0;
                    u32 right = num_buckets;
                    u32 place = num_buckets;
                    while (left < right)
                    {
                        u32 const mid = left + ((right - left) >> 1);
                        if (bucket.max_endpoint > buckets[mid].max_endpoint)
                        {
                            place = mid + 1;
                            left  = mid + 1;
                        }
                        else
                        {
                            right = mid;
                        }
                    }

                    // Move the range [place, num_buckets) to [place+1, num_buckets+1)
                    for (u32 j = num_buckets; j > place; --j)
                        buckets[j] = buckets[j - 1];

                    // Insert the bucket at place
                    buckets[place] = bucket;

                    // Increment the number of buckets
                    num_buckets += 1;
                }
            }

            // Iterate over all buckets and hand out the addresses
            u32 address = 0;
            for (u32 i = 0; i < num_buckets; ++i)
            {
                bucket_t const& bucket   = buckets[i];
                u32             max_size = 0;
                u16             node     = bucket.list_tail;
                do
                {
                    allocation_t& interval = allocations[node];
                    interval.address       = address;
                    if (interval.alloc_size > max_size)
                        max_size = interval.alloc_size;
                    node = nodes[node].m_parent;
                } while (node != 0xFFFF);
                address += max_size;
            }

            // Deallocate the buckets and nodes array
            g_deallocate(allocator, buckets);
            g_deallocate(allocator, nodes);

            return address;
        }

    } // namespace nts
} // namespace ncore
