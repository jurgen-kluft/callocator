#ifndef __X_SORTED_RANGES_H__
#define __X_SORTED_RANGES_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace xcore
{
	class xranges
	{
	public:
		class comparer
		{
		public:
			virtual s32	compare(u32 lhs, u32 rhs, u32 tag) const = 0;
		};

		void		init(u32 tag, xrange_compare* cmp, xalloc* allocator);

		s32			size() const;

		void		add(void* ptr, u32 handle);
		void		rem(void* ptr);
		bool		min(void*& ptr, u32& handle);
		bool		pop(void*& ptr, u32& handle);

		bool		find(void* ptr, u32& handle);

		comparer*	m_cmp;
		xalloc* 	m_alloc;
		node32*		m_root;
		u32			m_tag;
		s32			m_size;
	};

	class xhbitmap
	{
	public:
		// this hierarchical bitmap has 2 modes set by 'invert'
		// 1. false, searching quickly for '0' bits
		// 2. true, searching quickly for '1' bits
		void		init(xalloc* alloc, u32 maxbits, bool invert);

		void		reset();

		void		set(u32 bit);
		void		clr(u32 bit);

		u32			find();			// First 0 or 1

		// 32/1Kbit/32Kbit/1Mbit/32Mbit/1Gbit
		//  4/  128/   4KB/128KB/   4MB/128MB

		// Example:
		// When you initialize this xhbitmap with maxbits=512Kbit the
		// following will be allocated.
		// (512*1024 / 32) * 2 = 32768 * sizeof(u32) = 128 KB

		xalloc*		m_alloc;
		u32*		m_bitmap;
		u32			m_levels[6];	// Offsets into bitmap
		s32			m_invert;
		s32			m_numlevels;
	};
};

#endif	/// __X_SORTED_RANGES_H__