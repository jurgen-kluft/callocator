//==============================================================================
//  x_forwardbin.h
//==============================================================================
#ifndef __X_ALLOCATOR_FORWARD_BIN_H__
#define __X_ALLOCATOR_FORWARD_BIN_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	namespace xforwardbin
	{
		///< Behavior:
		///< This allocator only allocates memory from what is in front of head, when it is
		///< detected that there is not enough free space between head and end it finds out
		///< if wrapping around (head=begin) and then verifying if we have enough free space
		///< (tail - head)

		struct chunk;

		struct xallocator
		{
								xallocator();

			void				init(xbyte* mem_begin, xbyte* mem_end);
			void				reset();

			xbyte*				allocate(u32 size, u32 alignment);
			u32					get_size(void* p) const;
			void				deallocate(void* p);

		private:
			xbyte*				mMemBegin;
			xbyte*				mMemEnd;
			chunk*				mBegin;
			chunk*				mEnd;
			chunk*				mHead;
			u32					mNumAllocations;
		};
	}
};


#endif	/// __X_ALLOCATOR_FORWARD_BIN_H__

