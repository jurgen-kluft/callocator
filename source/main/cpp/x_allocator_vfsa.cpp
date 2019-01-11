#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_memory_std.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"
#include "xbase/x_tree.h"

#include "xallocator/x_allocator_vfsa.h"
#include "xallocator/private/x_marklist.h"

namespace xcore
{
    class xfsallocator : public xalloc
    {
    public:
        u32        m_min_size;
        u32        m_max_size;
        u32        m_size_step;
        xfsalloc** m_fsalloc;
    };

    /**
        @brief	xvfsallocator is a fixed size allocator using virtual memory pages over a reserved address range.

        This makes it the address of pages predictable and deallocation thus easy to find out which page to manipulate.

        Example:
            FSA address range = 512 MB
            Page Size = 64 KB
            Number of Pages = 8192

        @note	This allocator does not guarantee that two objects allocated sequentially are sequential in memory.
        **/
    class xvfsallocator : public xfsalloc
    {
    public:
        xvfsallocator();

        virtual const char* name() const { return TARGET_FULL_DESCR_STR " Virtual Memory FSA"; }

        ///@name	Should be called when created with default constructor
        //			Parameters are the same as for constructor with parameters
        void init(xalloc* a, xpage_alloc* page_allocator, xvfsa_params const& params);

        virtual void* allocate(u32& size);
        virtual void  deallocate(void* p);

        ///@name	Placement new/delete
        XCORE_CLASS_PLACEMENT_NEW_DELETE

    protected:
        bool extend(u32& page_index, void*& page_address);

        virtual void release();
        virtual ~xvfsallocator() {}

    protected:
        xvfsa_params m_params;
        xalloc*      m_alloc;
        xvpalloc*    m_page_allocator;
        xpage_info*  m_page_current;
        u32          m_page_current_index;
        xbitlist     m_pages_not_full;

    private:
        // Copy construction and assignment are forbidden
        xvfsallocator(const xvfsallocator&);
        xvfsallocator& operator=(const xvfsallocator&);
    };

    xvfsallocator::xvfsallocator() : m_alloc(nullptr), m_page_allocator(nullptr), m_page_allocating(-1) {}

    void xvfsallocator::init(xalloc* a, xpage_alloc* page_allocator, xvfsa_params const& params)
    {
        m_params             = params;
        m_alloc              = a;
        m_page_allocator     = page_allocator;
        m_page_current       = nullptr;
        m_page_current_index = 0xffffffff;

        u32 const maxnumpages = m_params.get_address_range() / m_params.get_page_size();

        m_pages_not_full.init(xheap(a), maxnumpages, true);
    }

    void* xvfsallocator::allocate(u32& size)
    {
        if (m_page_current == Null)
            return nullptr;

        void* page_address = calc_page_address(m_page_to_alloc_from);
        void* p            = m_pages[m_page_to_alloc_from].allocate(page_address);

        if (m_page_current->full())
        {
            m_pages_not_full.clr(m_page_current_index);
            if (!m_pages_not_full.find(m_page_current_index))
            {
                // Could not find an empty page, so we have to extend
                if (!extend(page_address, m_page_to_alloc_from))
                {
                    // No more decommitted pages available, full!
                    return nullptr;
                }
                m_pages_not_full.clr(m_page_current_index);
            }
        }
        return p;
    }

    void xvfsallocator::deallocate(void* p)
    {
        s32   page_index = calc_page_index(p);
        void* page_addr  = calc_page_address(page_index);
        m_pages[page_index].deallocate(p);
        if (m_pages[page_index].empty())
        {
            // The page we deallocated our item at is now empty
            m_pages_not_full.clr(page_index);
            m_pages_empty.set(page_index);
            m_pages_empty_cnt += 1;
            if (page_index == m_page_to_alloc_from)
            {
                // Get a 'better' page to allocate from (biasing)
                m_page_to_alloc_from = m_pages_empty.find();
            }
        }
    }

    bool xvfsallocator::extend(u32& page_index, void*& page_address)
    {
        page_address = m_page_allocator->allocate(page_index);
        return page_address != nullptr;
    }

    void xvfsallocator::release()
    {
        // Return all pages back to the page allocator

        xheap heap(m_alloc);
        heap.destruct(this);
    }

    xfsalloc* gCreateVFsAllocator(xalloc* a, xpage_alloc* page_allocator, xvfsa_params const& params)
    {
        xheap          heap(a);
        xvfsallocator* allocator = heap.construct<xvfsallocator>();
        allocator->init(a, page_allocator, params);
        return allocator;
    }

}; // namespace xcore
