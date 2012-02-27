#include "xbase\x_target.h"
#include "xbase\x_debug.h"
#include "xbase\x_allocator.h"
#include "xbase\x_integer.h"
#include "xbase\private\x_rbtree.h"
#include "xallocator\private\x_allocator_small_ext.h"

namespace xcore
{
	namespace tlsf_ext
	{
		/*
		TLSF managing external memory where the bookkeeping data is in main memory.

		Page Size is 64KB

		When we allocate we need to find the best-fit (based on size), we could use the strategy from TLSF and do this with a 2D table.

		First level splits it based on power-of-2 size:
		[      4096][ 4, [4608, 4608+512] ]
		[      8192][ 8]
		[     16384][16]
		[     32768][32]
		[     65536][64]
		[    131072][64]		100K
		[    262144][64]
		[    524288][64]
		[   1048576][64]		1 MB
		[   2097152][64]		2
		[   4194304][64]		4
		[   8388608][64]		8
		[  16777216][64]		16 MB
		[  33554432][64]
		[  67108864][64]
		[ 134217728][64]		128 MB
		[ 268435456][64]		256 MB
		[ 536870912][64]
		[1073741824][64]		1 GB

		We also keep a bitmap of the first-level that marks which entry has items, we do this so that we do not have to iterate from
		beginning to end to find a usable chunk.

		Second level is an array that has split the [size, next-size] by N number of lists, during allocation and de-allocation these
		lists and bitmaps will be updated.

		Deallocation is tricky in the sense that we need to go from an external memory pointer to the actual linked-list node, we do
		this by using the pages-array to find the page, and in the page using a tree like structure (similar to a 'small' bin) we find
		the 


		*/

		struct xlargebin
		{
		};

		struct xsmallbin
		{
			xexternal_memory::xsmall_allocator*		pool;

			xsmallbin*		next;
			xsmallbin*		prev;
		};

		struct xsmallbins
		{
			xsmallbin*		bins;
		};

		struct xmemory
		{
			void*			managed_mem_begin;
			u32				managed_mem_size;

			xlargebin		large_bin;
		};

		void*	allocate(u32 size, u32 alignment)
		{
			// If size > small_size
			//		return large_bin.allocate(size, alignment);
			// Else
			//      return small_bin.allocate(size, alignment);
		}

		void	deallocate(void* ptr)
		{
			// If size > small_size
			//		return large_bin.allocate(size, alignment);
			// Else
			//      return small_bin.allocate(size, alignment);
		}
	}
}