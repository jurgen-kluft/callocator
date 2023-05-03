#include "cbase/c_target.h"
#include "cbase/c_memory.h"
#include "cbase/c_limits.h"
#include "cbase/c_integer.h"
#include "cbase/c_allocator.h"
#include "cbase/c_integer.h"

#include "callocator/c_allocator_forward.h"
#include "callocator/private/c_forwardbin.h"

namespace ncore
{
    class allocator_forward : public alloc_t
    {
    public:
        allocator_forward();
        allocator_forward(u8* beginAddress, u32 size, alloc_t* allocator);
        virtual ~allocator_forward();

        virtual const char* name() const { return TARGET_FULL_DESCR_STR "[Allocator, Type=Forward]"; }

        void         initialize(void* beginAddress, u32 size);
        virtual void v_release();

        virtual void* v_allocate(u32 size, u32 alignment);
        virtual u32   v_deallocate(void* ptr);

        DCORE_CLASS_PLACEMENT_NEW_DELETE

    private:
        alloc_t*               mAllocator;
        u32                    mTotalSize;
        u8*                    mMemBegin;
        forwardbin::allocator mForwardAllocator;

        allocator_forward(const allocator_forward&);
        allocator_forward& operator=(const allocator_forward&);
    };

    allocator_forward::allocator_forward() : mAllocator(nullptr), mTotalSize(0), mMemBegin(nullptr) {}

    allocator_forward::allocator_forward(u8* beginAddress, u32 size, alloc_t* allocator) : mAllocator(allocator), mTotalSize(size), mMemBegin(beginAddress) { mForwardAllocator.init(mMemBegin, mMemBegin + size); }

    allocator_forward::~allocator_forward() { release(); }

    void allocator_forward::v_release()
    {
        mAllocator->deallocate(mMemBegin);
        mAllocator->deallocate(this);
    }

    void* allocator_forward::v_allocate(u32 size, u32 alignment)
    {
        ASSERT(size < D_U32_MAX);
        return mForwardAllocator.allocate((u32)size, alignment);
    }

    u32 allocator_forward::v_deallocate(void* ptr) { return mForwardAllocator.deallocate(ptr); }

    alloc_t* gCreateForwardAllocator(alloc_t* allocator, u32 memsize)
    {
        void*              memForAllocator      = allocator->allocate(sizeof(allocator_forward), sizeof(void*));
        void*              mem                  = allocator->allocate(memsize + 32, sizeof(void*));
        allocator_forward* forwardRingAllocator = new (memForAllocator) allocator_forward((u8*)mem, memsize, allocator);
        return forwardRingAllocator;
    }
}; // namespace ncore
