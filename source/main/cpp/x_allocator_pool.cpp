#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_memory_std.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"
#include "xbase/x_tree.h"

#include "xallocator/x_allocator_pool.h"

namespace xcore
{
	void	xfsa_params::set_elem_size(u32 size)									{ mElemSize = size; }
	void	xfsa_params::set_elem_alignment(u32 alignment)							{ mElemAlignment = alignment; }
	void	xfsa_params::set_block_min_count(u32 min_num_blocks)					{ mMinNumberOfBlocks = min_num_blocks; }
	void	xfsa_params::set_block_max_count(u32 max_num_blocks)					{ mMaxNumberOfBlocks = max_num_blocks; }

	u32		xfsa_params::get_elem_size() const										{ return mElemSize; }
	u32		xfsa_params::get_elem_alignment() const									{ return mElemAlignment; }
	u32		xfsa_params::get_block_min_count() const								{ return mMinNumberOfBlocks; }
	u32		xfsa_params::get_block_max_count() const								{ return mMaxNumberOfBlocks; }

	namespace xfsa_allocator
	{
		class xelement	
		{
		public:
			xelement*			getNext()							{ return *reinterpret_cast<xelement**>(&mData); }
			void				setNext(xelement* next)				{ xelement** temp = reinterpret_cast<xelement**>(&mData); *temp = next; }
			void*				getObject()							{ return (void*)&mData; }
		private:
			u32					mData;	
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
			virtual					~xfsallocator();

			virtual const char*		name() const									{ return TARGET_FULL_DESCR_STR " FSA"; }

			///@name	Should be called when created with default constructor
			//			Parameters are the same as for constructor with parameters
			void					init();

			virtual void*			allocate(u32& size);
			virtual void			deallocate(void* p);

			///@name	Placement new/delete
			XCORE_CLASS_PLACEMENT_NEW_DELETE

		protected:
			void					reset(xbool inRestoreToInitialSize = xFALSE);
			void					extend (u32 inBlockCount, u32 inBlockMaxCount);
			virtual void			release ();

		protected:
			bool					mIsInitialized;
			xalloc*					mAllocator;

			struct block_t
			{
				void*	m_block;
				u32*	m_freelist;
			};

			block_t					m

			xranges32				mBlocks;
			xranges32				mBlocksNotFull;

			xfsa_params				mParams;

		private:
			// Copy construction and assignment are forbidden
									xallocator_imp(const xallocator_imp&);
			xallocator_imp&			operator= (const xallocator_imp&);
		};

		xallocator_imp::xallocator_imp()
			: mIsInitialized(false)
			, mAllocator(NULL)
			, mBlocks()
			, mParams()
		{
		}

		xallocator_imp::xallocator_imp(xalloc* allocator, xfsa_params const& params)
			: mIsInitialized(false)
			, mAllocator(allocator)
			, mBlocks(allocator)
			, mParams(params)
		{
			init();
		}

		xallocator_imp::~xallocator_imp ()
		{
			ASSERT(mUsedItems==0);
		}

		void xallocator_imp::init()
		{
			// Check if memory pool is not already initialized
			ASSERT(mIsInitialized == false);

			// Check input parameters	
			ASSERT(mElemSize >= 4);
			ASSERT(mBlockElemCount > 0);
			ASSERT(mBlockInitialCount > 0);

			// Save initial parameters
			mElemAlignment     = mElemAlignment==0 ? X_ALIGNMENT_DEFAULT : mElemAlignment;
			mElemSize          = xalignUp(mElemSize, mElemAlignment);			// Align element size to a multiple of element alignment

			extend(mStaticBlocks, mBlockInitialCount, mBlockMaxCount);
		}

		void		xallocator_imp::reset(xbool inRestoreToInitialSize)
		{
			// Check if pool is empty
			ASSERT(mUsedItems == 0);

			if (inRestoreToInitialSize)
			{
				mDynamicBlocks.release(mAllocator);
				mStaticBlocks.reset(mBlockElemCount, mElemSize);
			}

			mUsedItems = 0;
		}

		void*		xallocator_imp::allocate(xsize_t size, u32 alignment)
		{
			ASSERT((u32)size <= mElemSize);

			void* p = mStaticBlocks.allocate();
			if (p == NULL)
			{
				p = mDynamicBlocks.allocate();
				if (p == NULL)
				{
					extend(mBlockGrowthCount, mBlockMaxCount);
					p = mDynamicBlocks.allocate();
				}
			}
			mUsedItems += p!=NULL ? 1 : 0;
			return p;
		}

		void*		xallocator_imp::reallocate(void* inObject, xsize_t size, u32 alignment)
		{
			ASSERT((u32)size <= mElemSize);
			ASSERT(alignment <= mElemAlignment);

			//TODO: We could reallocate from DynamicBlocks to StaticBlocks. That is if
			//      the incoming object comes from DynamicBlocks, if so and we can
			//      allocate from StaticBlocks we could reallocate it.

			return inObject;
		}

		void		xallocator_imp::deallocate(void* inObject)
		{
			// Check input parameters
			if (inObject == NULL)
				return;

			// First check the StaticBlocks
			s32 c = mStaticBlocks.deallocate(inObject);
			if (c==0)
				c = mDynamicBlocks.deallocate(inObject);
			
			ASSERTS(c==1, "Error: did not find that address in this pool allocator, are you sure you are deallocating with the right allocator?");
			mUsedItems -= c;
		}

		void		xallocator_imp::extend(Blocks& blocks, u32 inBlockCount, u32 inBlockMaxCount) const
		{
			// In case of fixed pool it is legal to call extend with blockCount = 0
			if (inBlockCount == 0)
				return;

			for (u32 i=0; i<inBlockCount; ++i)
			{
				if (blocks.size() >= inBlockMaxCount)
					return;
				xblock* new_block = xblock::create(mAllocator, mElemSize, mBlockElemCount, mElemAlignment);
				blocks.mFree.push(new_block);
				blocks.mSize += 1;
			}
		}

		void	xallocator_imp::release()
		{
			ASSERT(mUsedItems == 0);

			mDynamicBlocks.release(mAllocator);
			mStaticBlocks.release(mAllocator);

			mAllocator->deallocate(this);
		}

	}	// End of xpool_allocator namespace

	xalloc*		gCreatePoolAllocator(xalloc* allocator, xpool_params const& params)
	{
		void* mem = allocator->allocate(sizeof(xpool_allocator::xallocator_imp), X_ALIGNMENT_DEFAULT);
		xpool_allocator::xallocator_imp* pool_allocator = new (mem) xpool_allocator::xallocator_imp(allocator, params.get_elem_size(), params.get_block_size(), params.get_block_initial_count(), params.get_block_growth_count(), params.get_block_max_count(), params.get_elem_alignment());
		pool_allocator->init();
		return pool_allocator;
	}

};
