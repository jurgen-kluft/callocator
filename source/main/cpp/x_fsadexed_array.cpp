#include "xbase/x_target.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"
#include "xbase/x_memory.h"

namespace xcore
{
    class x_fsadexed_allocator : public xfsadexed
    {
    public:
        x_fsadexed_allocator(xalloc* allocator) : mAllocator(allocator) {}

        void initialize(void* object_array, u32 size_of_object, u32 object_alignment, u32 size);
        void initialize(xalloc* allocator, u32 size_of_object, u32 object_alignment, u32 size);

        XCORE_CLASS_PLACEMENT_NEW_DELETE

    protected:
        enum
        {
            NILL_IDX = 0xffffffff
        };

        void init_freelist();

        virtual void init();
        virtual void clear();

        virtual u32   v_size() const { return mSizeOfObject; }
        virtual void* v_allocate();
        virtual u32   v_deallocate(void* p);
        virtual void* v_idx2ptr(u32 idx) const;
        virtual u32   v_ptr2idx(void* p) const;

        virtual void v_release();

    private:
        xalloc* mAllocator;
        u32*    mFreeObjectList;
        u32     mAllocCount;
        xalloc* mObjectArrayAllocator;
        u32     mObjectArraySize;
        xbyte*  mObjectArray;
        xbyte*  mObjectArrayEnd;
        u32     mSizeOfObject;
        u32     mAlignOfObject;
    };

    xfsadexed* gCreateArrayIdxAllocator(xalloc* allocator, xalloc* object_array_allocator, u32 size_of_object, u32 object_alignment, u32 size)
    {
        void*                 mem             = allocator->allocate(sizeof(x_fsadexed_allocator), 4);
        x_fsadexed_allocator* array_allocator = new (mem) x_fsadexed_allocator(allocator);
        array_allocator->initialize(object_array_allocator, size_of_object, object_alignment, size);
        return array_allocator;
    }

    xfsadexed* gCreateArrayIdxAllocator(xalloc* allocator, void* object_array, u32 size_of_object, u32 object_alignment, u32 size)
    {
        void*                      mem             = allocator->allocate(sizeof(x_fsadexed_allocator), 4);
        x_fsadexed_allocator* array_allocator = new (mem) x_fsadexed_allocator(allocator);
        array_allocator->initialize(object_array, size_of_object, object_alignment, size);
        return array_allocator;
    }

    void x_fsadexed_allocator::init_freelist()
    {
        u32* object_array = (u32*)mObjectArray;
        for (u32 i = 1; i < mObjectArraySize; ++i)
        {
            *object_array = i;
            object_array += mSizeOfObject / 4;
        }
        *object_array   = NILL_IDX;
        mFreeObjectList = (u32*)mObjectArray;
    }

    void x_fsadexed_allocator::initialize(void* object_array, u32 size_of_object, u32 object_alignment, u32 size)
    {
        object_alignment = xalignUp(object_alignment, 4);

        mFreeObjectList       = NULL;
        mAllocCount           = 0;
        mObjectArrayAllocator = NULL;
        mObjectArraySize      = size;
        mSizeOfObject         = xalignUp(size_of_object, object_alignment);
        mObjectArray          = (xbyte*)object_array;
        mObjectArrayEnd       = mObjectArray + (mObjectArraySize * mSizeOfObject);
    }

    void x_fsadexed_allocator::initialize(xalloc* allocator, u32 size_of_object, u32 object_alignment, u32 size)
    {
        mFreeObjectList       = NULL;
        mAllocCount           = 0;
        mObjectArray          = NULL;
        mObjectArrayAllocator = allocator;
        mObjectArraySize      = size;

        mAlignOfObject = xalignUp(object_alignment, 4);
        mSizeOfObject  = xalignUp(size_of_object, mAlignOfObject);
    }

    void x_fsadexed_allocator::init()
    {
        clear();

        if (mObjectArray == NULL)
        {
            mObjectArray    = (xbyte*)mObjectArrayAllocator->allocate(mSizeOfObject * mObjectArraySize, mAlignOfObject);
            mObjectArrayEnd = mObjectArray + (mObjectArraySize * mSizeOfObject);
        }
        init_freelist();
    }

    void x_fsadexed_allocator::clear()
    {
        ASSERT(mAllocCount == 0);
        if (mObjectArrayAllocator != NULL)
        {
            if (mObjectArray != NULL)
            {
                mObjectArrayAllocator->deallocate(mObjectArray);
                mObjectArray    = NULL;
                mObjectArrayEnd = NULL;
            }
        }

        mFreeObjectList = NULL;
    }

    void* x_fsadexed_allocator::v_allocate()
    {
        void* p = nullptr;
        if (mFreeObjectList == NULL)
        {
            p = NULL;
            return p;
        }

        u32 idx = (u32)((uptr)mFreeObjectList - (uptr)mObjectArray) / mSizeOfObject;
        p       = (void*)mFreeObjectList;

        u32 next_object = *mFreeObjectList;
        if (next_object != NILL_IDX)
            mFreeObjectList = (u32*)((uptr)mObjectArray + (next_object * mSizeOfObject));
        else
            mFreeObjectList = NULL;

        ++mAllocCount;
        return p;
    }

    u32 x_fsadexed_allocator::v_deallocate(void* ptr)
    {
        u32 idx = ptr2idx(ptr);
        if (idx < mObjectArraySize)
        {
            u32* free_object = (u32*)(mObjectArray + (mSizeOfObject * idx));
            *free_object     = ptr2idx(mFreeObjectList);
            mFreeObjectList  = free_object;
            --mAllocCount;
        }
        return mSizeOfObject;
    }

    void* x_fsadexed_allocator::v_idx2ptr(u32 idx) const
    {
        if (idx == NILL_IDX)
            return NULL;
        ASSERT(mObjectArray != NULL && idx < mObjectArraySize);
        void* p = (void*)((uptr)mObjectArray + (mSizeOfObject * idx));
        return p;
    }

    u32 x_fsadexed_allocator::v_ptr2idx(void* p) const
    {
        ASSERT(mObjectArray != NULL && mObjectArrayEnd != NULL);
        if ((xbyte*)p >= mObjectArray && (xbyte*)p < mObjectArrayEnd)
        {
            u32 idx = (u32)((uptr)p - (uptr)mObjectArray) / mSizeOfObject;
            return idx;
        }
        else
        {
            return NILL_IDX;
        }
    }

    void x_fsadexed_allocator::v_release()
    {
        clear();
        this->~x_fsadexed_allocator();
        mAllocator->deallocate(this);
    }
} // namespace xcore
