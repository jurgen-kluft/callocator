#ifndef __CALLOCATOR_SEGMENTED_H__
#define __CALLOCATOR_SEGMENTED_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE
#    pragma once
#endif

#include "cbase/c_allocator.h"

namespace ncore
{
    namespace nsegmented
    {
        class segment_alloc_t
        {
        public:
            inline bool allocate(s64 size, s64& out_ptr) { return v_allocate(size, out_ptr); }
            inline bool deallocate(s64 ptr, s64 size) { return v_deallocate(ptr, size); }

        protected:
            virtual bool v_allocate(s64 size, s64& out_ptr) = 0;
            virtual bool v_deallocate(s64 ptr, s64 size)    = 0;
        };

        segment_alloc_t* g_create_segment_n_allocator(alloc_t* allocator, int_t min_size, int_t max_size, int_t total_size);
        segment_alloc_t* g_create_segment_b_allocator(alloc_t* allocator, int_t min_size, int_t max_size, int_t total_size);
        void             g_teardown(alloc_t* alloc, segment_alloc_t* allocator);

    } // namespace nsegmented
} // namespace ncore

#endif
