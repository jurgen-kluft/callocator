#include "xbase\x_target.h"
#include "xbase\x_types.h"
#include "xbase\x_debug.h"
#include "xbase\x_memory_std.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xbase\private\x_rbtree.h"

#include "xallocator\x_allocator_pool.h"

namespace xcore
{
	namespace xfreelist_allocator
	{
		/**
		@brief		Fixed size type, element
		@desc		It implements linked list behavior for free elements in the block.
		**/
		class xelement	
		{
		public:
			// This part is a little bit dirty...
			xelement*			getNext()							{ return *reinterpret_cast<xelement**>(&mData); }
			void				setNext(xelement* next)				{ xelement** temp = reinterpret_cast<xelement**>(&mData); *temp = next; }
			void*				getObject()							{ return (void*)&mData; }
		private:
			u32					mData;	
		};

		/**
		@brief	xblock contains an array of xelement objects.
		**/
		class xblock
		{
		public:
								xblock() 
									: mElementArray(NULL)
									, mFreeList (0)
									, mAllocCount (0) { }

			void				init(xbyte* elementArray, u32 inElemSize, u32 inNumElements);
			void				reset(u32 inSizeOfElement, u32 inElementsPerBlock);
			
			inline bool			full() const						{ return mFreeList == NULL; }
			inline bool			empty() const						{ return mAllocCount == 0; }

			inline xelement*	allocate()
			{
				xelement* e = mFreeList;
				mFreeList = e->getNext();
				++mAllocCount;
				return e;
			}
			
			inline void			deallocate(xelement* element, u32 inElementsPerBlock, u32 inSizeOfElement)
			{
				u32 idx = index_of(element, inSizeOfElement);
				ASSERT(idx>=0 && idx<inElementsPerBlock);
				element->setNext(mFreeList);
				mFreeList = element;
				--mAllocCount;
			}

		private:
			inline s32			index_of(xelement* element, u32 inSizeOfElement) const	{ return (s32)((u32)element - (u32)mElementArray) / (s32)inSizeOfElement; }
			inline xelement*	at(u32 index, u32 inSizeOfElement)						{ return (xelement*)((xbyte*)mElementArray + (index * inSizeOfElement)); }

			xelement*			mElementArray;
			xelement*			mFreeList;
			u32					mAllocCount;
		};

		void		xblock::init(xbyte* elementArray, u32 inElemSize, u32 inNumElements)
		{
			mElementArray = static_cast<xelement*>((void*)elementArray);
			ASSERT(mElementArray != 0);

			mFreeList   = NULL;
			mAllocCount = 0;

			reset(inElemSize, inNumElements);
		}

		void		xblock::reset(u32 inSizeOfElement, u32 inElementsPerBlock)
		{
			mFreeList = NULL;
			mAllocCount = 0;
			for (s32 i=inElementsPerBlock-1; i>=0; --i)
			{
				xelement* e = at(i, inSizeOfElement);
				e->setNext(mFreeList);
				mFreeList = e;
			}
		}

		/**
		@brief	xallocator_imp is a fast allocator for objects of fixed size.

		@desc	It preallocates (from @allocator) @inInitialBlockCount blocks with @inBlockSize T elements.
				By calling allocate() an application can fetch one T object. By calling deallocate()
				an application returns one T object to pool.

		@note	This allocator does not guarantee that two objects allocated sequentially are sequential in memory.
		**/
		class xallocator_imp : public x_iallocator
		{
		public:
									xallocator_imp();

			// @inElemSize			This determines the size in bytes of an element
			// @inElemAlignment		Alignment of the start of each pool (can be 0, which creates fixed size memory pool)
			// @inBlockElemCnt		This determines the number of elements that are part of a block
									xallocator_imp(x_iallocator* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
									xallocator_imp(x_iallocator* allocator, void* mem_block, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
			virtual					~xallocator_imp();

			virtual const char*		name() const									{ return "Fixed size free list allocator"; }

			///@name	Should be called when created with default constructor
			//			Parameters are the same as for constructor with parameters
			void					init();

			virtual void*			allocate(u32 size, u32 alignment);
			virtual void*			reallocate(void* p, u32 size, u32 alignment);
			virtual void			deallocate(void* p);

			///@name	Placement new/delete
			XCORE_CLASS_PLACEMENT_NEW_DELETE

		protected:
			virtual void			release ();

		protected:
			x_iallocator*			mAllocator;

			u32						mElementArrayOwned;
			xbyte*					mElementArray;
			xblock					mBlock;

			// Save initial parameters
			u32						mElemSize;
			u32						mElemAlignment;
			u32 					mBlockElemCount;

		private:
			// Copy construction and assignment are forbidden
									xallocator_imp(const xallocator_imp&);
			xallocator_imp&			operator= (const xallocator_imp&);
		};

		xallocator_imp::xallocator_imp()
			: mAllocator(NULL)
			, mElementArray(NULL)
			, mElemSize(4)
			, mElemAlignment(X_ALIGNMENT_DEFAULT)
			, mBlockElemCount(0)
		{
		}

		xallocator_imp::xallocator_imp(x_iallocator* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt)
			: mAllocator(allocator)
			, mElementArrayOwned(true)
			, mElementArray(NULL)
			, mElemSize(inElemSize)
			, mElemAlignment(inElemAlignment)
			, mBlockElemCount(inBlockElemCnt)
		{
			init();
		}

		xallocator_imp::xallocator_imp(x_iallocator* allocator, void* inElementArray, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt)
			: mAllocator(allocator)
			, mElementArrayOwned(false)
			, mElementArray((xbyte*)inElementArray)
			, mElemSize(inElemSize)
			, mElemAlignment(inElemAlignment)
			, mBlockElemCount(inBlockElemCnt)
		{
			init();
		}

		xallocator_imp::~xallocator_imp ()
		{
			ASSERT(mBlock.empty());
		}

		void xallocator_imp::init()
		{
			// Check input parameters	
			ASSERT(mElemSize >= 4);
			ASSERT(mBlockElemCount > 0);

			// Save initial parameters
			mElemAlignment     = mElemAlignment==0 ? X_ALIGNMENT_DEFAULT : mElemAlignment;
			mElemSize          = x_intu::alignUp(mElemSize, mElemAlignment);			// Align element size to a multiple of element alignment

			// Allocate element array
			if (mElementArray == NULL)
			{
				mElementArray = (xbyte*)mAllocator->allocate(mElemSize * mBlockElemCount, mElemAlignment);
				mElementArrayOwned = true;
			}

			mBlock.init(mElementArray, mElemSize, mBlockElemCount);
		}

		void*		xallocator_imp::allocate(u32 size, u32 alignment)
		{
			ASSERT((u32)size <= mElemSize);
			ASSERT((u32)alignment <= mElemAlignment);
			ASSERT(mBlock.empty()==false);
			return mBlock.allocate();
		}

		void*		xallocator_imp::reallocate(void* inObject, u32 size, u32 alignment)
		{
			if (inObject == NULL)
			{
				return allocate(size, alignment);
			}
			else if (inObject != NULL && size == 0)
			{
				deallocate(inObject);
				return NULL;
			}
			else
			{
				ASSERT((u32)size <= mElemSize);
				ASSERT(alignment <= mElemAlignment);
				return inObject;
			}
		}

		void		xallocator_imp::deallocate(void* inObject)
		{
			// Check input parameters
			if (inObject == NULL)
				return;
			mBlock.deallocate((xelement*)inObject, mBlockElemCount, mElemSize);
		}

		void	xallocator_imp::release()
		{
			ASSERT(mBlock.empty());
			if (mElementArrayOwned!=0)
			{
				mAllocator->deallocate(mElementArray);
				mElementArray = NULL;
			}
			mAllocator->deallocate(this);
		}

	}	// End of xfreelist_allocator namespace

	x_iallocator*		gCreateFreeListAllocator(x_iallocator* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
	{
		void* mem = allocator->allocate(sizeof(xfreelist_allocator::xallocator_imp), X_ALIGNMENT_DEFAULT);
		xfreelist_allocator::xallocator_imp* _allocator = new (mem) xfreelist_allocator::xallocator_imp(allocator, inSizeOfElement, inElementAlignment, inNumElements);
		_allocator->init();
		return _allocator;
	}

	x_iallocator*		gCreateFreeListAllocator(x_iallocator* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
	{
		void* mem = allocator->allocate(sizeof(xfreelist_allocator::xallocator_imp), X_ALIGNMENT_DEFAULT);
		xfreelist_allocator::xallocator_imp* _allocator = new (mem) xfreelist_allocator::xallocator_imp(allocator, inElementArray, inSizeOfElement, inElementAlignment, inNumElements);
		_allocator->init();
		return _allocator;
	}
};
