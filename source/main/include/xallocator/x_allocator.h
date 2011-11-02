/**
* @file Head file x_allocator.h
*/

#ifndef __X_ALLOCATOR_H__
#define __X_ALLOCATOR_H__
#include "xbase\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase\x_types.h"

/**
 * xCore namespace
 */
namespace xcore
{
	/// Forward declares
	class x_iallocator;

	/// The custom allocators, Dlmalloc and Tlfs allocator
	extern x_iallocator*		gCreateDlAllocator(void* mem, s32 memsize);
	extern x_iallocator*		gCreateTlsfAllocator(void* mem, s32 memsize);

	/// External block allocator, manages external memory like Texture Memory, Sound Memory by putting bookkeeping data in separate memory
	class x_iextmem
	{
	public:
		virtual void			copy(void const* src, u32 src_size, void* dst, u32 dst_size) = 0;
	};
	extern x_iallocator*		gCreateEbAllocator(void* mem, s32 memsize, x_iallocator *allocator, x_iextmem *extmem_access);
	
	/// The fixed sized type allocator
	extern x_iallocator*		gCreateFstAllocator(x_iallocator* main_allocator, s32 elem_size, s32 elem_alignment, s32 block_elem_count, s32 block_initial_count, s32 block_growth_count, s32 block_max_count);

};
/**
 * END xCore namespace
 */
#endif	///< __X_ALLOCATOR_H__

