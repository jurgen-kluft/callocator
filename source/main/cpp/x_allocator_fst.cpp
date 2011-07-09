#include "xbase\x_target.h"
#include "xbase\x_types.h"
#include "xbase\x_debug.h"
#include "xbase\x_memory_std.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"

#include "xallocator\x_allocator.h"

namespace xcore
{
	/**
	@brief		Fixed size type, element
	@desc		It implements linked list behavior for free elements in the block.
	**/
	class x_fst_elem
	{
	public:
		// This part is a little bit dirty...
		x_fst_elem*			getNext()							{ return *reinterpret_cast<x_fst_elem**>(&mData); }
		void				setNext(x_fst_elem* next)			{ x_fst_elem** temp = reinterpret_cast<x_fst_elem**>(&mData); *temp = next; }
		void*				getObject()							{ return (void*)&mData; }
	private:
		u32					mData;	
	};


	/**
	@brief	x_fst_block contains an array of x_fst_elem objects. This is the smallest
			memory chunk which is allocated from system (via allocated/deallocator) and is the
			smallest unit by which the allocator can grow. The inElemSize and inNumElements 
			parameter sent to init function determines the size of the block.
	**/
	class x_fst_block
	{
	public:
							x_fst_block() : mElementArray (0) {}

		void				init(x_iallocator* allocator, u32 inElemSize, u32 inNumElements, s32 inAlignment);
		void				release(x_iallocator* allocator);

		x_fst_elem*			getAt(s32 inElemIndex, s32 inElemSize)
		{
			x_fst_elem*	elem = (x_fst_elem*)((u32)mElementArray + (inElemIndex * inElemSize));
			return elem;
		}
			
	public:
		x_fst_elem*			mElementArray;
	};

	void		x_fst_block::init(x_iallocator* allocator, u32 inElemSize, u32 inNumElements, s32 inAlignment)
	{
		ASSERT(inElemSize != 0);			// Check input parameters
		ASSERT(inNumElements > 0);
		
		void* p = allocator->allocate(inElemSize * inNumElements, inAlignment);
   		mElementArray = static_cast<x_fst_elem*>((void*)p);
		
		ASSERT(mElementArray != 0);
	}

	void x_fst_block::release(x_iallocator* allocator)
	{ 
   		allocator->deallocate(mElementArray);
		mElementArray = 0;
	}

	/**
	@brief	x_allocator_fst is a fast allocator for objects of fixed size.

	@desc	It preallocates (from @allocator) @inInitialBlockCount blocks with @inBlockSize T elements.
			By calling Allocate an application can fetch one T object. By calling Deallocate
			an application returns one T object to pool.
			When all objects on the pool are used, pool can grow to accommodate new requests.
			@inGrowthCount specifies by how many blocks the pool will grow.
			Reset reclaims all objects and reinitializes the pool. The parameter RestoreToInitialSize
			can be used to resize the pool to initial size in case it grew.

	@note	This allocator does not guarantee that two objects allocated sequentially are sequential in memory.
	**/
	class x_allocator_fst : public x_iallocator
	{
	public:
								x_allocator_fst();

		// @inElemSize			This determines the size in bytes of an element
		// @inBlockElemCnt		This determines the number of elements that are part of a block
		// @inInitialBlockCount	Initial number of blocks in the memory pool
		// @inBlockGrowthCount	Number of blocks by which it will grow if all space is used
		// @inElemAlignment		Alignment of the start of each pool (can be 0, which creates fixed size memory pool)
								x_allocator_fst(x_iallocator* allocator, u32 inElemSize, u32 inBlockElemCnt, u32 inInitialBlockCount, u32 inBlockGrowthCount, u32 inElemAlignment = 0);
								~x_allocator_fst();

		virtual const char*		name() const
		{
			return "fixed size allocator";
		}

		///@name	Should be called when created with default constructor
		//			Parameters are the same as for constructor with parameters
		void					init(x_iallocator* allocator, u32 inElemSize, u32 inBlockElemCnt, u32 inInitialBlockCount, u32 inBlockGrowthCount, u32 inElemAlignment);

		///@name	Resets allocator
		void					reset(xbool inRestoreToInitialSize = xFALSE);

		void*					allocate(u32 size, u32 alignment);
		void*					reallocate(void* p, u32 size, u32 alignment);
		void					deallocate(void* p);

		///@name	Statistics
		s32						getUsedItemCount() const;
		u32						getCurrentBlockCount() const;
		u32						getTotalCapacity() const;

		///@name	Placement new/delete
		void*					operator new(xsize_t num_bytes)					{ return NULL; }
		void*					operator new(xsize_t num_bytes, void* mem)		{ return mem; }
		void					operator delete(void* pMem)						{ }
		void					operator delete(void* pMem, void* )				{ }

	protected:
		///@name	Grows memory pool by blockSize * blockCount
		void					extend (u32 inBlockCount);
		void					release ();

	protected:
		x_iallocator*			mAllocator;
		x_fst_block*			mBlockArray;
		u32						mBlockArraySize;
		u32						mBlockArrayCapacity;		// Real size of the block array (to avoid excessive reallocation)
		x_fst_elem*				mFirstFreeElement;			// First free element in the free list

		// Save initial parameters
		u32						mElemSize;
		u32						mElemAlignment;
		u32 					mBlockElemCount;
		u32 					mBlockInitialCount;
		u32 					mBlockGrowthCount;

		// Helper members
		s32						mUsedItems;

	private:
		// Copy construction and assignment are forbidden
								x_allocator_fst(const x_allocator_fst&);
		x_allocator_fst&		operator= (const x_allocator_fst&);
	};

	x_allocator_fst::x_allocator_fst()
		: mAllocator(NULL)
		, mBlockArray(0)
		, mBlockArraySize(0)
		, mBlockArrayCapacity(0)
		, mElemSize(4)
		, mElemAlignment(X_ALIGNMENT_DEFAULT)
		, mFirstFreeElement(0)
		, mBlockElemCount(0)
		, mBlockInitialCount(0)
		, mBlockGrowthCount(0)
		, mUsedItems(0)
	{
	}

	x_allocator_fst::x_allocator_fst(x_iallocator* allocator, u32 inElemSize, u32 inBlockElemCnt, u32 inBlockInitialCount, u32 inBlockGrowthCount, u32 inElemAlignment)
		: mAllocator(allocator)
		, mBlockArray(0)
		, mBlockArraySize(0)
		, mBlockArrayCapacity(0)
		, mElemSize(inElemSize)
		, mElemAlignment(inElemAlignment)
		, mFirstFreeElement(0)
		, mBlockElemCount(inBlockElemCnt)
		, mBlockInitialCount(inBlockInitialCount)
		, mBlockGrowthCount(inBlockGrowthCount)
		, mUsedItems(0)
	{
		init(allocator, inElemSize, inBlockElemCnt, inBlockInitialCount, inBlockGrowthCount, inElemAlignment);
	}

	x_allocator_fst::~x_allocator_fst ()
	{
	}

	void x_allocator_fst::init(x_iallocator* allocator, u32 inElemSize, u32 inBlockElemCnt, u32 inBlockInitialCount, u32 inBlockGrowthCount, u32 inElemAlignment)
	{
		mAllocator = allocator;

		// Check input parameters	
		ASSERT(inElemSize >= 4);
		ASSERT(inBlockElemCnt > 0);
		ASSERT(inBlockInitialCount > 0);

		// Check if memory pool is not already initialized
		ASSERT(mBlockArray == NULL && mBlockArraySize == 0);

		// Save initial parameters
		mElemSize          = inElemSize;
		mElemAlignment     = inElemAlignment==0 ? X_ALIGNMENT_DEFAULT : inElemAlignment;
		mBlockElemCount    = inBlockElemCnt;
		mBlockInitialCount = inBlockInitialCount;
		mBlockGrowthCount  = inBlockGrowthCount;
		
		// Align element size to a multiple of element alignment
		mElemSize = x_intu::alignUp(mElemSize, mElemAlignment);

		extend(mBlockInitialCount);
	}

	void		x_allocator_fst::reset(xbool inRestoreToInitialSize)
	{
		// Check if pool is empty
		ASSERT(mUsedItems == 0);

		if (inRestoreToInitialSize)
		{
			// Release all memory blocks beyond initialCount
			for (u32 i=mBlockInitialCount; i<mBlockArraySize; ++i)
				mBlockArray[i].release(mAllocator);

			// Do not reallocate memory for mBlockArray (it should never take much)
			mBlockArraySize = mBlockInitialCount;
		}

		// Initialize linked list of free elements
		mFirstFreeElement = 0;
		for (u32 i=0; i<mBlockArraySize; ++i)
		{
			for (u32 j=0; j<mBlockElemCount; ++j)
			{
				x_fst_elem* elem = mBlockArray[i].getAt(j, mElemSize);
				elem->setNext(mFirstFreeElement);
				mFirstFreeElement = elem;
			}
		}
		mUsedItems = 0;
	}

	void*		x_allocator_fst::allocate(u32 size, u32 alignment)
	{
		ASSERT((u32)size <= mElemSize);

		// If memory pool is exhausted, extend it	
		if (mFirstFreeElement == NULL)
			extend(mBlockGrowthCount);

		// Check if extend has succeeded
		if (mFirstFreeElement == NULL)
			return NULL;

		x_fst_elem* element = mFirstFreeElement;
		mFirstFreeElement = element->getNext();
		++mUsedItems;
		return element->getObject();
	}

	void*		x_allocator_fst::reallocate(void* inObject, u32 size, u32 alignment)
	{
		ASSERT((u32)size <= mElemSize);
		ASSERT(alignment <= mElemAlignment);
		return inObject;
	}

	void		x_allocator_fst::deallocate(void* inObject)
	{
		// Check input parameters
		ASSERT(inObject != NULL);

		// Verify if object is from this pool
#ifdef X_DEBUG
		xbool addressOk = xFALSE;
		for (u32 i=0; i<mBlockArraySize; ++i)
		{
			u32 uiLowerLimit = reinterpret_cast<u32>(mBlockArray[i].mElementArray);
			u32 uiUpperLimit = uiLowerLimit + mBlockElemCount * mElemSize;
			
			if (reinterpret_cast<u32>(inObject)>=uiLowerLimit && reinterpret_cast<u32>(inObject)<=uiUpperLimit)
			{
				addressOk = true;
				break;
			}
		}
		ASSERT(addressOk == xTRUE);
#endif

		x_fst_elem*	element = reinterpret_cast<x_fst_elem*>(inObject);
		element->setNext(mFirstFreeElement);
		mFirstFreeElement = element;
		--mUsedItems;
	}

	void		x_allocator_fst::extend(u32 inBlockCount)
	{
		// In case of fixed pool it is legal to call extend with blockCount = 0
		if (inBlockCount == 0)
			return;

		// Check if after extend the block array will be too small
		if (mBlockArraySize + inBlockCount > mBlockArrayCapacity)
		{
			// Calculate new size of the blockArrayCapacity
			u32 newSize = mBlockArraySize + inBlockCount;
			if (newSize < 2 * mBlockArrayCapacity)
				newSize = 2 * mBlockArrayCapacity;

			// Allocate new array
			x_fst_block* oldArray = mBlockArray;
			mBlockArray = static_cast<x_fst_block*>(mAllocator->allocate(newSize * sizeof(x_fst_block), X_ALIGNMENT_DEFAULT));
			ASSERT(mBlockArray != NULL);
			if (oldArray != NULL)		// at first extend (initialization) this is maybe not true
			{ 
				// Copy old array to new array
				x_memcpy(mBlockArray, oldArray, mBlockArraySize * sizeof(x_fst_block));
				mAllocator->deallocate(oldArray);
			}		
			mBlockArrayCapacity = newSize;
		}

		// Extend memory pool by allocating new blocks
		for (u32 i=mBlockArraySize; i<mBlockArraySize+inBlockCount; ++i)
		{
			x_fst_block* block = &mBlockArray[i];
			block->init(mAllocator, mElemSize, mBlockElemCount, mElemAlignment);

			// Initialize free list
			for (u32 j=0; j<mBlockElemCount; ++j)
			{
				x_fst_elem* elem = block->getAt(j, mElemSize);
				elem->setNext(mFirstFreeElement);
				mFirstFreeElement = elem;
			}
		}
		mBlockArraySize += inBlockCount;
	}

	s32		x_allocator_fst::getUsedItemCount() const
	{
		return mUsedItems;
	}

	u32		x_allocator_fst::getCurrentBlockCount() const
	{
		return mBlockArraySize;
	}

	u32		x_allocator_fst::getTotalCapacity() const
	{
		return mBlockArraySize * mBlockElemCount;
	}

	void	x_allocator_fst::release()
	{
		ASSERT(mUsedItems == 0);
		for (u32 i=0; i<mBlockArraySize; ++i)
			mBlockArray[i].release(mAllocator);

		mAllocator->deallocate(mBlockArray);
		mBlockArraySize     = 0;
		mBlockArrayCapacity = 0;

		mAllocator->deallocate(this);
	}

	x_iallocator*		gCreateFstAllocator(x_iallocator* allocator, s32 elem_size, s32 elem_alignment, s32 block_elem_count, s32 block_initial_count, s32 block_growth_count, s32 block_max_count)
	{
		void* mem = allocator->allocate(sizeof(x_allocator_fst), X_ALIGNMENT_DEFAULT);
		x_allocator_fst* fst_allocator = new (mem) x_allocator_fst();
		fst_allocator->init(allocator, elem_size, block_elem_count, block_initial_count, block_growth_count, elem_alignment);

		return fst_allocator;
	}

};
