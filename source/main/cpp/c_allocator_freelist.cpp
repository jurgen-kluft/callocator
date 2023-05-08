#include "ccore/c_target.h"
#include "ccore/c_debug.h"
#include "cbase/c_memory.h"
#include "cbase/c_integer.h"
#include "cbase/c_allocator.h"

#include "callocator/private/c_freelist.h"

namespace ncore
{
    namespace freelist_allocator
    {

        /**
        @brief	allocator_imp is a fast allocator for objects of fixed size.

        @desc	It preallocates (from @allocator) @inInitialBlockCount blocks with @inBlockSize T elements.
                By calling allocate() an application can fetch one T object. By calling deallocate()
                an application returns one T object to pool.

        @note	This allocator does not guarantee that two objects allocated sequentially are sequential in memory.
        **/
        class allocator_imp : public fsadexed_t
        {
        public:
            allocator_imp();

            // @inElemSize			This determines the size in bytes of an element
            // @inElemAlignment		Alignment of the start of each pool (can be 0, which creates fixed size memory pool)
            // @inBlockElemCnt		This determines the number of elements that are part of a block
            allocator_imp(alloc_t* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
            allocator_imp(alloc_t* allocator, void* mem_block, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
            virtual ~allocator_imp();

            virtual const char* name() const { return TARGET_FULL_DESCR_STR " [Allocator, Type=freelist]"; }

            ///@name	Should be called when created with default constructor
            //			Parameters are the same as for constructor with parameters
            void init();
            void clear();
            void exit();

            virtual u32   v_size() const { return mAllocCount; }
            virtual void* v_allocate();
            virtual u32   v_deallocate(void* p);
            virtual u32   v_ptr2idx(void* p) const;
            virtual void* v_idx2ptr(u32 idx) const;
            virtual void  v_release();

            alloc_t* allocator() const { return (alloc_t*)mAllocator; }

            ///@name	Placement new/delete
            DCORE_CLASS_PLACEMENT_NEW_DELETE

        protected:
            alloc_t*   mAllocator;
            void*      mElementArray;
            freelist_t mFreeList;
            u32        mElemSize;
            u32        mAllocCount;

        private:
            // Copy construction and assignment are forbidden
            allocator_imp(const allocator_imp&);
            allocator_imp& operator=(const allocator_imp&);
        };

        allocator_imp::allocator_imp() : mAllocator(nullptr), mElementArray(nullptr), mFreeList(), mAllocCount(0) {}

        allocator_imp::allocator_imp(alloc_t* allocator, u32 inElemSize, u32 inElemAlignment, u32 inMaxNumElements) : mAllocator(allocator), mElementArray(nullptr), mFreeList(), mAllocCount(0)
        {
            mElemSize = inElemSize;
            mFreeList.init_with_alloc(allocator, inElemSize, inElemAlignment, inMaxNumElements);
        }

        allocator_imp::allocator_imp(alloc_t* allocator, void* inElementArray, u32 inElemSize, u32 inElemAlignment, u32 inMaxNumElements) : mAllocator(allocator), mElementArray(inElementArray), mFreeList(), mAllocCount(0)
        {
            mElemSize = inElemSize;
            mFreeList.init_with_array((ncore::u8*)inElementArray, inMaxNumElements * inElemSize, inElemSize, inElemAlignment);
        }

        allocator_imp::~allocator_imp() { ASSERT(mAllocCount == 0); }

        void allocator_imp::init()
        {
            mAllocCount = 0;
            mFreeList.init_list();
        }

        void allocator_imp::clear()
        {
            mAllocCount = 0;
            mFreeList.init_list();
        }

        void allocator_imp::exit()
        {
            ASSERT(mAllocCount == 0);
            mFreeList.release();
        }

        void* allocator_imp::v_allocate()
        {
            ASSERT((u32)mElemSize <= mFreeList.getElemSize());
            void* p = mFreeList.alloc(); // Will return nullptr if no more memory available
            if (p != nullptr)
                ++mAllocCount;
            return p;
        }

        u32 allocator_imp::v_deallocate(void* inObject)
        {
            // Check input parameters
            if (inObject == nullptr)
                return 0;
            mFreeList.free((freelist_t::item_t*)inObject);
            --mAllocCount;
            return mElemSize;
        }

        u32 allocator_imp::v_ptr2idx(void* p) const
        {
            // Check input parameters
            ASSERT(p != nullptr);
            return mFreeList.idx_of((freelist_t::item_t*)p);
        }

        void* allocator_imp::v_idx2ptr(u32 idx) const { return (void*)mFreeList.ptr_of(idx); }

        void allocator_imp::v_release()
        {
            exit();
            mAllocator->deallocate(this);
        }

        class xiallocator_imp : public fsadexed_t
        {
            alloc_t*      mOurAllocator;
            allocator_imp mAllocator;

        public:
            xiallocator_imp();
            xiallocator_imp(alloc_t* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
            xiallocator_imp(alloc_t* allocator, void* mem_block, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);

            virtual const char* name() const { return TARGET_FULL_DESCR_STR " [Allocator, Type=freelist,indexed]"; }

            virtual void* v_allocate() { return mAllocator.allocate(); }
            virtual u32   v_deallocate(void* p) { return mAllocator.deallocate(p); }
            virtual void  v_release()
            {
                mAllocator.exit();
                mOurAllocator->deallocate(this);
            }

            virtual void init() { mAllocator.init(); }
            virtual void clear() { mAllocator.clear(); }

            virtual u32 v_size() const { return mAllocator.size(); }

            virtual u32 iallocate(void*& p)
            {
                p = allocate();
                return mAllocator.ptr2idx(p);
            }

            virtual void ideallocate(u32 idx)
            {
                void* p = mAllocator.idx2ptr(idx);
                mAllocator.deallocate(p);
            }

            virtual void* v_idx2ptr(u32 idx) const
            {
                void* p = mAllocator.idx2ptr(idx);
                return p;
            }

            virtual u32 v_ptr2idx(void* p) const
            {
                u32 idx = mAllocator.ptr2idx(p);
                return idx;
            }

            ///@name	Placement new/delete
            DCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        xiallocator_imp::xiallocator_imp() : mOurAllocator(nullptr) {}

        xiallocator_imp::xiallocator_imp(alloc_t* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt) : mOurAllocator(allocator), mAllocator(allocator, inElemSize, inElemAlignment, inBlockElemCnt) {}

        xiallocator_imp::xiallocator_imp(alloc_t* allocator, void* inElementArray, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt) : mOurAllocator(allocator), mAllocator(allocator, inElementArray, inElemSize, inElemAlignment, inBlockElemCnt) {}

    } // namespace freelist_allocator

    fsadexed_t* gCreateFreeListAllocator(alloc_t* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
    {
        void*                              mem        = allocator->allocate(sizeof(freelist_allocator::allocator_imp), D_ALIGNMENT_DEFAULT);
        freelist_allocator::allocator_imp* _allocator = new (mem) freelist_allocator::allocator_imp(allocator, inSizeOfElement, inElementAlignment, inNumElements);
        _allocator->init();
        return _allocator;
    }

    fsadexed_t* gCreateFreeListAllocator(alloc_t* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
    {
        void*                              mem        = allocator->allocate(sizeof(freelist_allocator::allocator_imp), D_ALIGNMENT_DEFAULT);
        freelist_allocator::allocator_imp* _allocator = new (mem) freelist_allocator::allocator_imp(allocator, inElementArray, inSizeOfElement, inElementAlignment, inNumElements);
        _allocator->init();
        return _allocator;
    }

    fsadexed_t* gCreateFreeListIdxAllocator(alloc_t* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
    {
        void*                                mem        = allocator->allocate(sizeof(freelist_allocator::xiallocator_imp), D_ALIGNMENT_DEFAULT);
        freelist_allocator::xiallocator_imp* _allocator = new (mem) freelist_allocator::xiallocator_imp(allocator, inSizeOfElement, inElementAlignment, inNumElements);
        _allocator->init();
        return _allocator;
    }

    fsadexed_t* gCreateFreeListIdxAllocator(alloc_t* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
    {
        void*                                mem        = allocator->allocate(sizeof(freelist_allocator::xiallocator_imp), D_ALIGNMENT_DEFAULT);
        freelist_allocator::xiallocator_imp* _allocator = new (mem) freelist_allocator::xiallocator_imp(allocator, inElementArray, inSizeOfElement, inElementAlignment, inNumElements);
        _allocator->init();
        return _allocator;
    }
}; // namespace ncore
