//==============================================================================
//
//  x_memory_heap.h
//
//==============================================================================
#ifndef __X_MEMORY_HEAP_H__
#define __X_MEMORY_HEAP_H__
#include "..\x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "..\x_memory.h"

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	#define XMEM_HEAP_DEBUG								1


	struct malloc_chunk
	{
		xsize_t					prev_foot;										///< Size of previous chunk (if free).  */
		xsize_t					head;											///< Size and inuse bits. */
		struct malloc_chunk*	fd;												///< double links -- used only if free. */
		struct malloc_chunk*	bk;
	};

	typedef struct malloc_chunk  mchunk;
	typedef struct malloc_chunk* mchunkptr;
	typedef struct malloc_chunk* sbinptr;										///< The type of bins of chunks */
	typedef u32 bindex_t;														///< Described below */
	typedef u32 binmap_t;														///< Described below */
	typedef u32 flag_t;															///< The type of various bit flag sets */

	struct malloc_tree_chunk 
	{
		///< The first four fields must be compatible with malloc_chunk
		xsize_t						prev_foot;
		xsize_t						head;
		struct malloc_tree_chunk*	fd;
		struct malloc_tree_chunk*	bk;

		struct malloc_tree_chunk*	child[2];
		struct malloc_tree_chunk*	parent;
		bindex_t					index;
	};

	typedef struct malloc_tree_chunk  tchunk;
	typedef struct malloc_tree_chunk* tchunkptr;
	typedef struct malloc_tree_chunk* tbinptr;									///< The type of bins of trees */

	struct malloc_segment 
	{
		xbyte*					base;											///< base address */
		xsize_t					size;											///< allocated size */
		struct malloc_segment*	next;											///< ptr to next segment */
		flag_t					sflags;											///< user and extern flag */
	};

	typedef struct malloc_segment  msegment;
	typedef struct malloc_segment* msegmentptr;

	#define NSMALLBINS        (32U)
	#define NTREEBINS         (32U)

	struct malloc_state
	{
		binmap_t			smallmap;
		binmap_t			treemap;
		xsize_t				dvsize;
		xsize_t				topsize;
		xbyte*				least_addr;
		mchunkptr			dv;
		mchunkptr			top;
		xsize_t				release_checks;
		xsize_t				magic;
		mchunkptr			smallbins[(NSMALLBINS+1)*2];
		tbinptr				treebins[NTREEBINS];
		xsize_t				footprint;
		xsize_t				max_footprint;
		flag_t				mflags;
		msegment			seg;
	};

	typedef struct malloc_state*    mstate;

	struct malloc_params 
	{
		volatile xsize_t	magic;
		xsize_t				page_size;
		flag_t				default_mflags;
	};

	typedef				void*	(*SysAllocFunc)(xsize_t size);
	typedef				void	(*SysFreeFunc)(void* ptrsize);

	//////////////////////////////////////////////////////////////////////////
	// A memory heap capable of managing multiple segments (based on dlmalloc)
	struct xmem_managed_size
	{
		xsize_t		mMaxSystemSize;
		xsize_t		mCurrentSystemSize;
		xsize_t		mCurrentInuseSize;
	};

	//////////////////////////////////////////////////////////////////////////
	// A memory heap capable of managing multiple segments (based on dlmalloc)
	class xmem_heap_base
	{
	protected:
		malloc_params		mParams;
		mstate				mState;

		SysAllocFunc		mSysAlloc;
		SysFreeFunc			mSysFree;

	public:
		void*				__alloc(xsize_t bytes);													///< Normal allocation
		void*				__allocA(xsize_t alignment, xsize_t size);								///< Aligned allocation
		void*				__allocR(void* ptr, xsize_t alignment, xsize_t size);										///< Re allocation
		void*				__allocN(xsize_t n_elements, xsize_t element_size);						///< Elements allocation
		void**				__allocIC(xsize_t n_elements, xsize_t element_size, void** chunks);		///< Independent continues with equal sized elements
		void**				__allocICO(xsize_t n_elements, xsize_t* element_sizes, void** chunks);	///< Independent continues with different size specified for every element
		void				__free(void* ptr);

		u32					__usable_size(void* mem);

		void				__stats(xmem_managed_size& stats);

#ifdef TARGET_PC
		void				__set_sys_calls(SysAllocFunc sys_alloc, SysFreeFunc sys_free)
		{
			mSysAlloc = sys_alloc;
			mSysFree = sys_free;
		}
#endif

	protected:
		void				__internal_malloc_stats(xmem_managed_size& stats);
		void*				__internal_realloc(mstate m, void* oldmem, xsize_t alignment, xsize_t bytes);
		void*				__internal_memalign(xsize_t alignment, xsize_t bytes);
		void**				__internal_ic_alloc(xsize_t n_elements, xsize_t* sizes, s32 opts, void* chunks[]);

		void				__add_segment(void* tbase, xsize_t tsize, s32 sflags);
		xsize_t				__release_unused_segments(mstate m);

		void*				__tmalloc_large(mstate m, xsize_t nb);
		void*				__tmalloc_small(mstate m, xsize_t nb);

		xsize_t				__footprint();
		xsize_t				__max_footprint();

		s32					__init_mparams();
		s32					__change_mparam(s32 param_number, s32 value);
	};


	//////////////////////////////////////////////////////////////////////////
	/// xmem_space is an opaque type representing an independent region of space that supports mspace_malloc, etc.
	class xmem_space : public xmem_heap_base
	{
	public:
		void				__manage(void* mem, xsize_t size);
		void				__destroy();

	private:
		void				__initialize(xbyte* tbase, xsize_t tsize);
	};

	//////////////////////////////////////////////////////////////////////////
	// A memory heap capable of managing multiple segments (based on dlmalloc)
	class xmem_heap : public xmem_heap_base
	{
	public:
		void				__initialize();
		void				__destroy();

		void				__manage(void* mem, xsize_t size);

		static xcore::u32	__sGetMemSize(void* mem);

	private:
		malloc_state		mStateData;
	};


	


	//==============================================================================
	// END xCore namespace
	//==============================================================================
};

#endif	/// __X_MEMORY_HEAP_H__