#include "ccore/c_target.h"
#include "ccore/c_allocator.h"
#include "ccore/c_memory.h"
#include "ccore/c_math.h"
#include "ccore/c_arena.h"
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
    class stack_allocator_t : public stack_alloc_t
    {
    public:
        DCORE_CLASS_PLACEMENT_NEW_DELETE

        stack_allocator_t(arena_t* arena);
        virtual ~stack_allocator_t() {}

        void reset();

        arena_t* m_arena;
        void*    m_save_address;
        int_t    m_allocation_count;

    protected:
        virtual void* v_allocate(u32 size, u32 alignment) final;
        virtual void  v_deallocate(void* ptr) final;
        virtual void  v_restore_point(void* point) final;
        virtual void* v_save_point() final;

        friend class stack_alloc_scope_t;
    };

    stack_allocator_t::stack_allocator_t(arena_t* arena) : m_arena(arena), m_allocation_count(0) {}

    void stack_allocator_t::reset()
    {
        narena::restore_address(m_arena, m_save_address);
        m_allocation_count = 0;
    }

    void* stack_allocator_t::v_allocate(u32 size, u32 alignment)
    {
        m_allocation_count++;
        return narena::alloc(m_arena, size, alignment);
    }

    void stack_allocator_t::v_deallocate(void* ptr)
    {
        if (ptr == nullptr)
            return;
        ASSERT(m_allocation_count > 0);
        m_allocation_count--;
    }

    void* stack_allocator_t::v_save_point()
    {
        int_t* ptr = (int_t*)narena::alloc(m_arena, sizeof(int_t));
        *ptr       = m_allocation_count;
        return ptr;
    }

    void stack_allocator_t::v_restore_point(void* point)
    {
        // Has the user forgotten to deallocate one or more allocations?
        int_t* allocation_count = (int_t*)point;
        ASSERT(m_allocation_count == *allocation_count);
        m_allocation_count = *allocation_count;
        narena::restore_address(m_arena, point);
    }

    stack_alloc_t* g_create_stack_allocator(int_t initial_size, int_t reserved_size)
    {
        arena_t*           arena     = narena::new_arena(reserved_size, initial_size);
        void*              mem       = narena::alloc(arena, sizeof(stack_allocator_t));
        stack_allocator_t* allocator = new (mem) stack_allocator_t(arena);
        allocator->m_save_address    = narena::current_address(arena);
        return allocator;
    }

    void g_destroy_stack_allocator(stack_alloc_t* allocator)
    {
        if (allocator == nullptr)
            return;

        stack_allocator_t* impl  = static_cast<stack_allocator_t*>(allocator);
        arena_t*           arena = impl->m_arena;
        narena::destroy(arena);
    }

}; // namespace ncore
