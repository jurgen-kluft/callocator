#include "ccore/c_target.h"
#include "ccore/c_memory.h"
#include "ccore/c_vmem.h"

#include "callocator/c_allocator_linear.h"

namespace ncore
{
    linear_alloc_t::~linear_alloc_t()
    {
        m_arena->release();
        m_arena = nullptr;
    }

    void linear_alloc_t::reset()
    {
        m_arena->reset();
    }

    void* linear_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        if (size == 0)
            return nullptr;
        return m_arena->commit(size, alignment);
    }

    void linear_alloc_t::v_deallocate(void* ptr) {}


    linear_alloc_t* g_create_linear_allocator(int_t initial_size, int_t reserved_size)
    {
        vmem_arena_t a;
        a.reserved(reserved_size);
        a.committed(initial_size);

        vmem_arena_t* arena  = (vmem_arena_t*)a.commit(sizeof(vmem_arena_t));
        *arena = a;

        void* mem = arena->commit_and_zero(sizeof(linear_alloc_t));
        linear_alloc_t* allocator = new (mem) linear_alloc_t(arena);
        return allocator;
    }

}; // namespace ncore
