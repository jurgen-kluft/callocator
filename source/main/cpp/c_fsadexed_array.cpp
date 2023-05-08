#include "ccore/c_target.h"
#include "cbase/c_integer.h"
#include "cbase/c_allocator.h"
#include "cbase/c_memory.h"

namespace ncore
{
    class fsadexed_allocator : public fsadexed_t
    {
    public:
        fsadexed_allocator(alloc_t* allocator) : mAllocator(allocator) {}

        void initialize(void* object_array, u32 size_of_object, u32 object_alignment, u32 size);
        void initialize(alloc_t* allocator, u32 size_of_object, u32 object_alignment, u32 size);

        DCORE_CLASS_PLACEMENT_NEW_DELETE

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
        alloc_t* mAllocator;
        u32*     mFreeObjectList;
        u32      mAllocCount;
        alloc_t* mObjectArrayAllocator;
        u32      mObjectArraySize;
        u8*   mObjectArray;
        u8*   mObjectArrayEnd;
        u32      mSizeOfObject;
        u32      mAlignOfObject;
    };

    fsadexed_t* gCreateArrayIdxAllocator(alloc_t* allocator, alloc_t* object_array_allocator, u32 size_of_object, u32 object_alignment, u32 size)
    {
        void*                 mem             = allocator->allocate(sizeof(fsadexed_allocator), 4);
        fsadexed_allocator* array_allocator = new (mem) fsadexed_allocator(allocator);
        array_allocator->initialize(object_array_allocator, size_of_object, object_alignment, size);
        return array_allocator;
    }

    fsadexed_t* gCreateArrayIdxAllocator(alloc_t* allocator, void* object_array, u32 size_of_object, u32 object_alignment, u32 size)
    {
        void*                 mem             = allocator->allocate(sizeof(fsadexed_allocator), 4);
        fsadexed_allocator* array_allocator = new (mem) fsadexed_allocator(allocator);
        array_allocator->initialize(object_array, size_of_object, object_alignment, size);
        return array_allocator;
    }

    void fsadexed_allocator::init_freelist()
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

    void fsadexed_allocator::initialize(void* object_array, u32 size_of_object, u32 object_alignment, u32 size)
    {
        object_alignment = math::alignUp(object_alignment, (u32)4);

        mFreeObjectList       = nullptr;
        mAllocCount           = 0;
        mObjectArrayAllocator = nullptr;
        mObjectArraySize      = size;
        mSizeOfObject         = math::alignUp(size_of_object, object_alignment);
        mObjectArray          = (u8*)object_array;
        mObjectArrayEnd       = mObjectArray + (mObjectArraySize * mSizeOfObject);
    }

    void fsadexed_allocator::initialize(alloc_t* allocator, u32 size_of_object, u32 object_alignment, u32 size)
    {
        mFreeObjectList       = nullptr;
        mAllocCount           = 0;
        mObjectArray          = nullptr;
        mObjectArrayAllocator = allocator;
        mObjectArraySize      = size;

        mAlignOfObject = math::alignUp(object_alignment, (u32)4);
        mSizeOfObject  = math::alignUp(size_of_object, mAlignOfObject);
    }

    void fsadexed_allocator::init()
    {
        clear();

        if (mObjectArray == nullptr)
        {
            mObjectArray    = (u8*)mObjectArrayAllocator->allocate(mSizeOfObject * mObjectArraySize, mAlignOfObject);
            mObjectArrayEnd = mObjectArray + (mObjectArraySize * mSizeOfObject);
        }
        init_freelist();
    }

    void fsadexed_allocator::clear()
    {
        ASSERT(mAllocCount == 0);
        if (mObjectArrayAllocator != nullptr)
        {
            if (mObjectArray != nullptr)
            {
                mObjectArrayAllocator->deallocate(mObjectArray);
                mObjectArray    = nullptr;
                mObjectArrayEnd = nullptr;
            }
        }

        mFreeObjectList = nullptr;
    }

    void* fsadexed_allocator::v_allocate()
    {
        void* p = nullptr;
        if (mFreeObjectList == nullptr)
        {
            p = nullptr;
            return p;
        }

        u32 idx = (u32)((ptr_t)mFreeObjectList - (ptr_t)mObjectArray) / mSizeOfObject;
        p       = (void*)mFreeObjectList;

        u32 next_object = *mFreeObjectList;
        if (next_object != NILL_IDX)
            mFreeObjectList = (u32*)((ptr_t)mObjectArray + (next_object * mSizeOfObject));
        else
            mFreeObjectList = nullptr;

        ++mAllocCount;
        return p;
    }

    u32 fsadexed_allocator::v_deallocate(void* ptr)
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

    void* fsadexed_allocator::v_idx2ptr(u32 idx) const
    {
        if (idx == NILL_IDX)
            return nullptr;
        ASSERT(mObjectArray != nullptr && idx < mObjectArraySize);
        void* p = (void*)((ptr_t)mObjectArray + (mSizeOfObject * idx));
        return p;
    }

    u32 fsadexed_allocator::v_ptr2idx(void* p) const
    {
        ASSERT(mObjectArray != nullptr && mObjectArrayEnd != nullptr);
        if ((u8*)p >= mObjectArray && (u8*)p < mObjectArrayEnd)
        {
            u32 idx = (u32)((ptr_t)p - (ptr_t)mObjectArray) / mSizeOfObject;
            return idx;
        }
        else
        {
            return NILL_IDX;
        }
    }

    void fsadexed_allocator::v_release()
    {
        clear();
        this->~fsadexed_allocator();
        mAllocator->deallocate(this);
    }
} // namespace ncore
