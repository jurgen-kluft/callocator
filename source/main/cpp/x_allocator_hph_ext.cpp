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
	/// All the larger allocations go to the large bin. You can specify the
	/// minimum alignment for the large bin as well as the page size.
	/// The user also needs to supply a function for copying external memory to
	/// facilitate reallocation.

	/// Memory block info struct
	struct block
	{
		void*			mPtr;						/// Start of this free/used memory
		u32				mSize;						/// Size of free/used memory from @mMemoryPtr
		void*			mBin;						/// Either a smallbin* or largebin* (bit 0, 0=smallbin, 1=largebin)
		block*			mList;						/// In a tree of free nodes this will link nodes of the same size
	};

	/// Linked list node (16 bytes)
	struct ll_node
	{
		u32				mLock;						/// Lock-Locked(1)/Unlocked(0)
		block*			mBlock;
		ll_node*		mNext;
		ll_node*		mPrev;
	};

	/// A handle is actually a pointer to a ll_node with the 2 lowest bits set to 1
	inline bool			gIsExternalMemHandle(void *p)		{ return ((u32)p&3) == 3; }
	inline void*		gHandleToExternalMemPtr(void* p)	{ return ((ll_node*)((u32)p&~3))->mBlock->mPtr; }

	/// BST node (16 bytes)
	struct rb_node
	{
		rb_node*		mParent;					/// (rb_node) (bit 0 = Side-Left/Right)
		rb_node*		mChild[2];					/// (rb_node) Tree children, Left (bit 0 = Color-Red/Black) and Right
		block*			mSibling;					/// (block) Minimum of 1 sibling
	};

	/// Small bin (16 bytes)
	struct smallbin
	{
		rb_node*		mFreeTreeByAddress;			/// (rb_node) BST organized by address to support coalescing during deallocation
		ll_node*		mFreeList;					/// (ll_node) List of free nodes
		smallbin*		mNext;						/// (smallbin) List of bins
		smallbin*		mPrev;
	};

	// Large bin (8 bytes)
	struct largebin
	{
		rb_node*		mFreeTreeByAddress;			/// (rb_node) BST organized by address to support coalescing during deallocation
		rb_node*		mFreeTreeBySize;			/// (rb_node) BST organized by free size to support allocation
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

