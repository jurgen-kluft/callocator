#ifndef __X_FREELIST_H__
#define __X_FREELIST_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	class xelement;

	struct xfreelist_t
	{
		xfreelist_t();

		void				init(xbyte* array, u32 array_size, u32 elem_size, u32 elem_alignment);
		void				alloc(x_iallocator* allocator, u32 elem_size, u32 elem_alignment, s32 size);
		void				release();

		inline bool			valid() const							{ return (mElementArray!=nullptr); }
		inline s32			size() const							{ return mSize; }

		inline u32			getElemSize() const						{ return mElemSize; }
		inline u32			getElemAlignment() const				{ return mElemAlignment; }

		inline s32			idx_of(xelement const* element) const	{ return ((s32)((xbyte const*)element - (xbyte const*)mElementArray)) / (s32)mElemSize; }
		inline xelement*	ptr_of(u32 index) const					{ return (xelement*)(mElementArray + (index * mElemSize)); }

	private:
		x_iallocator *		mAllocator;
		u32					mElemSize;
		u32					mElemAlignment;
		u32 				mSize;
		xbyte*				mElementArray;
	};

};


#endif	/// __X_FREELIST_H__

