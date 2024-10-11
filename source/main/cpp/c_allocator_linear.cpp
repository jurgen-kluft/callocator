#include "ccore/c_target.h"
#include "ccore/c_memory.h"

#include "callocator/c_allocator_linear.h"

namespace ncore
{
    const u32 c_node_state_free   = 0x00000000;
    const u32 c_node_state_alloc  = 0x80000000;
    const u32 c_node_state_locked = 0x80000000;
    const u32 c_node_state_head   = 0x00000000;
    const u32 c_node_state_mask   = 0x80000000;

    struct linear_alloc_t::node_t
    {
        inline void reset()
        {
            m_next = 0;
            m_prev = 0;
        }

        inline void set_state_free() { m_next = (m_next & ~c_node_state_mask) | c_node_state_free; }
        inline void set_state_alloc() { m_next = (m_next & ~c_node_state_mask) | c_node_state_alloc; }
        inline void set_state_locked() { m_prev = (m_prev & ~c_node_state_mask) | c_node_state_locked; }
        inline void set_state_head() { m_prev = (m_prev & ~c_node_state_mask) | c_node_state_head; }

        inline bool is_free() const { return (m_next & c_node_state_mask) == c_node_state_free; }
        inline bool is_alloc() const { return (m_next & c_node_state_mask) == c_node_state_alloc; }
        inline bool is_locked() const { return (m_prev & c_node_state_mask) == c_node_state_locked; }
        inline bool is_head() const { return (m_prev & c_node_state_mask) == c_node_state_head; }

        inline linear_alloc_t::node_t* get_next()
        {
            u32 const offset = m_next & ~c_node_state_mask;
            if (offset == 0)
                return nullptr;
            return this + offset;
        }

        inline void set_next(linear_alloc_t::node_t* next)
        {
            if (next == nullptr)
                next = this;
            u32 const offset = ((u32)(next - this) & ~c_node_state_mask);
            m_next           = (m_next & c_node_state_mask) | offset;
        }

        inline linear_alloc_t::node_t* get_prev()
        {
            u32 const offset = m_prev & ~c_node_state_mask;
            if (offset == 0)
                return nullptr;
            return this - offset;
        }
        inline void set_prev(linear_alloc_t::node_t* prev)
        {
            if (prev == nullptr)
                prev = this;
            u32 const offset = ((u32)(this - prev) & ~c_node_state_mask);
            m_prev           = (m_prev & c_node_state_mask) | offset;
        }

    private:
        u32 m_next; // relative offsets (forwards)
        u32 m_prev; // relative offsets (backwards)
    };

    static inline u32   s_align_u32(u32 size, u32 align) { return (size + (align - 1)) & ~(align - 1); }
    static inline void* s_align_ptr(void* ptr, ptr_t align) { return (void*)(((ptr_t)ptr + (align - 1)) & ~(align - 1)); }

    static inline bool is_pointer_inside(void* ptr, linear_alloc_t::node_t* begin, linear_alloc_t::node_t* end) { return ptr >= (void*)(begin + 1) && ptr < (void*)(end - 1); }

    static linear_alloc_t::node_t* s_reset_cursor(linear_alloc_t::node_t* begin, linear_alloc_t::node_t* end)
    {
        linear_alloc_t::node_t* cursor = begin + 1;
        cursor->reset();
        cursor->set_prev(begin);
        cursor->set_next(end);
        cursor->set_state_head();
        begin->set_next(cursor);
        begin->set_prev(nullptr);
        begin->set_state_free();
        end->set_next(nullptr);
        end->set_prev(cursor);
        end->set_state_locked();
        return cursor;
    }

    linear_alloc_t::linear_alloc_t() : m_buffer_begin(nullptr), m_buffer_cursor(nullptr) {}
    linear_alloc_t::~linear_alloc_t() {}

    void linear_alloc_t::setup(void* _beginAddress, u32 _size)
    {
        // Align to 8 bytes
        void* const beginAddress = s_align_ptr(_beginAddress, (u32)sizeof(node_t));
        void* const endAddress   = (u8*)_beginAddress + _size;
        u32 const   size         = (u8*)endAddress - (u8*)beginAddress;

        m_buffer_begin = (node_t*)beginAddress;
        m_buffer_end   = m_buffer_begin + (size / sizeof(node_t)) - 1;
        m_buffer_begin->reset();
        m_buffer_end->reset();

        m_buffer_begin->set_prev(nullptr);
        m_buffer_begin->set_next(m_buffer_cursor);
        m_buffer_begin->set_state_free();

        m_buffer_end->set_prev(m_buffer_cursor);
        m_buffer_end->set_next(nullptr);
        m_buffer_end->set_state_locked();

        m_buffer_cursor = s_reset_cursor(m_buffer_begin, m_buffer_end);
    }

    bool linear_alloc_t::is_valid() const { return m_buffer_begin != nullptr && m_buffer_cursor < m_buffer_cursor->get_next(); }
    bool linear_alloc_t::is_empty() const { return m_buffer_cursor == m_buffer_begin + 1; }
    void linear_alloc_t::reset() { m_buffer_cursor = s_reset_cursor(m_buffer_begin, m_buffer_end); }

    void* linear_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        if (m_buffer_cursor == m_buffer_end)
            return nullptr;

        ASSERT(m_buffer_cursor->is_head());

        // align the size to a multiple of the node_t size
        size = s_align_u32(size, sizeof(node_t));

        // check if we can allocate this size
        u32 sizeLeft = (m_buffer_end - (m_buffer_cursor + 1)) * sizeof(node_t);
        if (size > sizeLeft)
            return nullptr;

        // alignment, shift cursor forward to handle the alignment request
        // for that we need to update cursor->prev->next
        void* ptr         = (void*)(m_buffer_cursor + 1);
        void* aligned_ptr = s_align_ptr(ptr, alignment);
        if (ptr < aligned_ptr)
        {
            // Move cursor according to the alignment shift
            u32 const offset = (u32)((u8*)aligned_ptr - (u8*)ptr);
            ASSERT(offset >= sizeof(node_t) && ((offset & (sizeof(node_t) - 1)) == 0));
            node_t* adjusted_cursor = m_buffer_cursor + (offset / sizeof(node_t));
            adjusted_cursor->reset();
            m_buffer_cursor->get_prev()->set_next(adjusted_cursor);
            m_buffer_cursor->get_next()->set_prev(adjusted_cursor);
            adjusted_cursor->set_prev(m_buffer_cursor->get_prev());
            adjusted_cursor->set_next(m_buffer_cursor->get_next());
            adjusted_cursor->set_state_head();

            // check again if we can allocate this size
            sizeLeft = (m_buffer_end - (adjusted_cursor + 1)) * sizeof(node_t);
            if (size > sizeLeft)
                return nullptr;

            m_buffer_cursor = adjusted_cursor;
            ptr             = aligned_ptr;
        }

        m_buffer_cursor->set_state_alloc();

        sizeLeft = sizeLeft - size;
        if (sizeLeft <= sizeof(node_t))
        {
            // We are out of memory, we can't create a new node
            m_buffer_cursor->get_prev()->set_next(m_buffer_end);
            m_buffer_cursor->get_next()->set_prev(m_buffer_cursor->get_prev());
            m_buffer_cursor = m_buffer_end;
        }
        else
        {
            node_t* new_cursor = m_buffer_cursor + 1 + (size / sizeof(node_t));
            new_cursor->reset();
            new_cursor->set_next(m_buffer_cursor->get_next());
            new_cursor->set_prev(m_buffer_cursor);
            new_cursor->set_state_head();
            m_buffer_cursor->get_next()->set_prev(new_cursor);
            m_buffer_cursor->set_next(new_cursor);
            m_buffer_cursor = new_cursor;
        }

#ifdef TARGET_DEBUG
        nmem::memset(ptr, 0xCD, size);
#endif

        return ptr;
    }

    void linear_alloc_t::v_deallocate(void* ptr)
    {
        if (ptr == nullptr)
            return;

        ASSERT(is_pointer_inside(ptr, m_buffer_begin, m_buffer_end));

        node_t* node = (node_t*)ptr - 1;
        ASSERT(!node->is_free());

        // remove this node from the chain (merge)
        node_t* prev = node->get_prev();
        node_t* next = node->get_next();

        // always see if we can move the cursor to the most left of the chain
        if (m_buffer_cursor == next)
        {
            if (m_buffer_begin == prev)
            {
                m_buffer_cursor = s_reset_cursor(m_buffer_begin, m_buffer_end);
            }
            else
            {
                // we can move the cursor back to this node that has been freed
                node->set_next(m_buffer_cursor->get_next());
                m_buffer_cursor->get_next()->set_prev(node);
                m_buffer_cursor = node;
                m_buffer_cursor->set_state_head();
            }
        }
        else
        {
            // the node freed is in the middle of the chain, cannot move cursor
            prev->set_next(next);
            next->set_prev(prev);
            node->set_state_free();
            node->set_next(nullptr);
            node->set_prev(nullptr);
        }
    }

}; // namespace ncore
