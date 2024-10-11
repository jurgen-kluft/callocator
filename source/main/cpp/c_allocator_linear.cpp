#include "ccore/c_target.h"
#include "ccore/c_memory.h"

#include "callocator/c_allocator_linear.h"

namespace ncore
{
    const u32 c_node_state_free   = 0x80000000;
    const u32 c_node_state_alloc  = 0x40000000;
    const u32 c_node_state_locked = 0x80000000;
    const u32 c_node_state_head   = 0x40000000;
    const u32 c_node_state_mask   = 0xC0000000;

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

    const u32 c_state_parent = 0x80000000;
    const u32 c_state_mask   = 0xC0000000;

    // static inline void set_parent(u32& flags) { flags = (flags & ~c_state_mask) | c_state_parent; }
    // static inline bool is_parent(u32 flags) { return (flags & c_state_mask) == c_state_parent; }

    linear_alloc_t::linear_alloc_t() : m_buffer_begin(nullptr), m_buffer_cursor(nullptr) {}
    linear_alloc_t::~linear_alloc_t() {}

    void linear_alloc_t::setup(void* beginAddress, u32 size)
    {
        // Align to 8 bytes
        beginAddress = (void*)s_align_ptr(beginAddress, (u32)8);

        m_buffer_begin = (node_t*)beginAddress;
        m_buffer_end   = m_buffer_begin + (size / sizeof(node_t));
        m_buffer_begin->reset();
        m_buffer_end->reset();
        m_buffer_cursor = m_buffer_begin + 1;

        m_buffer_begin->set_prev(nullptr);
        m_buffer_begin->set_next(m_buffer_cursor);
        m_buffer_begin->set_state_locked();

        m_buffer_end->set_prev(m_buffer_cursor);
        m_buffer_end->set_next(nullptr);
        m_buffer_end->set_state_locked();

        m_buffer_cursor->set_prev(m_buffer_begin);
        m_buffer_cursor->set_next(m_buffer_end);
        m_buffer_cursor->set_state_head();
    }

    bool linear_alloc_t::is_valid() const { return m_buffer_begin != nullptr && m_buffer_cursor < m_buffer_cursor->get_next(); }
    bool linear_alloc_t::is_empty() const { return m_buffer_cursor == m_buffer_begin->get_next() && m_buffer_cursor->get_next() == m_buffer_end; }
    void linear_alloc_t::reset()
    {
        m_buffer_cursor = m_buffer_begin;
        m_buffer_cursor->reset();
        m_buffer_cursor->set_prev(m_buffer_cursor);
        m_buffer_cursor->set_next(m_buffer_end);
    }

    // // Checkout will reserve a part of the memory and construct a new linear_alloc_t and provides
    // // it a start and end block of memory to work with.
    // linear_alloc_t* linear_alloc_t::checkout(u32 _size, u32 alignment)
    // {
    //     ASSERT(m_buffer_cursor->is_head());

    //     _size = s_align_u32(_size + 8, 8);

    //     // Check if we had an active checkout already, if so we should commit it first.

    //     // Set the beginning node here to a state that will not be merged by any
    //     // deallocation that might happen.
    //     m_buffer_cursor->set_state_locked();

    //     // [parent node, checkout] [child linear alloc] [child begin/cursor, head] ----- memory ------ [child end, checkout] [parent cursor, head]

    //     void*           na_mem = ((u8*)(m_buffer_cursor + 1));
    //     linear_alloc_t* na     = new (na_mem) linear_alloc_t();
    //     na->m_parent           = this;
    //     na->m_flags            = 0;
    //     na->m_buffer_begin     = m_buffer_cursor + 1 + s_align_u32((u32)sizeof(linear_alloc_t), 8) / sizeof(node_t);
    //     na->m_buffer_begin->reset();
    //     na->m_buffer_begin->set_prev(m_buffer_cursor);
    //     na->m_buffer_begin->set_next(na->m_buffer_begin);
    //     na->m_buffer_begin->set_state_head();
    //     na->m_buffer_end = na->m_buffer_begin + (_size / sizeof(node_t));
    //     na->m_buffer_end->reset();
    //     na->m_buffer_end->set_next(na->m_buffer_end + 1); // See 'cursor' below
    //     na->m_buffer_end->set_prev(na->m_buffer_cursor);
    //     na->m_buffer_end->set_state_locked();

    //     na->m_buffer_cursor = na->m_buffer_begin;

    //     // We do need to create a next node for this allocator, so that any call to v_allocate
    //     // will create a new node at the end of the requested memory size. This is necessary to
    //     // keep the chain of nodes consistent.
    //     // When commit is called we can check the last node and if it is a free node we can merge it.
    //     node_t* next = na->m_buffer_end + 1;
    //     next->reset();
    //     next->set_next(next);
    //     next->set_prev(na->m_buffer_end);
    //     next->set_state_head();

    //     m_buffer_cursor->set_next(na->m_buffer_begin);
    //     m_buffer_cursor->set_state_locked();
    //     m_buffer_cursor = next;

    //     return na;
    // }

    // // Commit will finalize the checkout and merge the memory of the checkout allocator with the current allocator.
    // // This means that any allocation done with the checkout allocator will be part of the current allocator, and
    // // calling deallocate on the current allocator will also deallocate the memory of the checkout allocator.
    // void linear_alloc_t::commit()
    // {
    //     if (m_parent == nullptr)
    //         return;

    //     if (m_count == 0)
    //     {
    //         // We haven't allocated anything, see if our parent also hasn't allocated anything.
    //         // All of our nodes are in a chain, so we only have to check the last node which is
    //         // a node that is managed by our parent. If that node is still marked as head then
    //         // we know that our parent hasn't allocated anything more.
    //         if (m_buffer_end->get_next()->is_head())
    //         {
    //             // We can rewind the parent cursor
    //             m_parent->m_buffer_cursor = m_buffer_begin->get_prev();
    //             m_parent->m_buffer_cursor->set_state_head();
    //         }
    //         else
    //         {
    //             // We can't merge the memory of the checkout allocator with the parent allocator
    //             // because the parent allocator has, in the meantime, received more allocation requests.
    //             // We should commit the memory of the checkout allocator and make it part of the parent allocator.
    //             // This is very 'bad' since we are waisting memory, but we can't do anything about it.
    //             m_buffer_begin->get_prev()->set_next(m_buffer_end->get_next());
    //         }
    //     }
    //     else
    //     {
    //         // We have allocated memory, see if our parent, in the meantime, has allocated memory.
    //         // If not we can change the cursor of our parent allocator to be our cursor.
    //         if (m_buffer_end->get_next()->is_head())
    //         {
    //             // Parent hasn't allocated anything, we can rewind the cursor
    //             m_parent->m_buffer_cursor = m_buffer_cursor;
    //         }
    //         else
    //         {
    //             // Parent has allocated memory, connect our last allocation with the node after m_buffer_end
    //             m_buffer_cursor->get_prev()->set_next(m_buffer_end->get_next());
    //         }
    //     }

    //     // We are done, reset the parent pointer
    //     m_parent = nullptr;
    // }

    void* linear_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        // @todo
        // allocate initializes current node and creates a new
        // node at the end of the requested memory size
        ASSERT(m_buffer_cursor->is_head());

        size = s_align_u32(size, sizeof(node_t));

        // @todo, check if we can allocate this size

        // @todo, alignment, shift m_buffer_cursor forward to handle the alignment request
        // for that we need to update m_buffer_cursor->m_prev->m_next

        void* ptr = (void*)(m_buffer_cursor + 1);
        m_buffer_cursor->set_state_alloc();

        node_t* cursor = m_buffer_cursor + 1 + (size / sizeof(node_t));
        cursor->set_next(cursor);
        cursor->set_prev(m_buffer_cursor);
        cursor->set_state_head();
        m_buffer_cursor = cursor; // This is our new head

#ifdef TARGET_DEBUG
        nmem::memset(ptr, 0xCD, size);
#endif
        return ptr;
    }

    void linear_alloc_t::v_deallocate(void* ptr)
    {
        node_t* node = (node_t*)ptr - 1;
        ASSERT(!node->is_free());
        node->set_state_free();

        // remove this node from the chain (merge)
        if (node->get_prev() != nullptr)
        {
            node->get_prev()->set_next(node->get_next());
        }
        if (node->get_next() != nullptr)
        {
            node->get_next()->set_prev(node->get_prev());
        }

    }

}; // namespace ncore
