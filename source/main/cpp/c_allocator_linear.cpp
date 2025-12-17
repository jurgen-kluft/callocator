#include "ccore/c_target.h"
#include "ccore/c_memory.h"
#include "ccore/c_vmem.h"

#include "callocator/c_allocator_linear.h"

namespace ncore
{
    class linear_alloc_imp_t : public linear_alloc_t
    {
    public:
        inline linear_alloc_imp_t(arena_t* arena) : m_arena(arena) {}
        virtual ~linear_alloc_imp_t();

        DCORE_CLASS_PLACEMENT_NEW_DELETE

        arena_t* m_arena;
        s64      m_base_pos;

    private:
        virtual void* v_allocate(u32 size, u32 alignment) final;
        virtual void  v_deallocate(void* ptr) final;
        virtual void  v_reset() final;
    };

    linear_alloc_imp_t::~linear_alloc_imp_t()
    {
        m_arena->release();
        m_arena = nullptr;
    }

    void linear_alloc_imp_t::v_reset()
    {
#ifdef TARGET_DEBUG
        s64 const current_pos = m_arena->save_point();
        if (current_pos > m_base_pos)
        {
            void* mem = m_arena->m_base + m_base_pos;
            nmem::memset(mem, 0xCD, current_pos - m_base_pos);
        }
#endif

        m_arena->restore_point(m_base_pos);
    }

    void* linear_alloc_imp_t::v_allocate(u32 size, u32 alignment)
    {
        if (size == 0)
            return nullptr;
        return m_arena->commit(size, alignment);
    }

    void linear_alloc_imp_t::v_deallocate(void* ptr) {}

    linear_alloc_t* g_create_linear_allocator(int_t initial_size, int_t reserved_size)
    {
        arena_t a;
        a.reserved(reserved_size);
        a.committed(initial_size);

        arena_t* arena = (arena_t*)a.commit(sizeof(arena_t));
        *arena         = a;

        void*               mem       = arena->commit_and_zero(sizeof(linear_alloc_imp_t));
        linear_alloc_imp_t* allocator = new (mem) linear_alloc_imp_t(arena);
        allocator->m_base_pos         = arena->save_point();
        return allocator;
    }

    int_t g_current_size(linear_alloc_t* allocator)
    {
        if (allocator == nullptr)
            return 0;

        linear_alloc_imp_t* impl  = static_cast<linear_alloc_imp_t*>(allocator);
        arena_t*            arena = impl->m_arena;
        return arena->save_point() - impl->m_base_pos;
    }

    void g_destroy_allocator(linear_alloc_t* allocator)
    {
        if (allocator == nullptr)
            return;

        linear_alloc_imp_t* impl  = static_cast<linear_alloc_imp_t*>(allocator);
        arena_t*            arena = impl->m_arena;
        arena->release();
    }

}; // namespace ncore
