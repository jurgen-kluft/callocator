//==============================================================================
//  x_freelist.h
//==============================================================================
#ifndef __X_FREELIST_H__
#define __X_FREELIST_H__
#include "xbase\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	namespace xfreelist
	{
		class xelement;

		struct xdata
		{
								xdata();

			void				init(u32 elem_size, u32 elem_alignment, u32 elem_max_count);

			void				alloc_array(x_iallocator* allocator);
			void				dealloc_array(x_iallocator* allocator);

			void				set_array(xbyte* elem_array);

			inline bool			valid() const							{ return (mElementArray!=0); }

			inline u32			getElemSize() const						{ return mElemSize; }
			inline u32			getElemAlignment() const				{ return mElemAlignment; }
			inline u32			getElemMaxCount() const					{ return mElemMaxCount; }

			inline s32			iof(xelement const* element) const		{ return ((s32)((xbyte const*)element - (xbyte const*)mElementArray)) / (s32)mElemSize; }
			inline xelement*	pat(u32 index) const					{ return (xelement*)(mElementArray + (index * mElemSize)); }

		private:

			u32					mElemSize;
			u32					mElemAlignment;
			u32 				mElemMaxCount;
			xbyte*				mElementArray;
		};

		class xlist
		{
		public:
			inline				xlist() 
									: mFreeList (0)
									, mInfo (0) { }

			void				init(xdata const* info);
			void				reset();

			inline bool			valid() const							{ return (mInfo->valid()); }
			inline bool			full() const							{ ASSERT(valid()); return mFreeList==0; }

			xelement*			allocate();
			void				deallocate(xelement* element);

			inline s32			iof(xelement const* element) const		{ return mInfo->iof(element); }
			inline xelement*	pat(u32 index) const					{ return mInfo->pat(index); }

		private:
			xelement*		mFreeList;
			xdata const*	mInfo;
		};
	}
};


#endif	/// __X_FREELIST_H__

