#ifndef __X_FREELIST_H__
#define __X_FREELIST_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace ncore
{
	struct xfreelist_t
	{
		xfreelist_t();

		const s32			NULL_INDEX = -1;

		void				init_with_array(u8* array, u32 array_size, u32 elem_size, u32 elem_alignment);
		void				init_with_alloc(alloc_t* allocator, u32 elem_size, u32 elem_alignment, s32 size);
		void				init_list();
		void				release();

		inline bool			valid() const							{ return (mElementArray!=nullptr); }
		inline s32			size() const							{ return mSize; }
		inline s32			used() const							{ return mUsed; }

		inline u32			getElemSize() const						{ return mElemSize; }
		inline u32			getElemAlignment() const				{ return mElemAlignment; }

		struct xitem_t;

		inline xitem_t*		ptr_of(s32 index) const
		{
			if (index == NULL_INDEX)
				return nullptr;
			return (xitem_t*)(mElementArray + (index * mElemSize));
		}
		inline s32			idx_of(xitem_t const* element) const
		{
			if (element == nullptr)
				return NULL_INDEX;
			s32 idx = ((s32)((u8 const*)element - (u8 const*)mElementArray)) / (s32)mElemSize;
			if (idx >= 0 && idx < mSize)
				return idx;
			return NULL_INDEX;
		}

		xitem_t*			alloc();
		void				free(xitem_t*);

	private:

		alloc_t *			mAllocator;
		u32					mElemSize;
		u32					mElemAlignment;
		u32 				mUsed;
		u32 				mSize;
		u8*				mElementArray;
		xitem_t*			mFreeList;
	};

};


#endif	/// __X_FREELIST_H__

