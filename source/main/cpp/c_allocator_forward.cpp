#include "ccore/c_target.h"
#include "cbase/c_memory.h"
#include "cbase/c_limits.h"
#include "cbase/c_integer.h"
#include "cbase/c_allocator.h"
#include "cbase/c_integer.h"

#include "callocator/c_allocator_forward.h"

namespace ncore
{
    class allocator_forward : public alloc_t
    {
    public:
        allocator_forward();
        allocator_forward(u8* beginAddress, u32 size, alloc_t* allocator);
        virtual ~allocator_forward();

        void                     initialize(void* beginAddress, u32 size);
        template <typename T> T* checkout(u32 count, u32 alignment = sizeof(void*)) { return (T*)v_checkout((u32)(count * sizeof(T)), alignment); }
        void                     commit(void* ptr) { v_commit(ptr); }

        DCORE_CLASS_PLACEMENT_NEW_DELETE

    private:
        virtual void* v_checkout(u32 size, u32 alignment);
        virtual void  v_commit(void* ptr);
        virtual void* v_allocate(u32 size, u32 alignment);
        virtual u32   v_deallocate(void* ptr);
        virtual void  v_release();

        struct header_t;

        alloc_t*  main_;
        header_t* header_;
        u8*       buffer_;
        u8*       buffer_begin_;
        u8*       buffer_end_;
        s32       checkout_;
        s32       num_allocations_;

        allocator_forward(const allocator_forward&);
        allocator_forward& operator=(const allocator_forward&);
    };

    struct allocator_forward::header_t
    {
        u64   magic;
        void* ptr;
        s64   size;
    };

    allocator_forward::allocator_forward() : main_(nullptr), header_(nullptr), buffer_(nullptr), buffer_begin_(nullptr), buffer_end_(nullptr), checkout_(0), num_allocations_(0) {}
    allocator_forward::allocator_forward(u8* beginAddress, u32 size, alloc_t* allocator) : main_(allocator), header_(nullptr), buffer_(beginAddress), buffer_begin_(beginAddress), buffer_end_(beginAddress + size), checkout_(0), num_allocations_(0) {}
    allocator_forward::~allocator_forward() { release(); }

    void allocator_forward::initialize(void* beginAddress, u32 size)
    {
        buffer_begin_    = (u8*)beginAddress;
        buffer_end_      = (u8*)beginAddress + size;
        buffer_          = (u8*)beginAddress;
        checkout_        = 0;
        num_allocations_ = 0;
    }

    void allocator_forward::v_release()
    {
        main_->deallocate(buffer_begin_);
        main_->deallocate(this);
    }

    void* allocator_forward::v_checkout(u32 size, u32 alignment)
    {
        ASSERT(checkout_ >= 0);
        u8* p = ncore::g_align_ptr(buffer_, alignment);
        if ((p + size) > buffer_end_)
            return nullptr;

        header_        = (header_t*)p;
        p              = (u8*)(header_ + 1);
        header_->magic = 0XDEADBEEFDEADBEEFULL;
        header_->ptr   = (void*)(p);
        header_->size  = size;

        ASSERT(buffer_ < buffer_end_);
        num_allocations_++;
        checkout_++;
        return p;
    }

    void allocator_forward::v_commit(void* ptr)
    {
        header_->size = (u8*)ptr - (u8*)(header_->ptr);
        buffer_       = (u8*)ptr;
        --checkout_;
        ASSERT(buffer_ < buffer_end_);
        ASSERT(checkout_ == 0);
    }

    void* allocator_forward::v_allocate(u32 size, u32 alignment)
    {
        void* ptr = v_checkout(size, alignment);
        if (ptr == nullptr)
            return nullptr;
        v_commit((u8*)ptr + size);
        return ptr;
    }

    u32 allocator_forward::v_deallocate(void* ptr)
    {
        // Check the header for double free's and validate the pointer
        header_t* header = (header_t*)ptr - 1;
        if (header->magic == 0XDEADBEEFDEADBEEFULL)
        {
            if (header->ptr != ptr)
                return;
            if (header->size <= 0)
                return;
        }
        else
        {
            // corrupted ?
            return;
        }

        ASSERT(header->magic == 0XDEADBEEFDEADBEEFULL);
        ASSERT(header->ptr == ptr);
        ASSERT(header->size > 0);

        // Empty this header by just setting size to 0
        header->size = 0;

        ASSERT(ptr >= buffer_begin_ && ptr < buffer_end_);
        if (num_allocations_ == 0)
        {
            return;
        }
        ASSERT(num_allocations_ > 0);
        --num_allocations_;
    }

    alloc_t* gCreateForwardAllocator(alloc_t* allocator, u32 memsize)
    {
        void*              memForAllocator      = allocator->allocate(sizeof(allocator_forward), sizeof(void*));
        void*              mem                  = allocator->allocate(memsize + 32, sizeof(void*));
        allocator_forward* forwardRingAllocator = new (memForAllocator) allocator_forward((u8*)mem, memsize, allocator);
        return forwardRingAllocator;
    }
}; // namespace ncore
