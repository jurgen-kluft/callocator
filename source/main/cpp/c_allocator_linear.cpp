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
        void*    m_save_address;

    private:
        virtual void* v_allocate(u32 size, u32 alignment) final;
        virtual void  v_deallocate(void* ptr) final;
        virtual void  v_reset() final;
    };

    linear_alloc_imp_t::~linear_alloc_imp_t()
    {
        narena::release(m_arena);
        m_arena = nullptr;
    }

    void linear_alloc_imp_t::v_reset()
    {
#ifdef TARGET_DEBUG
        void* const current_pos = narena::current_address(m_arena);
        if (current_pos > m_save_address)
        {
            nmem::memset(m_save_address, 0xCD, (byte const*)current_pos - (byte const*)m_save_address);
        }
#endif

        narena::restore_address(m_arena, m_save_address);
    }

    void* linear_alloc_imp_t::v_allocate(u32 size, u32 alignment)
    {
        if (size == 0)
            return nullptr;
        return narena::alloc(m_arena, size, alignment);
    }

    void linear_alloc_imp_t::v_deallocate(void* ptr) {}

    linear_alloc_t* g_create_linear_allocator(int_t initial_size, int_t reserved_size)
    {
        arena_t*            arena     = narena::create(reserved_size, initial_size);
        void*               mem       = narena::alloc(arena, sizeof(linear_alloc_imp_t));
        linear_alloc_imp_t* allocator = new (mem) linear_alloc_imp_t(arena);
        allocator->m_save_address     = narena::current_address(arena);
        return allocator;
    }

    int_t g_current_size(linear_alloc_t* allocator)
    {
        if (allocator == nullptr)
            return 0;

        linear_alloc_imp_t* impl  = static_cast<linear_alloc_imp_t*>(allocator);
        arena_t*            arena = impl->m_arena;
        return (byte const*)narena::current_address(arena) - (byte const*)impl->m_save_address;
    }

    void g_destroy_allocator(linear_alloc_t* allocator)
    {
        if (allocator == nullptr)
            return;

        linear_alloc_imp_t* impl  = static_cast<linear_alloc_imp_t*>(allocator);
        arena_t*            arena = impl->m_arena;
        narena::release(arena);
    }

}; // namespace ncore
