#include "xbase\x_target.h"
#include "xbase\x_types.h"
#include "xbase\x_memory_std.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xbase\x_idx_allocator.h"
#include "xbase\x_integer.h"

#include "xallocator\x_allocator.h"

namespace xcore
{
	/// High Performance External Heap Allocator
	/// ----------------------------------------
	/// This allocator manages external memory like video or sound memory.
	/// All bookkeeping data is in main memory so there is 'extra' memory
	/// required for this allocator.
	/// We can pre-allocate all necessary bookkeeping data from initialization
	/// parameters given by the user.
	/// You can configure the allocator with:
	/// - deallocate in 1 or 2 stages;
	///   1 stage  : deallocate will treat the block as free for allocation.
	///   2 stages : first deallocate call will do some logic to free the block 
	///              but it will still be locked from using it for allocation.
	///              second deallocate will unlock it for allocation.
	/// - the page size
	/// - the number and sizes of small bins
	/// - the default alignment of the large bin
	/// - the size alignment of large allocations
	/// 
	/// This allocator manages many small bins and one large bin. The number
	/// of small bins can be configured as well as the allocation size that 
	/// every bin should manage (e.g. 64/128/256/512/1024).
	//
	/// All the larger allocations go to the large bin. You can specify the
	/// minimum alignment for the large bin as well as the page size.
	/// The user also needs to supply a function for copying external memory to
	/// facilitate reallocation.

	/// Memory block info struct
	struct block
	{
		void*			mPtr;						/// Start of this free/used memory
		u32				mLock;						/// Bit 32, Lock-Locked(1)/Unlocked(0)
		u32				mSize;						/// Size of free/used memory
		void*			mLink;						/// (ll_node*) when free or (bin*) when allocated (bit 0 = Bin-Small/Large)
	};

	/// Linked list node (16 bytes)
	struct ll_node
	{
		rbnode_size*	mParent;					/// Our parent
		ll_node*		mLink[2];					/// Next/Prev, used for linking nodes of the same size
		block*			mBlock;
	};

	/// A handle is actually a pointer to a block with the 2 lowest bits set to 1
	inline bool		gIsExternalMemHandle(void *p)		{ return ((u32)p&3) == 3; }
	inline void*	gHandleToExternalMemPtr(void* p)	{ return ((block*)((u32)p&~3))->mPtr; }

	/// BST node (16 bytes)
	struct rbnode_address
	{
		rbnode_address*	mParent;					/// (rbnode_address) (bit 0 = Side-Left/Right)
		rbnode_address*	mChild[2];					/// (rbnode_address) Tree children, Left (bit 0 = Color-Red/Black) and Right
		block*			mBlock;						/// (block) Our associated size node, so that when we coalesce we can 
													/// also pull out the nodes from the FreeTreeBySize. Within the rbnode_size
													/// we still need to search through the ll_nodes our associated block.
	};

	struct rbnode_size
	{
		rbnode_size*	mParent;					/// (rbnode_size) (bit 0 = Side-Left/Right, bit 1 = Color-Red/Black)
		rbnode_size*	mChild[2];					/// (rbnode_size) Tree children, Left and Right
		ll_node*		mSibling;					/// (block) Minimum of 1 sibling
	};

	/// Small bin (16 bytes)
	struct smallbin
	{
		rbnode_address*	mFreeTreeByAddress;			/// (rb_node_address) BST organized by address to support coalescing during deallocation
		void*			mDummy1;					/// We do not need a free tree by size since the size is fixed, we can pop from the FreeTreeByAddress
		smallbin*		mNext;						/// (smallbin) List of bins
		smallbin*		mPrev;
	};

	// Large bin (8 bytes)
	struct largebin
	{
		rbnode_address*	mFreeTreeByAddress;			/// (rb_node_address) BST organized by address to support coalescing during deallocation
		rbnode_size*	mFreeTreeBySize;			/// (rb_node_size) BST organized by size to support allocation
		void*			mDummy1;
		void*			mDummy2;
	};

	// Small bin allocator
	struct smallbin_allocator
	{
		u32				mNumSmallBins;				/// Number of small bins
		u32*			mSmallBinSizes;				/// Size of every small bin
		smallbin**		mSmallBins;					/// Small bins
		smallbin**		mFullBins;					/// Full bins
	};

	/// Allocates from high to low
	struct largebin_allocator
	{
		u16				mDefaultAlignmentAddress;
		u16				mDefaultAlignmentSize;
		largebin		mLargeBin;
	};

	struct hph_ext_allocator
	{
		x_iallocator*		mAllocator;				/// Where we and our resources are allocated from
		x_iallocator*		mNodeAllocator;			/// Fixed size pool allocator for rb_node/ll_node/block/largebin/smallbin/smallbin_allocator/largebin_allocator, element size = 16 bytes

		/// Configuration
		bool				mTwoStageDeallocation;
		u32					mPageSize;

		/// Small bins
		smallbin_allocator	mSmallBinAllocator;

		/// Large bin
		largebin_allocator	mLargeBinAllocator;

		/// Doubly linked list of allocations
		ll_node*			mAllocations;
		ll_node*			mDeallocations;			/// 1st stage deallocation

		/// e.g.
		/// u16 sizes[] = { 64, 128, 256, 512, 1024 };
		/// init(sizeof(sizes)/sizeof(u16), sizes, 4000);
		/// 4000 allocations should include the overhead of holding on to deallocated blocks 
		/// that cannot be merged. Main memory for 4000 allocs will use 4000 * 2*16 = 128.000 bytes.
		/// Any free memory chunk will use 3 * 16 = 48 bytes
		void				init(u16 num_small_bins, u16* small_bin_sizes, u16 max_num_allocs);

		/// Allocate returns a handle, get the pointer to external
		/// memory as follows: 
		///       void* handle = externalMemoryAllocator->allocate(256, 16);
		///       void* external_memory_ptr = gHandleToExternalMemPtr(handle);
		///       externalMemoryAllocator->deallocate(handle);
		///       externalMemoryAllocator->deallocate(handle);		/// If it is a 2 stage deallocation
		void*				allocate(u32 size, u32 alignment);		/// Returns a handle
		void				deallocate(void* p);
	};
};

