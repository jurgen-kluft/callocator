#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "ccore/c_math.h"
#include "ccore/c_vmem.h"
#include "cbase/c_context.h"

#include "callocator/c_allocator_stack.h"

namespace ncore
{
    // Stack allocator
    //
    // The stack allocator is a specialized allocator. You can use it when you are allocating temporary
    // objects that are only used in a limited scope, for example, in a function. Furthermore, it is
    // not thread safe, so there should be one instance of the stack allocator per thread.
    //
    class stack_allocator_t : protected stack_alloc_t
    {
    public:
        stack_allocator_t(vmem_arena_t* arena);
        virtual ~stack_allocator_t() {}

        void          reset();

        DCORE_CLASS_PLACEMENT_NEW_DELETE

        vmem_arena_t* m_arena;
        int_t         m_allocation_count;

    protected:
        virtual void* v_allocate(u32 size, u32 alignment) final;
        virtual void  v_deallocate(void* ptr) final;
        virtual void  v_restore_point(void* point) final;
        virtual void* v_save_point() final;

        friend class stack_alloc_scope_t;
    };

    stack_allocator_t::stack_allocator_t(vmem_arena_t* arena) : m_arena(arena), m_allocation_count(0) {}

    void stack_allocator_t::reset()
    {
        m_arena->reset();
        m_allocation_count = 0;
    }

    void* stack_allocator_t::v_allocate(u32 size, u32 alignment) { return m_arena->commit(size, alignment); }

    void stack_allocator_t::v_deallocate(void* ptr)
    {
        if (ptr == nullptr)
            return;
        ASSERT(m_allocation_count > 0);
        m_allocation_count--;
    }

    void* stack_allocator_t::v_save_point()
    {
        int_t* ptr = (int_t*)m_arena->commit(sizeof(int_t));
        *ptr       = m_allocation_count;
        return ptr;
    }

    void stack_allocator_t::v_restore_point(void* point)
    {
        // Has the user forgotten to deallocate one or more allocations?
        int_t* allocation_count = (int_t*)point;
        ASSERT(m_allocation_count == *allocation_count);
        uint_t pos         = g_ptr_diff_in_bytes(point, m_arena->m_base);
        m_allocation_count = *allocation_count;
        m_arena->restore(pos);
    }

    stack_alloc_t* g_create_stack_allocator(int_t initial_size, int_t reserved_size)
    {
        return nullptr;
    }

    void g_destroy_stack_allocator(stack_alloc_t* allocator)
    {

    }

}; // namespace ncore
