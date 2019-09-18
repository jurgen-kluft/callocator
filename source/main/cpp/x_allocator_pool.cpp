#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_memory_std.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"
#include "xbase/x_tree.h"

#include "xallocator/x_allocator_pool.h"

namespace xcore
{
    void xfsa_params::set_elem_size(u32 size) { mElemSize = size; }
    void xfsa_params::set_elem_alignment(u32 alignment) { mElemAlignment = alignment; }
    void xfsa_params::set_block_min_count(u32 min_num_blocks) { mMinNumberOfBlocks = min_num_blocks; }
    void xfsa_params::set_block_max_count(u32 max_num_blocks) { mMaxNumberOfBlocks = max_num_blocks; }

    u32 xfsa_params::get_elem_size() const { return mElemSize; }
    u32 xfsa_params::get_elem_alignment() const { return mElemAlignment; }
    u32 xfsa_params::get_block_min_count() const { return mMinNumberOfBlocks; }
    u32 xfsa_params::get_block_max_count() const { return mMaxNumberOfBlocks; }

    namespace xfsa_allocator
    {
        class xelement
        {
        public:
            xelement* getNext() { return *reinterpret_cast<xelement**>(&mData); }
            void      setNext(xelement* next)
            {
                xelement** temp = reinterpret_cast<xelement**>(&mData);
                *temp           = next;
            }
            void* getObject() { return (void*)&mData; }

        private:
            u32 mData;
        };

        /**
        @brief	xallocator_imp is a fast allocator for objects of fixed size.

        @desc	It preallocates (from @allocator) @inInitialBlockCount blocks with @inBlockSize T elements.
                By calling Allocate an application can fetch one T object. By calling Deallocate
                an application returns one T object to pool.
                When all objects on the pool are used, pool can grow to accommodate new requests.
                @inGrowthCount specifies by how many blocks the pool will grow.
                Reset reclaims all objects and reinitializes the pool. The parameter RestoreToInitialSize
                can be used to resize the pool to initial size in case it grew.

        @note	This allocator does not guarantee that two objects allocated sequentially are sequential in memory.
        **/
        class xfsallocator : public xfsalloc
        {
        public:
            xfsallocator();

            // @inElemSize			This determines the size in bytes of an element
            // @inBlockElemCnt		This determines the number of elements that are part of a block
            // @inInitialBlockCount	Initial number of blocks in the memory pool
            // @inBlockGrowthCount	Number of blocks by which it will grow if all space is used
            // @inElemAlignment		Alignment of the start of each pool (can be 0, which creates fixed size memory pool)
            xfsallocator(xalloc* allocator, xfsa_params const& params);
            virtual ~xfsallocator();

            ///@name	Should be called when created with default constructor
            //			Parameters are the same as for constructor with parameters
            void init();

            virtual void* allocate(u32& size);
            virtual void  deallocate(void* p);

            ///@name	Placement new/delete
            XCORE_CLASS_PLACEMENT_NEW_DELETE

        protected:
            void         reset();
            void         extend();
            virtual void release();

        protected:
            bool    mIsInitialized;
            xalloc* mAllocator;

            s32        mAllocCount;
            s32        mNumBins;
            xsmallbin* mBins;
            u32*       mBinsSorted;
            xbitlist   mBinsNotFull;
            xbitlist   mBinsEmpty;

            xfsa_params mParams;

        private:
            // Copy construction and assignment are forbidden
            xallocator_imp(const xallocator_imp&);
            xallocator_imp& operator=(const xallocator_imp&);
        };

        xallocator_imp::xallocator_imp() : mIsInitialized(false), mAllocator(NULL), mAllocCount(0), mNumBins(0), mBins(nullptr), mParams() {}

        xallocator_imp::xallocator_imp(xalloc* allocator, xfsa_params const& params) : mIsInitialized(false), mAllocator(allocator), mAllocCount(0), mNumBins(0), mBins(nullptr), mParams(params) { init(); }

        xallocator_imp::~xallocator_imp() {}

        void xallocator_imp::init()
        {
            // Check if memory pool is not already initialized
            ASSERT(mIsInitialized == false);

            // Check input parameters
            ASSERT(mParams.mElemSize >= 4);
            ASSERT(mParams.mBlockElemCount > 0);
            ASSERT(mParams.mBlockInitialCount > 0);

            extend();
        }

        void xallocator_imp::reset() {}

        void* xallocator_imp::allocate()
        {
            void* ptr = nullptr;

            u32 binIndex;
            if (mBinsNotFull.find(binIndex))
            {
                xsmallbin* bin = mBins[binIndex];
                ptr            = bin->allocate();
                if (bin->is_full())
                {
                    mBinsNotFull.set(binIndex);
                    extend();
                }
            }

            return p;
        }

        void xallocator_imp::deallocate(void* inObject)
        {
            // Check input parameters
            if (inObject == NULL)
                return;

            // Figure out the smallbin, binary search since
            // 'mBinsSorted' is sorted by address.
        }

        void xallocator_imp::extend()
        {
            u32 binIndex;
            if (mBinsEmpty.find(binIndex))
            {
                xfsalloc* binalloc = mParams.get_block_allocator();

                u32        binSize;
                void*      base = binalloc->allocate(binSize);
                xsmallbin* bin  = &mBins[binIndex];
                bin->init(base, binSize, mParams.mElemSize, mAllocator);
                mNumBins += 1;

                // In the mBinsSorted find the index where to insert this new bin

                mBinsNotFull.set(binIndex);
            }
        }

        void xallocator_imp::release()
        {
            ASSERT(mAllocCount == 0);

            // Release resources

            mAllocator->deallocate(this);
        }

    } // namespace xfsa_allocator

    xalloc* gCreatePoolAllocator(xalloc* allocator, xpool_params const& params)
    {
        void*                            mem            = allocator->allocate(sizeof(xpool_allocator::xallocator_imp), X_ALIGNMENT_DEFAULT);
        xpool_allocator::xallocator_imp* pool_allocator = new (mem) xpool_allocator::xallocator_imp(allocator, params);
        pool_allocator->init();
        return pool_allocator;
    }

}; // namespace xcore
