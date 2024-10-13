#include "ccore/c_target.h"
#include "ccore/c_memory.h"

#include "callocator/c_allocator_linear.h"

namespace ncore
{
    typedef linear_alloc_t::node_t lnode_t;

    struct linear_alloc_t::node_t
    {
        inline lnode_t* get_next() { return (m_next == 0) ? nullptr : this + m_next; }
        inline void     set_next(lnode_t* next) { m_next = (next == nullptr) ? 0 : ((u32)(next - this)); }
        inline lnode_t* get_prev() { return (m_prev == 0) ? nullptr : this - m_prev; }
        inline void     set_prev(lnode_t* prev) { m_prev = (prev == nullptr) ? 0 : ((u32)(this - prev)); }

        inline void set_deallocated()
        {
#ifdef TARGET_DEBUG
            m_next = 0xF2EEF2EE;
            m_prev = 0xF2EEF2EE;
#endif
        }

        // adjust the cursor to the alignment request but also check if we are not moving beyond 'next'
        // furthermore, we need to ensure that we can still fulfill the requested allocation size.
        // Note: the unit of _alloc_size_requested is 'number of nodes'
        static bool s_align_to(node_t*& _this, u32 _alignment, u32 _alloc_size_requested)
        {
            if (_alignment > sizeof(node_t))
            {
                node_t* cursor         = _this + 1; // Alignment requests are for the pointer after the node
                node_t* aligned_cursor = nmem::ptr_align(cursor, _alignment);

                // The cursor shift according to the alignment request
                u32 const cursor_shift = (u32)(aligned_cursor - cursor);
                if (cursor_shift > 0)
                {
                    // Aligning a node we need to verify if we are not moving beyond the next node
                    if (cursor_shift >= _this->m_next)
                        return false; // Not enough memory to align this node

                    // Check if we can still fulfill the requested allocation size
                    u32 const next = _this->m_next - cursor_shift;
                    if (_alloc_size_requested > (next - 1))
                        return false;

                    // Move and adjust the cursor according to the alignment shift
                    u32 const prev = _this->m_prev + cursor_shift;
                    _this += cursor_shift;
                    _this->m_next = next;
                    _this->m_prev = prev;
                    _this->get_prev()->set_next(_this); // Adjust the previous node
                    _this->get_next()->set_prev(_this); // Adjust the next node
                    return true;
                }
            }

            // Check if we can still fulfill the requested allocation size
            return (_alloc_size_requested < _this->m_next);
        }

        u32 m_next; // relative offsets (forwards)
        u32 m_prev; // relative offsets (backwards)
    };

    static inline bool is_pointer_inside(void* ptr, lnode_t* begin, lnode_t* end) { return ptr >= (void*)(begin + 1) && ptr < (void*)(end - 1); }

    static lnode_t* s_reset_cursor(lnode_t* begin, lnode_t* end)
    {
        lnode_t* cursor = begin + 1;
        cursor->set_prev(begin);
        cursor->set_next(end);
        begin->set_next(cursor);
        end->set_prev(cursor);
        return cursor;
    }

    static bool s_validate_chain(lnode_t* begin, lnode_t* end)
    {
        if (begin->m_next == 0 || begin->m_next == 0xF2EEF2EE)
            return false;

        lnode_t* cursor = begin->get_next();
        while (cursor != end)
        {
            if (cursor >= end)
                return false;

            if (cursor->m_prev == 0 || cursor->m_prev == 0xF2EEF2EE)
                return false;
            lnode_t* prev      = cursor->get_prev();
            lnode_t* prev_next = prev->get_next();
            if (prev_next != cursor)
                return false;

            if (cursor->m_next == 0 || cursor->m_next == 0xF2EEF2EE)
                return false;
            lnode_t* next      = cursor->get_next();
            lnode_t* next_prev = next->get_prev();
            if (next_prev != cursor)
                return false;

            cursor = next;
        }
        return cursor == end;
    }

    linear_alloc_t::linear_alloc_t() : m_buffer_begin(nullptr), m_buffer_cursor(nullptr) {}
    linear_alloc_t::~linear_alloc_t() {}

    void linear_alloc_t::setup(void* _beginAddress, u32 _size)
    {
        // Align to 8 bytes
        void* const beginAddress = nmem::ptr_align(_beginAddress, (u32)sizeof(node_t));
        void* const endAddress   = (u8*)_beginAddress + _size;
        u32 const   size         = (u8*)endAddress - (u8*)beginAddress;

        m_buffer_begin = (node_t*)beginAddress;
        m_buffer_end   = m_buffer_begin + (size / sizeof(node_t)) - 1;

        m_buffer_begin->set_prev(nullptr);
        m_buffer_begin->set_next(m_buffer_cursor);

        m_buffer_end->set_prev(m_buffer_cursor);
        m_buffer_end->set_next(nullptr);

        m_buffer_cursor = s_reset_cursor(m_buffer_begin, m_buffer_end);

        ASSERT(s_validate_chain(m_buffer_begin, m_buffer_end));
    }

    bool linear_alloc_t::is_valid() const { return m_buffer_begin != nullptr && m_buffer_cursor < m_buffer_cursor->get_next(); }
    bool linear_alloc_t::is_empty() const
    {
        return m_buffer_cursor == m_buffer_begin + 1 && m_buffer_cursor->get_next() == m_buffer_end && m_buffer_cursor->get_prev() == m_buffer_begin && m_buffer_begin->get_next() == m_buffer_cursor && m_buffer_end->get_prev() == m_buffer_cursor;
    }
    void linear_alloc_t::reset() { m_buffer_cursor = s_reset_cursor(m_buffer_begin, m_buffer_end); }

    void* linear_alloc_t::v_allocate(u32 size, u32 alignment)
    {
        if (m_buffer_cursor == m_buffer_end)
            return nullptr;

        // align the size to a multiple of sizeof(node_t)
        // size unit is now 'number of nodes'
        size = (size + (sizeof(node_t) - 1)) / sizeof(node_t);

        // adjust cursor when we can allocate this size even considering the alignment
        if (!node_t::s_align_to(m_buffer_cursor, alignment, size))
        {
            // if the cursor is at the beginning of the chain, we are out of memory
            if (m_buffer_cursor->get_prev() == m_buffer_begin)
            {
                return nullptr;
            }

            // detach the cursor from the chain
            m_buffer_cursor->get_prev()->set_next(m_buffer_cursor->get_next());
            m_buffer_cursor->get_next()->set_prev(m_buffer_cursor->get_prev());

            // insert the cursor between the beginning of the chain and its next node
            m_buffer_cursor = s_reset_cursor(m_buffer_begin, m_buffer_begin->get_next());

            // check if we can align and allocate this size
            if (!node_t::s_align_to(m_buffer_cursor, alignment, size))
            {
                return nullptr;
            }
        }

        node_t* current_cursor = m_buffer_cursor;
        node_t* end_cursor     = m_buffer_cursor->get_next();
        node_t* new_cursor     = current_cursor + 1 + size;

        current_cursor->set_next(new_cursor);
        new_cursor->set_prev(current_cursor);
        new_cursor->set_next(end_cursor);
        end_cursor->set_prev(new_cursor);

        m_buffer_cursor = new_cursor;

        void* ptr = current_cursor + 1;

#ifdef TARGET_DEBUG
        nmem::memset(ptr, 0xCD, size * sizeof(node_t));
#endif

        ASSERT(s_validate_chain(m_buffer_begin, m_buffer_end));
        return ptr;
    }

    void linear_alloc_t::v_deallocate(void* ptr)
    {
        if (ptr == nullptr)
            return;

        ASSERT(is_pointer_inside(ptr, m_buffer_begin, m_buffer_end));

        node_t* const node = (node_t*)ptr - 1;

        // check if the node wasn't already freed
        // this is not fully bullet proof, but it's a good check
        ASSERT(node->m_next != 0xF2EEF2EE && node->m_prev != 0xF2EEF2EE);

        // remove this node from the chain (merge)
        node_t*       node_next = node->get_next();
        node_t* const node_prev = node->get_prev();

        // always see if we can move the cursor to the most left of the chain
        if (m_buffer_cursor == node_next)
        {
            // move the cursor to this node that has been freed
            node_next = m_buffer_cursor->get_next();
            node->set_next(node_next);
            node_next->set_prev(node);
            m_buffer_cursor = node;

            // check if we can move the cursor more to the left
            if (node_prev == m_buffer_begin)
            {
                m_buffer_cursor = s_reset_cursor(node_prev, node_next);
            }
        }
        else
        {
            // the node freed is in the middle of the chain, cannot move cursor
            node_prev->set_next(node_next);
            node_next->set_prev(node_prev);
            node->set_deallocated();
        }

        ASSERT(s_validate_chain(m_buffer_begin, m_buffer_end));
    }

}; // namespace ncore
