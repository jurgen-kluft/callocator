#include "xbase\x_target.h"
#include "xbase\x_types.h"
#include "xbase\x_debug.h"
#include "xbase\x_memory_std.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"

#include "xallocator\private\x_freelist.h"

namespace xcore
{
	namespace xfreelist
	{
		/**
		@brief		The back-end data of the fixed size free list
		@desc		Contains the configuration and the raw element array of the list.
		**/
		xdata::xdata() 
			: mElemSize(0)
			, mElemAlignment(0)
			, mElemMaxCount(0)
			, mElementArray(0) {}


		void				xdata::init(u32 elem_size, u32 elem_alignment, u32 elem_max_count)
		{
			// Take over the parameters
			mElemSize = elem_size;
			mElemAlignment = elem_alignment;
			mElemMaxCount = elem_max_count;

			// Clamp/Guard parameters
			mElemAlignment     = mElemAlignment==0 ? X_ALIGNMENT_DEFAULT : mElemAlignment;
			mElemAlignment     = x_intu::alignUp(mElemAlignment, sizeof(void*));					// Align element alignment to the size of a pointer
			mElemSize          = x_intu::alignUp(mElemSize, sizeof(void*));							// Align element size to the size of a pointer
			mElemSize          = x_intu::alignUp(mElemSize, mElemAlignment);						// Align element size to a multiple of element alignment

			// Check parameters	
			ASSERT(mElemSize >= sizeof(void*));
			ASSERT(mElemMaxCount > 0);
		}

		void				xdata::alloc_array(x_iallocator* allocator)
		{
			// Initialize the element array
			mElementArray = (xbyte*)allocator->allocate(mElemSize * mElemMaxCount, mElemAlignment);
		}

		void				xdata::dealloc_array(x_iallocator* allocator)
		{
			allocator->deallocate(mElementArray);
			mElementArray = NULL;
		}


		void				xdata::set_array(xbyte* elem_array)
		{
			// Set/Reset the user provided element array
			mElementArray = elem_array;
		}


		/**
		@brief		Fixed size type, element
		@desc		It implements linked list behavior for free elements in the block.
					Works on 64-bit systems since we use indexing here instead of pointers.
		**/
		class xelement	
		{
		public:
			xelement*			getNext(xdata const* info)						{ return info->pat(mIndex); }
			void				setNext(xdata const* info, xelement* next)		{ mIndex = info->iof(next); }
			void*				getObject()										{ return (void*)&mIndex; }
		private:
			u32					mIndex;	
		};

		xelement*		xlist::allocate()
		{
			if (mFreeList == NULL)
				return NULL;
			xelement* e = mFreeList;
			mFreeList = e->getNext(mInfo);
			return e;
		}
			
		void			xlist::deallocate(xelement* element)
		{
			if (mFreeList == NULL)
				return;

			u32 idx = mInfo->iof(element);
			ASSERT(idx>=0 && idx<mInfo->getElemMaxCount());
			element->setNext(mInfo, mFreeList);
			mFreeList = element;
		}


		void			xlist::init(xdata const* info)
		{
			mInfo = info;
			ASSERT(mInfo->valid());
			mFreeList = NULL;
			reset();
		}

		void			xlist::reset()
		{
			mFreeList = NULL;
			for (s32 i=mInfo->getElemMaxCount()-1; i>=0; --i)
			{
				xelement* e = mInfo->pat(i);
				e->setNext(mInfo, mFreeList);
				mFreeList = e;
			}
		}
	}
};
