#ifndef __C_ALLOCATOR_RANGES_H__
#define __C_ALLOCATOR_RANGES_H__
#include "cbase/c_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

namespace ncore
{
	class alloc_t;

	class ranges
	{
	public:
		class comparer
		{
		public:
			virtual s32	compare(u32 lhs, u32 rhs, u32 tag) const = 0;
		};

		void		init(u32 tag, comparer* cmp, alloc_t* allocator);

		s32			size() const;

		void		add(void* ptr, u32 handle);
		void		rem(void* ptr);
		bool		min(void*& ptr, u32& handle);
		bool		pop(void*& ptr, u32& handle);

		bool		find(void* ptr, u32& handle);

		comparer*	m_cmp;
		alloc_t* 	m_alloc;
		node32*		m_root;
		u32			m_tag;
		s32			m_size;
	};
};

#endif	/// __C_SORTED_RANGES_H__