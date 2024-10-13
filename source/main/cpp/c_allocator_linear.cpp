#include "ccore/c_target.h"
#include "ccore/c_memory.h"

#include "callocator/c_allocator_linear.h"

namespace ncore
{
    struct linear_alloc_t::node_t
    {
        inline void reset()
        {
            m_next = 0;
            m_prev = 0;
        }

        inline linear_alloc_t::node_t* get_next() { return (m_next == 0) ? nullptr : this + m_next; }
        inline void                    set_next(linear_alloc_t::node_t* next) { m_next = (next == nullptr) ? 0 : ((u32)(next - this)); }
        inline linear_alloc_t::node_t* get_prev() { return (m_prev == 0) ? nullptr : this - m_prev; }
        inline void                    set_prev(linear_alloc_t::node_t* prev) { m_prev = (prev == nullptr) ? 0 : ((u32)(this - prev)); }

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
        begin->set_next(cursor);
        begin->set_prev(nullptr);
        end->set_next(nullptr);
        end->set_prev(cursor);
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

        m_buffer_end->set_prev(m_buffer_cursor);
        m_buffer_end->set_next(nullptr);

        m_buffer_cursor = s_reset_cursor(m_buffer_begin, m_buffer_end);
    }

    bool linear_alloc_t::is_valid() const { return m_buffer_begin != nullptr && m_buffer_cursor < m_buffer_cursor->get_next(); }
    bool linear_alloc_t::is_empty() const { return m_buffer_cursor == m_buffer_begin + 1; }
    void linear_alloc_t::reset() { m_buffer_cursor = s_reset_cursor(m_buffer_begin, m_buffer_end); }

    void* linear_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        if (m_buffer_cursor == m_buffer_end)
            return nullptr;

        // align the size to a multiple of the node_t size
        size = s_align_u32(size, sizeof(node_t));

        // check if we can allocate this size
        u32 sizeLeft = (m_buffer_cursor->get_next() - (m_buffer_cursor + 1)) * sizeof(node_t);
        if (size > sizeLeft)
        {
            // Can we move the cursor to the beginning of the chain?
            return nullptr;
        }

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

            // check again if we can allocate this size
            sizeLeft = (m_buffer_cursor->get_next() - (adjusted_cursor + 1)) * sizeof(node_t);
            if (size > sizeLeft)
            {
                // Can we move the cursor to the beginning of the chain?
                return nullptr;
            }

            m_buffer_cursor = adjusted_cursor;
            ptr             = aligned_ptr;
        }

        sizeLeft = sizeLeft - size;
        if (sizeLeft <= sizeof(node_t))
        {
            // We are out of memory, we can't create a new node
            m_buffer_cursor->get_prev()->set_next(m_buffer_end);
            m_buffer_cursor->get_next()->set_prev(m_buffer_cursor->get_prev());
            m_buffer_cursor = m_buffer_end;

            // Can we move the cursor to the beginning of the chain?
        }
        else
        {
            node_t* new_cursor = m_buffer_cursor + 1 + (size / sizeof(node_t));
            new_cursor->reset();
            new_cursor->set_next(m_buffer_cursor->get_next());
            new_cursor->set_prev(m_buffer_cursor);
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

        // check if the node wasn't already freed
        // this is not fully bullet proof, but it's a good check
        ASSERT(node->m_next != 0xF2EEF2EE && node->m_prev != 0xF2EEF2EE);

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
            }
        }
        else
        {
            // the node freed is in the middle of the chain, cannot move cursor
            prev->set_next(next);
            next->set_prev(prev);
            node->m_next = 0xF2EEF2EE;
            node->m_prev = 0xF2EEF2EE;
        }
    }

}; // namespace ncore
