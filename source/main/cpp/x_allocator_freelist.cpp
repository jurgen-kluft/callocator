#include "xbase\x_target.h"
#include "xbase\x_types.h"
#include "xbase\x_debug.h"
#include "xbase\x_memory_std.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xbase\private\x_rbtree.h"

#include "xbase\x_idx_allocator.h"
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

		struct xblock_info
		{
								xblock_info() : mElemSize(0), mElemAlignment(0), mElemMaxCount(0) {}
								xblock_info(u32 elem_size, u32 elem_alignment, u32 elem_max_count) : mElemSize(elem_size), mElemAlignment(elem_alignment), mElemMaxCount(elem_max_count) {}

			u32					mElemSize;
			u32					mElemAlignment;
			u32 				mElemMaxCount;
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
									, mInfo (NULL) { }

			void				init(xbyte* elementArray, xblock_info const* info);
			void				reset();
			
			inline bool			full() const						{ return mFreeList == NULL; }

			inline xelement*	allocate()
			{
				if (mFreeList == NULL)
					return NULL;
				xelement* e = mFreeList;
				mFreeList = e->getNext();
				return e;
			}
			
			inline void			deallocate(xelement* element)
			{
				if (mFreeList == NULL)
					return;

				u32 idx = index_of(element);
				ASSERT(idx>=0 && idx<mInfo->mElemMaxCount);
				element->setNext(mFreeList);
				mFreeList = element;
			}

			inline s32			index_of(xelement const* element) const	{ return ((s32)((xbyte const*)element - (xbyte const*)mElementArray)) / (s32)mInfo->mElemSize; }
			inline xelement*	ptr_at(u32 index) const					{ return (xelement*)((xbyte*)mElementArray + (index * mInfo->mElemSize)); }
		
		private:
			xelement*			mElementArray;
			xelement*			mFreeList;
			xblock_info const*	mInfo;
		};

		void		xblock::init(xbyte* elementArray, xblock_info const* info)
		{
			mElementArray = static_cast<xelement*>((void*)elementArray);
			ASSERT(mElementArray != 0);

			mFreeList = NULL;
			mInfo     = info;
			reset();
		}

		void		xblock::reset()
		{
			mFreeList = NULL;
			for (s32 i=mInfo->mElemMaxCount-1; i>=0; --i)
			{
				xelement* e = ptr_at(i);
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
			void					clear();
			void					exit();

			u32						size() const										{ return mAllocCount; }
			u32						max_size() const									{ return mBlockInfo.mElemMaxCount; }

			virtual void*			allocate(u32 size, u32 alignment);
			virtual void*			reallocate(void* p, u32 size, u32 alignment);
			virtual void			deallocate(void* p);

			u32						to_idx(void const* p) const;
			void*					from_idx(u32 idx) const;

			x_iallocator*			allocator() const									{ return (x_iallocator*)mAllocator; }

			virtual void			release ();

			///@name	Placement new/delete
			XCORE_CLASS_PLACEMENT_NEW_DELETE

		protected:
			x_iallocator*			mAllocator;

			xbyte*					mElementArray;
			u32						mElementArrayOwned;

			xblock					mBlock;
			xblock_info				mBlockInfo;
			u32						mAllocCount;

		private:
			// Copy construction and assignment are forbidden
									xallocator_imp(const xallocator_imp&);
			xallocator_imp&			operator= (const xallocator_imp&);
		};

		xallocator_imp::xallocator_imp()
			: mAllocator(NULL)
			, mElementArray(NULL)
			, mBlockInfo(4, X_ALIGNMENT_DEFAULT, 0)
			, mAllocCount(0)
		{
		}

		xallocator_imp::xallocator_imp(x_iallocator* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt)
			: mAllocator(allocator)
			, mElementArrayOwned(true)
			, mElementArray(NULL)
			, mBlockInfo(inElemSize, inElemAlignment, inBlockElemCnt)
			, mAllocCount(0)
		{
		}

		xallocator_imp::xallocator_imp(x_iallocator* allocator, void* inElementArray, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt)
			: mAllocator(allocator)
			, mElementArrayOwned(false)
			, mElementArray((xbyte*)inElementArray)
			, mBlockInfo(inElemSize, inElemAlignment, inBlockElemCnt)
			, mAllocCount(0)
		{
		}

		xallocator_imp::~xallocator_imp ()
		{
			ASSERT(mAllocCount == 0);
		}

		void xallocator_imp::init()
		{
			// Check input parameters	
			ASSERT(mBlockInfo.mElemSize >= 4);
			ASSERT(mBlockInfo.mElemMaxCount > 0);

			// Save initial parameters
			mBlockInfo.mElemAlignment     = mBlockInfo.mElemAlignment==0 ? X_ALIGNMENT_DEFAULT : mBlockInfo.mElemAlignment;
			mBlockInfo.mElemSize          = x_intu::alignUp(mBlockInfo.mElemSize, mBlockInfo.mElemAlignment);			// Align element size to a multiple of element alignment

			// Allocate element array
			if (mElementArray == NULL)
			{
				mElementArray = (xbyte*)mAllocator->allocate(mBlockInfo.mElemSize * mBlockInfo.mElemMaxCount, mBlockInfo.mElemAlignment);
				mElementArrayOwned = true;
			}

			mAllocCount = 0;
			mBlock.init(mElementArray, &mBlockInfo);
		}

		void		xallocator_imp::clear()
		{
			mAllocCount = 0;
			mBlock.reset();
		}

		void		xallocator_imp::exit()
		{
			ASSERT(mAllocCount == 0);
			if (mElementArrayOwned!=0)
			{
				mAllocator->deallocate(mElementArray);
				mElementArray = NULL;
			}
		}

		void*		xallocator_imp::allocate(u32 size, u32 alignment)
		{
			ASSERT((u32)size <= mBlockInfo.mElemSize);
			ASSERT((u32)alignment <= mBlockInfo.mElemAlignment);
			void* p = mBlock.allocate();	// Will return NULL if no more memory available
			if (p != NULL)
				++mAllocCount;
			return p;
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
				ASSERT((u32)size <= mBlockInfo.mElemSize);
				ASSERT(alignment <= mBlockInfo.mElemAlignment);
				return inObject;
			}
		}

		void		xallocator_imp::deallocate(void* inObject)
		{
			// Check input parameters
			if (inObject == NULL)
				return;
			mBlock.deallocate((xelement*)inObject);
			--mAllocCount;
		}

		u32		xallocator_imp::to_idx(void const* p) const
		{
			// Check input parameters
			ASSERT (p != NULL);
			return mBlock.index_of((xfreelist_allocator::xelement*)p);
		}

		void*	xallocator_imp::from_idx(u32 idx) const
		{
			return mBlock.ptr_at(idx);
		}

		void	xallocator_imp::release()
		{
			exit();
			mAllocator->deallocate(this);
		}

		class xiallocator_imp : public x_iidx_allocator
		{
			x_iallocator*			mOurAllocator;
			xallocator_imp			mAllocator;

		public:
			xiallocator_imp();
			xiallocator_imp(x_iallocator* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
			xiallocator_imp(x_iallocator* allocator, void* mem_block, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);

			virtual const char*		name() const										{ return "freelist idx allocator"; }

			virtual void*			allocate(u32 size, u32 align)						{ return mAllocator.allocate(size, align); }
			virtual void*			reallocate(void* p, u32 size, u32 align)			{ return mAllocator.reallocate(p, size, align); }
			virtual void			deallocate(void* p)									{ return mAllocator.deallocate(p); }

			virtual void			release()											{ mAllocator.exit(); mOurAllocator->deallocate(this); }

			virtual void			init()												{ mAllocator.init(); }
			virtual void			clear()												{ mAllocator.clear(); }

			virtual u32				size() const										{ return mAllocator.size(); }
			virtual u32				max_size() const									{ return mAllocator.max_size(); }

			virtual u32				iallocate(void*& p)
			{
				p = allocate(1, 4);
				return mAllocator.to_idx(p);
			}

			virtual void			ideallocate(u32 idx)
			{
				void* p = mAllocator.from_idx(idx);
				mAllocator.deallocate(p);
			}

			virtual void*			to_ptr(u32 idx) const
			{
				void* p = mAllocator.from_idx(idx);
				return p;
			}

			virtual u32				to_idx(void const* p) const
			{
				u32 idx = mAllocator.to_idx(p);
				return idx;
			}

			///@name	Placement new/delete
			XCORE_CLASS_PLACEMENT_NEW_DELETE
		};

		xiallocator_imp::xiallocator_imp()
			: mOurAllocator(NULL)
		{
		}

		xiallocator_imp::xiallocator_imp(x_iallocator* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt)
			: mOurAllocator(allocator)
			, mAllocator(allocator, inElemSize, inElemAlignment, inBlockElemCnt)
		{
		}

		xiallocator_imp::xiallocator_imp(x_iallocator* allocator, void* inElementArray, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt)
			: mOurAllocator(allocator)
			, mAllocator(allocator, inElementArray, inElemSize, inElemAlignment, inBlockElemCnt)
		{
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

	x_iidx_allocator*	gCreateFreeListIdxAllocator(x_iallocator* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
	{
		void* mem = allocator->allocate(sizeof(xfreelist_allocator::xiallocator_imp), X_ALIGNMENT_DEFAULT);
		xfreelist_allocator::xiallocator_imp* _allocator = new (mem) xfreelist_allocator::xiallocator_imp(allocator, inSizeOfElement, inElementAlignment, inNumElements);
		_allocator->init();
		return _allocator;
	}

	x_iidx_allocator*	gCreateFreeListIdxAllocator(x_iallocator* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
	{
		void* mem = allocator->allocate(sizeof(xfreelist_allocator::xiallocator_imp), X_ALIGNMENT_DEFAULT);
		xfreelist_allocator::xiallocator_imp* _allocator = new (mem) xfreelist_allocator::xiallocator_imp(allocator, inElementArray, inSizeOfElement, inElementAlignment, inNumElements);
		_allocator->init();
		return _allocator;
	}
};
