#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_memory_std.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"

#include "xbase/x_idx_allocator.h"
#include "xallocator/x_allocator_pool.h"
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
		class xallocator_imp : public xalloc
		{
		public:
									xallocator_imp();

			// @inElemSize			This determines the size in bytes of an element
			// @inElemAlignment		Alignment of the start of each pool (can be 0, which creates fixed size memory pool)
			// @inBlockElemCnt		This determines the number of elements that are part of a block
									xallocator_imp(xalloc* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
									xallocator_imp(xalloc* allocator, void* mem_block, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
			virtual					~xallocator_imp();

			virtual const char*		name() const									{ return TARGET_FULL_DESCR_STR " [Allocator, Type=freelist]"; }

			///@name	Should be called when created with default constructor
			//			Parameters are the same as for constructor with parameters
			void					init();
			void					clear();
			void					exit();

			u32						size() const										{ return mAllocCount; }
			u32						max_size() const									{ return mFreeList.size(); }

			virtual void*			allocate(xsize_t size, u32 alignment);
			virtual void*			reallocate(void* p, xsize_t size, u32 alignment);
			virtual void			deallocate(void* p);

			u32						to_idx(void const* p) const;
			void*					from_idx(u32 idx) const;

			xalloc*					allocator() const									{ return (xalloc*)mAllocator; }

			virtual void			release ();

			///@name	Placement new/delete
			XCORE_CLASS_PLACEMENT_NEW_DELETE

		protected:
			xalloc*					mAllocator;
			void*					mElementArray;
			xfreelist_t				mFreeList;
			u32						mAllocCount;

		private:
			// Copy construction and assignment are forbidden
									xallocator_imp(const xallocator_imp&);
			xallocator_imp&			operator= (const xallocator_imp&);
		};

		xallocator_imp::xallocator_imp()
			: mAllocator(NULL)
			, mElementArray(NULL)
			, mFreeList()
			, mAllocCount(0)
		{
		}

		xallocator_imp::xallocator_imp(xalloc* allocator, u32 inElemSize, u32 inElemAlignment, u32 inMaxNumElements)
			: mAllocator(allocator)
			, mElementArray(NULL)
			, mFreeList()
			, mAllocCount(0)
		{
			mFreeList.init_with_alloc(allocator, inElemSize, inElemAlignment, inMaxNumElements);
		}

		xallocator_imp::xallocator_imp(xalloc* allocator, void* inElementArray, u32 inElemSize, u32 inElemAlignment, u32 inMaxNumElements)
			: mAllocator(allocator)
			, mElementArray(inElementArray)
			, mFreeList()
			, mAllocCount(0)
		{
			mFreeList.init_with_array((xcore::xbyte*)inElementArray, inMaxNumElements * inElemSize, inElemSize, inElemAlignment);
		}

		xallocator_imp::~xallocator_imp ()
		{
			ASSERT(mAllocCount == 0);
		}

		void xallocator_imp::init()
		{
			mAllocCount = 0;
			mFreeList.init_list();
		}

		void		xallocator_imp::clear()
		{
			mAllocCount = 0;
			mFreeList.init_list();
		}

		void		xallocator_imp::exit()
		{
			ASSERT(mAllocCount == 0);
			mFreeList.release();
		}

		void*		xallocator_imp::allocate(xsize_t size, u32 alignment)
		{
			ASSERT((u32)size <= mFreeList.getElemSize());
			ASSERT((u32)alignment <= mFreeList.getElemAlignment());
			void* p = mFreeList.alloc();	// Will return NULL if no more memory available
			if (p != NULL)
				++mAllocCount;
			return p;
		}

		void*		xallocator_imp::reallocate(void* inObject, xsize_t size, u32 alignment)
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
				ASSERT((u32)size <= mFreeList.getElemSize());
				ASSERT(alignment <= mFreeList.getElemAlignment());
				return inObject;
			}
		}

		void		xallocator_imp::deallocate(void* inObject)
		{
			// Check input parameters
			if (inObject == NULL)
				return;
			mFreeList.free((xfreelist_t::xitem_t*)inObject);
			--mAllocCount;
		}

		u32		xallocator_imp::to_idx(void const* p) const
		{
			// Check input parameters
			ASSERT (p != NULL);
			return mFreeList.idx_of((xfreelist_t::xitem_t*)p);
		}

		void*	xallocator_imp::from_idx(u32 idx) const
		{
			return (void*)mFreeList.ptr_of(idx);
		}

		void	xallocator_imp::release()
		{
			exit();
			mAllocator->deallocate(this);
		}

		class xiallocator_imp : public x_iidx_allocator
		{
			xalloc*					mOurAllocator;
			xallocator_imp			mAllocator;

		public:
			xiallocator_imp();
			xiallocator_imp(xalloc* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);
			xiallocator_imp(xalloc* allocator, void* mem_block, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt);

			virtual const char*		name() const										{ return TARGET_FULL_DESCR_STR " [Allocator, Type=freelist,indexed]"; }

			virtual void*			allocate(xsize_t size, u32 align)					{ return mAllocator.allocate(size, align); }
			virtual void*			reallocate(void* p, xsize_t size, u32 align)		{ return mAllocator.reallocate(p, size, align); }
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

		xiallocator_imp::xiallocator_imp(xalloc* allocator, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt)
			: mOurAllocator(allocator)
			, mAllocator(allocator, inElemSize, inElemAlignment, inBlockElemCnt)
		{
		}

		xiallocator_imp::xiallocator_imp(xalloc* allocator, void* inElementArray, u32 inElemSize, u32 inElemAlignment, u32 inBlockElemCnt)
			: mOurAllocator(allocator)
			, mAllocator(allocator, inElementArray, inElemSize, inElemAlignment, inBlockElemCnt)
		{
		}

	}	// End of xfreelist_allocator namespace

	xalloc*		gCreateFreeListAllocator(xalloc* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
	{
		void* mem = allocator->allocate(sizeof(xfreelist_allocator::xallocator_imp), X_ALIGNMENT_DEFAULT);
		xfreelist_allocator::xallocator_imp* _allocator = new (mem) xfreelist_allocator::xallocator_imp(allocator, inSizeOfElement, inElementAlignment, inNumElements);
		_allocator->init();
		return _allocator;
	}

	xalloc*		gCreateFreeListAllocator(xalloc* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
	{
		void* mem = allocator->allocate(sizeof(xfreelist_allocator::xallocator_imp), X_ALIGNMENT_DEFAULT);
		xfreelist_allocator::xallocator_imp* _allocator = new (mem) xfreelist_allocator::xallocator_imp(allocator, inElementArray, inSizeOfElement, inElementAlignment, inNumElements);
		_allocator->init();
		return _allocator;
	}

	x_iidx_allocator*	gCreateFreeListIdxAllocator(xalloc* allocator, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
	{
		void* mem = allocator->allocate(sizeof(xfreelist_allocator::xiallocator_imp), X_ALIGNMENT_DEFAULT);
		xfreelist_allocator::xiallocator_imp* _allocator = new (mem) xfreelist_allocator::xiallocator_imp(allocator, inSizeOfElement, inElementAlignment, inNumElements);
		_allocator->init();
		return _allocator;
	}

	x_iidx_allocator*	gCreateFreeListIdxAllocator(xalloc* allocator, void* inElementArray, u32 inSizeOfElement, u32 inElementAlignment, u32 inNumElements)
	{
		void* mem = allocator->allocate(sizeof(xfreelist_allocator::xiallocator_imp), X_ALIGNMENT_DEFAULT);
		xfreelist_allocator::xiallocator_imp* _allocator = new (mem) xfreelist_allocator::xiallocator_imp(allocator, inElementArray, inSizeOfElement, inElementAlignment, inNumElements);
		_allocator->init();
		return _allocator;
	}
};
