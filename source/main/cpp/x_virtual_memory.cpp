#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_allocator.h"

#include "xallocator/x_virtual_memory.h"

namespace xcore
{
    class xvmem_os : public xvmem
    {
    public:
        virtual bool reserve(u64 address_range, u32 page_size, u32 attributes, void*& baseptr, void*& id);
        virtual bool release(void* baseptr, void* id);

        virtual bool commit(void* id, void* page_address, u32 page_count);
        virtual bool decommit(void* id, void* page_address, u32 page_count);
    };

#if defined TARGET_MAC

    bool xvmem_os::reserve(u64 address_range, u32 page_size, u32 attributes, void*& baseptr, void*& id) { return false; }

    bool xvmem_os::release(void* baseptr, void* id) { return false; }

    bool xvmem_os::commit(void* id, void* page_address, u32 page_count) { return false; }

    bool xvmem_os::decommit(void* id, void* page_address, u32 page_count) { return false; }

#elif defined TARGET_PC

    bool xvmem_os::reserve(u64 address_range, u32 page_size, u32 attributes, void*& baseptr, void*& id) { return false; }

    bool xvmem_os::release(void* baseptr, void* id) { return false; }

    bool xvmem_os::commit(void* id, void* page_address, u32 page_count) { return false; }

    bool xvmem_os::decommit(void* id, void* page_address, u32 page_count) { return false; }

#else

#error Unknown Platform/Compiler configuration for xvmem

#endif

    xvmem* gGetVirtualMemory()
    {
        static xvmem_os sVMem;
        return &sVMem;
    }

    // -----------------------------------------------------------------------------------------------------
    // -----------------------------------------------------------------------------------------------------
    // Virtual Memory Page Allocator
    //
    // This allocator is capable of allocating pages from a virtual address range. When a page is needed it is
    // committed to physical memory and tracked
    // decommiting

    class xvpage_alloc : public xpage_alloc
    {
    public:
        virtual void* allocate(u32& size)
        {
            void* ptr = nullptr;
            if (m_pages_empty_cnt == 0)
            {
                if (m_pages_free_cnt > 0)
                {
                    u32 page_index;
                    if (m_pages_free.find(page_index))
                    {
                        m_pages_used_cnt += 1;
                        m_pages_used.set(page_index);
                        m_pages_free_cnt -= 1;
                        m_pages_free.clr(page_index);
                        ptr  = calc_page_addr(page_index);
                        size = m_page_size;
                    }
                }
            }
            else
            {
                u32 page_index;
                if (m_pages_empty.find(page_index))
                {
                    m_pages_used_cnt += 1;
                    m_pages_used.set(page_index);
                    m_pages_empty_cnt -= 1;
                    m_pages_empty.clr(page_index);
                    ptr = calc_page_addr(page_index);
                    m_vmem->commit(ptr, 1);
                    size = m_page_size;
                }
            }
            return ptr;
        }

        virtual void deallocate(void* ptr)
        {
            s32 const pindex = calc_page_index(ptr);
            m_pages_used_cnt -= 1;
            m_pages_used.clr(pindex);
            m_pages_empty_cnt += 1;
            m_pages_empty.set(pindex);
            if (m_pages_empty_cnt > m_pages_comm_max)
            {
                // Decommit pages
            }
        }

        virtual bool get_info(void* ptr, void*& page_addr, xvpage*& page_info)
        {
            if (ptr >= m_addr_base && ptr < ((uptr)m_addr_base + m_addr_range))
            {
                s32 const pindex = calc_page_index(ptr);
                page_addr        = calc_page_addr(pindex);
                page_info        = &m_pages[page_index];
                return true;
            }
            return false;
        }

        virtual void release()
        {
            ASSERT(m_pages_used_cnt == 0); // Any pages still being used ?

            // decommit
            u32 page_index;
            while (m_pages_empty.find(page_index))
            {
                void* page_addr = calc_page_addr(page_index);
                m_vmem->decommit(page_addr, m_page_size);
            }
            m_vmem->release(m_addr_base);

            m_vmem        = nullptr;
            m_addr_base   = nullptr;
            m_addr_range  = 0;
            m_page_size   = 0;
            m_page_battrs = 0;
            m_page_pattrs = 0;

            xheap heap(m_alloc);
            m_alloc = nullptr;

            m_pages_comm_max  = 0;
            m_pages_used_cnt  = 0;
            m_pages_empty_cnt = 0;
            m_pages_free_cnt  = 0;
            heap.deallocate(m_pages);
            heap.deallocate(m_pages_used.m_hbitmap);
            heap.deallocate(m_pages_empty.m_hbitmap);
            heap.deallocate(m_pages_free.m_hbitmap);
        }

        xalloc* m_alloc;
        xvmem*  m_vmem;

        void* calc_page_addr(u32 index) const { return (uptr)m_addr_base + index * m_page_size; }

        s32 calc_page_index(void* ptr) const
        {
            ASSERT(ptr >= m_addr_base && ptr < (m_addr_base + m_addr_range));
            s32 const page_index = (s32)(((uptr)ptr - (uptr)m_addr_base) / m_page_size);
            return page_index;
        }

        void* m_addr_base;
        u64   m_addr_range;
        u32   m_page_size;
        u32   m_page_battrs;
        u32   m_page_pattrs;
        u32   m_pages_comm_max;

        xvpage*  m_pages;
        s32      m_pages_used_cnt;
        xbitlist m_pages_used; // Pages that are committed and used
        s32      m_pages_empty_cnt;
        xbitlist m_pages_empty; // Pages that are committed but free
        s32      m_pages_free_cnt;
        xbitlist m_pages_free; // Pages that are not committed and free

        void init(xheap& heap, u64 address_range, u32 page_size, u32 page_battrs, u32 page_pattrs) {}
    };

    xpage_alloc* gCreateVMemPageAllocator(xalloc* a, u64 address_range, u32 page_size, u32 page_battrs, u32 page_pattrs,
                                          xvirtual_memory* vmem)
    {
        xheap         heap(a);
        xvpage_alloc* allocator = heap.construct<xvpage_alloc>();
        allocator->m_alloc      = a;
        allocator->m_vmem       = vmem;
        allocator->init(heap, address_range, page_size, page_battrs, page_pattrs);
        return allocator;
    }

}; // namespace xcore
