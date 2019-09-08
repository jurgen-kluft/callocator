#ifndef __X_ALLOCATOR_BITLIST_H__
#define __X_ALLOCATOR_BITLIST_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase/x_allocator.h"

namespace xcore
{
	// this hierarchical bitlist has 2 modes set by 'invert'
	// 1. false, searching quickly for '0' bits
	// 2. true, searching quickly for '1' bits

	// Number of bits and how much memory they consume
	// 32/1Kbit/32Kbit/1Mbit/32Mbit/1Gbit
	//  4/  128/   4KB/128KB/   4MB/128MB ( * ~1.5)

	// Example:
	// When you initialize this bitlist with maxbits = 512Kbit the
	// size_in_dwords() function will return 16921, or 67684 bytes.
	// level 0, bits= 524288, dwords= 16384, bytes= 65536 + 8
	// level 1, bits= 16384, dwords= 512, bytes= 2048 + 8
	// level 2, bits= 512, dwords= 16, bytes= 64 + 8
	// level 3, bits= 16, dwords= 1, bytes= 4 + 8
	// total: 65536 + 2048 + 64 + 4 = 67652 + 32 = 67684
	// note: The + 8 bytes is the extra information per level

	class xhibitset
	{
	public:
    	inline		xhibitset() : m_level0(nullptr), m_levelT(nullptr), m_numbits(0), m_invert(0) {}

		void		init(u32* bits, u32 maxbits, bool setall, bool invert);
		void		init(xalloc* alloc, u32 maxbits, bool setall, bool invert);

		void		release(xalloc* alloc);
		
		void		reset(bool setall);

		void		set(u32 bit);
		void		clr(u32 bit);

		bool		is_set(u32 bit) const;
		bool		is_full() const;
		bool		find(u32& bit) const;			// First 0 or 1

		static u32	size_in_dwords(u32 maxbits);

		enum { AllBitsSet = 0xffffffff };

		// 7 levels maximum, this means a maximum of 7 * 5 = 2^35 = 34.359.738.368
		u32*		m_levels[7];
		u32			m_maxlevel;
		u32			m_numbits;
		s32			m_invert;
	};
};

#endif	/// __X_ALLOCATOR_BITLIST_H__