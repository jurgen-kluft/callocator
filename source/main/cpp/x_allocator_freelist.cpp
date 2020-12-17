#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_memory.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"

#include "xallocator/private/x_freelist.h"

namespace xcore
{
    namespace xfreelist_allocator
    {

        /**
        @brief	xallocator_imp is a fast allocator for objects of fixed size.

        @desc	It preallocates (from @allocator) @inInitialBlockCount blocks with @inBlockSize T elements.
                By calling allocate() an application can fetch one T object. By calling deallocate()
                an application returns one T object to pool.

        @note	This allocator does not guarantee that two objects allocated sequentially are sequential in memory.
        **/
        class xallocator_imp : public fsadexed_t
        {
        public:
            xallocator_imp();

            // @inElemSize			This determines the size in bytes of an element
            // @inElemAlignment		Alignment of the start of each pool (can be 0, which creates fixed size memory pool)
            // @inBlockElemCnt		This determines the number of elements that are part of a block
            xallocator_imp(alloc_t* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
            xallocator_imp(alloc_t* allocator, void* mem_block, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
            virtual ~xallocator_imp();

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

            alloc_t*      allocator() const { return (alloc_t*)mAllocator; }

            ///@name	Placement new/delete
            XCORE_CLASS_PLACEMENT_NEW_DELETE

        protected:
            alloc_t*     mAllocator;
            void*       mElementArray;
            xfreelist_t mFreeList;
            u32         mElemSize;
            u32         mAllocCount;

        private:
            // Copy construction and assignment are forbidden
            xallocator_imp(const xallocator_imp&);
            xallocator_imp& operator=(const xallocator_imp&);
        };

        xallocator_imp::xallocator_imp() : mAllocator(NULL), mElementArray(NULL), mFreeList(), mAllocCount(0) {}

        xallocator_imp::xallocator_imp(alloc_t* allocator, u32 inElemSize, u32 inElemAlignment, u32 inMaxNumElements) : mAllocator(allocator), mElementArray(NULL), mFreeList(), mAllocCount(0)
        {
            mElemSize = inElemSize;
            mFreeList.init_with_alloc(allocator, inElemSize, inElemAlignment, inMaxNumElements);
        }

        xallocator_imp::xallocator_imp(alloc_t* allocator, void* inElementArray, u32 inElemSize, u32 inElemAlignment, u32 inMaxNumElements) : mAllocator(allocator), mElementArray(inElementArray), mFreeList(), mAllocCount(0)
        {
            mElemSize = inElemSize;
            mFreeList.init_with_array((xcore::xbyte*)inElementArray, inMaxNumElements * inElemSize, inElemSize, inElemAlignment);
        }

        xallocator_imp::~xallocator_imp() { ASSERT(mAllocCount == 0); }

        void xallocator_imp::init()
        {
            mAllocCount = 0;
            mFreeList.init_list();
        }

        void xallocator_imp::clear()
        {
            mAllocCount = 0;
            mFreeList.init_list();
        }

        void xallocator_imp::exit()
        {
            ASSERT(mAllocCount == 0);
            mFreeList.release();
        }

        void* xallocator_imp::v_allocate()
        {
            ASSERT((u32)mElemSize <= mFreeList.getElemSize());
            void* p = mFreeList.alloc(); // Will return NULL if no more memory available
            if (p != NULL)
                ++mAllocCount;
            return p;
        }

        u32 xallocator_imp::v_deallocate(void* inObject)
        {
            // Check input parameters
            if (inObject == NULL)
                return 0;
            mFreeList.free((xfreelist_t::xitem_t*)inObject);
            --mAllocCount;
			return mElemSize;
        }

        u32 xallocator_imp::v_ptr2idx(void* p) const
        {
            // Check input parameters
            ASSERT(p != NULL);
            return mFreeList.idx_of((xfreelist_t::xitem_t*)p);
        }

        void* xallocator_imp::v_idx2ptr(u32 idx) const { return (void*)mFreeList.ptr_of(idx); }

        void xallocator_imp::v_release()
        {
            exit();
            mAllocator->deallocate(this);
        }

        class xiallocator_imp : public fsadexed_t
        {
            alloc_t*        mOurAllocator;
            xallocator_imp mAllocator;

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

            virtual u32 v_ptr2idx(void * p) const
            {
                u32 idx = mAllocator.ptr2idx(p);
                return idx;
            }

            ///@name	Placement new/delete
            XCORE_CLASS_PLACEMENT_NEW_DELETE
        };

        xiallocator_imp::xiallocator_imp() : mOurAllocator(NULL) {}

        xiallocator_imp::xiallocator_imp(alloc_t* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt) : mOurAllocator(allocator), mAllocator(allocator, inElemSize, inElemAlignment, inBlockElemCnt) {}

        xiallocator_imp::xiallocator_imp(alloc_t* allocator, void* inElementArray, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt) : mOurAllocator(allocator), mAllocator(allocator, inElementArray, inElemSize, inElemAlignment, inBlockElemCnt) {}

    } // namespace xfreelist_allocator

    fsadexed_t* gCreateFreeListAllocator(alloc_t* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
    {
        void*                                mem        = allocator->allocate(sizeof(xfreelist_allocator::xallocator_imp), X_ALIGNMENT_DEFAULT);
        xfreelist_allocator::xallocator_imp* _allocator = new (mem) xfreelist_allocator::xallocator_imp(allocator, inSizeOfElement, inElementAlignment, inNumElements);
        _allocator->init();
        return _allocator;
    }

    fsadexed_t* gCreateFreeListAllocator(alloc_t* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
    {
        void*                                mem        = allocator->allocate(sizeof(xfreelist_allocator::xallocator_imp), X_ALIGNMENT_DEFAULT);
        xfreelist_allocator::xallocator_imp* _allocator = new (mem) xfreelist_allocator::xallocator_imp(allocator, inElementArray, inSizeOfElement, inElementAlignment, inNumElements);
        _allocator->init();
        return _allocator;
    }

    fsadexed_t* gCreateFreeListIdxAllocator(alloc_t* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
    {
        void*                                 mem        = allocator->allocate(sizeof(xfreelist_allocator::xiallocator_imp), X_ALIGNMENT_DEFAULT);
        xfreelist_allocator::xiallocator_imp* _allocator = new (mem) xfreelist_allocator::xiallocator_imp(allocator, inSizeOfElement, inElementAlignment, inNumElements);
        _allocator->init();
        return _allocator;
    }

    fsadexed_t* gCreateFreeListIdxAllocator(alloc_t* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
    {
        void*                                 mem        = allocator->allocate(sizeof(xfreelist_allocator::xiallocator_imp), X_ALIGNMENT_DEFAULT);
        xfreelist_allocator::xiallocator_imp* _allocator = new (mem) xfreelist_allocator::xiallocator_imp(allocator, inElementArray, inSizeOfElement, inElementAlignment, inNumElements);
        _allocator->init();
        return _allocator;
    }
}; // namespace xcore
