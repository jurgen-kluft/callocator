#include "xbase/x_target.h"
#include "xbase/x_memory.h"
#include "xbase/x_limits.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"
#include "xbase/x_integer.h"

#include "xallocator/x_allocator_forward.h"
#include "xallocator/private/x_forwardbin.h"

namespace ncore
{

    class x_allocator_forward : public alloc_t
    {
    public:
        x_allocator_forward();
        x_allocator_forward(u8* beginAddress, u32 size, alloc_t* allocator);
        virtual ~x_allocator_forward();

        virtual const char* name() const { return TARGET_FULL_DESCR_STR "[Allocator, Type=Forward]"; }

        void         initialize(void* beginAddress, u32 size);
        virtual void v_release();

        virtual void* v_allocate(u32 size, u32 alignment);
        virtual u32   v_deallocate(void* ptr);

        XCORE_CLASS_PLACEMENT_NEW_DELETE

    private:
        alloc_t*                mAllocator;
        u32                     mTotalSize;
        u8*                  mMemBegin;
        xforwardbin::xallocator mForwardAllocator;

        x_allocator_forward(const x_allocator_forward&);
        x_allocator_forward& operator=(const x_allocator_forward&);
    };

    x_allocator_forward::x_allocator_forward() : mAllocator(nullptr), mTotalSize(0), mMemBegin(nullptr) {}

    x_allocator_forward::x_allocator_forward(u8* beginAddress, u32 size, alloc_t* allocator) : mAllocator(allocator), mTotalSize(size), mMemBegin(beginAddress) { mForwardAllocator.init(mMemBegin, mMemBegin + size); }

    x_allocator_forward::~x_allocator_forward() { release(); }

    void x_allocator_forward::v_release()
    {
        mAllocator->deallocate(mMemBegin);
        mAllocator->deallocate(this);
    }

    void* x_allocator_forward::v_allocate(u32 size, u32 alignment)
    {
        ASSERT(size < X_U32_MAX);
        return mForwardAllocator.allocate((u32)size, alignment);
    }

    u32 x_allocator_forward::v_deallocate(void* ptr) { return mForwardAllocator.deallocate(ptr); }

    alloc_t* gCreateForwardAllocator(alloc_t* allocator, u32 memsize)
    {
        void*                memForAllocator      = allocator->allocate(sizeof(x_allocator_forward), sizeof(void*));
        void*                mem                  = allocator->allocate(memsize + 32, sizeof(void*));
        x_allocator_forward* forwardRingAllocator = new (memForAllocator) x_allocator_forward((u8*)mem, memsize, allocator);
        return forwardRingAllocator;
    }
}; // namespace ncore
