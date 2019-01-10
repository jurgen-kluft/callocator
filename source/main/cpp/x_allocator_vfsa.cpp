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
    /**
        @brief	xvfsallocator is a fixed size allocator using virtual memory pages over a reserved address range.

        This makes it the address of pages predictable and deallocation thus easy to find out which page to manipulate.

        @note	This allocator does not guarantee that two objects allocated sequentially are sequential in memory.
        **/
    class xvfsallocator : public xfsalloc
    {
    public:
        xvfsallocator();

        xvfsallocator(xalloc* allocator, xvfsa_params const& params);
        virtual ~xvfsallocator();

        virtual const char* name() const { return TARGET_FULL_DESCR_STR " Virtual Memory FSA"; }

        ///@name	Should be called when created with default constructor
        //			Parameters are the same as for constructor with parameters
        void init();

        virtual void* allocate(u32& size);
        virtual void  deallocate(void* p);

        ///@name	Placement new/delete
        XCORE_CLASS_PLACEMENT_NEW_DELETE

    protected:
        void         reset(xbool inRestoreToInitialSize = xFALSE);
        void         extend(u32 inBlockCount, u32 inBlockMaxCount);
        virtual void release();

    protected:
        xvfsa_params mParams;
        xalloc*      mAllocator;
        xvmem*       mVirtualMemory;

        struct page_t
        {
			// Smallest alloc size is '4 bytes'!
			// Alloc size is aligned up on '4 bytes'
			enum { Null = 0xffffffff }
			inline page_t() : m_allocs(0), m_head(0) {}

			void	init(void* page_addr, u32 page_size, u32 alloc_size)
			{
				m_allocs = 0;
				m_head = 0;
				u32 const node_size = (alloc_size + 3) / 4;
				u32 const count = (page_size/4) / node_size;
				u32* node = (u32*)page_addr;
				for (u32 i=0; i<count; ++i)
				{
					node[0] = (i + 1);
					node += node_size;
				}
				node = (u32*)page_addr;
				node[count - 1] = Null;
			}

			void	reset()
			{
				m_allocs = 0;
				m_head = Null;
			}

			bool	full() const { return m_head == Null; }

			void*	allocate(void* page_addr)
			{
				ASSERT(full() == false);
				u32* node = (u32*)((xbyte*)page_addr + m_head);
				m_head = node[0];
				m_allocs += 1;
				return (void*)node;
			}

			// Make sure this is the correct page!
			void	deallocate(void* alloc_addr, void* page_addr)
			{
				u32* node = (u32*)alloc_addr;
				node[0] = m_head;
				m_head = (u32)((uptr)alloc_addr - (uptr)page_addr);
			}

            s32 m_allocs; // Number of allocations done on this page
            u32 m_head;   // The head of the free list (item = page-address + m_head)
        };

        void* m_addr_base;
        u64   m_addr_range;
        u32   m_page_size;
        s32   m_per_page_max_allocs;

        s32 calc_page_index(void* ptr) const
        {
            ASSERT(ptr >= m_addr_base && ptr < (m_addr_base + m_addr_range));
            s32 const page_index = (s32)(((uptr)ptr - (uptr)m_addr_base) / m_page_size);
            return page_index;
        }

        // Every page has a freelist, alloc-count and can be in the 'pages not full' list or the
        // 'pages empty' list.

        // When a page is full we remove it from 'freelist_pages' and we get a new page which
        // still has a free list.
        page_t*    m_pages;
        page_t*    m_page_to_alloc_from; // Current page we are allocating from
        xmarklist	m_pages_not_full;

        // When deallocating and a page and 'm_allocs == 0' we should remove it from 'm_pages_not_full' and
        // add it here to 'm_pages_empty'
        s32        m_pages_empty_cnt;
        marklist_t m_pages_empty;

    private:
        // Copy construction and assignment are forbidden
        xvfsallocator(const xvfsallocator&);
        xvfsallocator& operator=(const xvfsallocator&);
    };

    xfsalloc* gCreateVFsAllocator(xalloc* a, xvmem* vmem, xvfsa_params const& params)
    {
        void*          mem       = a->allocate(sizeof(xvfsallocator), X_ALIGNMENT_DEFAULT);
        xvfsallocator* allocator = new (mem) xvfsallocator(a, vmem, params);
        allocator->init();
        return allocator;
    }

}; // namespace xcore
