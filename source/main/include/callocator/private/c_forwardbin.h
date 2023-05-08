//==============================================================================
//  x_forwardbin.h
//==============================================================================
#ifndef __C_ALLOCATOR_FORWARD_BIN_H__
#define __C_ALLOCATOR_FORWARD_BIN_H__
#include "ccore/c_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

//==============================================================================
// ccore namespace
//==============================================================================
namespace ncore
{
	namespace forwardbin
	{
		///< Behavior:
		///< This allocator only allocates memory from what is in front of head, when it is
		///< detected that there is not enough free space between head and end it finds out
		///< if wrapping around (head=begin) and then verifying if we have enough free space
		///< (tail - head)

		struct chunk;

		struct allocator
		{
								allocator();

			void				init(u8* mem_begin, u8* mem_end);
			void				reset();

			u8*				allocate(u32 size, u32 alignment);
			u32					get_size(void* p) const;
			u32  				deallocate(void* p);

		private:
			u8*				mMemBegin;
			u8*				mMemEnd;
			chunk*				mBegin;
			chunk*				mEnd;
			chunk*				mHead;
			u32					mNumAllocations;
		};
	}
};


#endif	/// __C_ALLOCATOR_FORWARD_BIN_H__

