#ifndef __X_ALLOCATOR_BITLIST_H__
#define __X_ALLOCATOR_BITLIST_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	class xbitlist
	{
	public:
		static u32	size_in_dwords(u32 maxbits);

		// this hierarchical bitmap has 2 modes set by 'invert'
		// 1. false, searching quickly for '0' bits
		// 2. true, searching quickly for '1' bits
		void		init(u32* bits, u32 maxbits, u32 numdwords, bool invert);
		
		void		reset();

		void		set(u32 bit);
		void		clr(u32 bit);

		bool		find(u32& bit) const;			// First 0 or 1

		// 32/1Kbit/32Kbit/1Mbit/32Mbit/1Gbit
		//  4/  128/   4KB/128KB/   4MB/128MB

		// Example:
		// When you initialize this xhbitmap with maxbits=512Kbit the
		// following will be allocated.
		// (512*1024 / 32) * 2 = 32768 * sizeof(u32) = 128 KB
		enum { AllBitsSet = 0xffffffff }

		u32*		m_level0;
		u32*		m_levelT;
		u32			m_maxbits;
		s32			m_invert;
	};
};

#endif	/// __X_ALLOCATOR_BITLIST_H__