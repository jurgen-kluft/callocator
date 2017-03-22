#include "xbase\x_target.h"
#include "xbase\x_debug.h"
#include "xbase\x_memory_std.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xbase\x_idx_allocator.h"
#include "xbase\x_integer.h"
#include "xbase\x_tree.h"

#include "xallocator\x_allocator_hph_ext.h"
#include "xallocator\x_allocator_pool.h"

namespace xcore
{
	namespace xhpeha
	{
		/// High Performance External Heap Allocator
		/// ----------------------------------------
		/// This allocator manages external memory like video or sound memory.
		/// All bookkeeping data is in main memory so there is 'extra' memory
		/// required for this allocator.
		/// We can preallocate all necessary bookkeeping data from initialization
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
		///
		/// All the larger allocations go to the large bin. You can specify the
		/// minimum alignment for the large bin as well as the page size.
		/// The user also needs to supply a function for copying external memory to
		/// facilitate reallocation.
		///
		/// Main memory for 4000 allocs will use 4000 * 2*16 = 128.000 bytes.
		/// Additionally there is memory overhead for the free blocks:
		///   - Any free memory block in the small bin will use 2 * 16 = 32 bytes
		///   - Any free memory block in the large bin will use 3 * 16 = 48 bytes

		struct block_free;
		struct block_alloc;

		struct rbnode_alloc;
		struct rbnode_address;
		struct rbnode_size;

		struct smallbin;
		struct largebin;

		static void				insert_size(rbnode_size* root, block_free* block, x_iallocator* node_allocator, rbnode_size*& outNode);
		static bool				find_size(rbnode_size* root, xsize_t size, u32 alignment, rbnode_size*& outNode, block_free*& outBlock);
		static bool				find_size(rbnode_size* root, block_free* free_block, rbnode_size*& outNode);
		static void				remove_size(rbnode_size* root, rbnode_size* n, block_free* block);

		static void				insert_allocation(rbnode_alloc* root, block_alloc* block, x_iallocator* node_allocator, rbnode_alloc*& outNode);
		static bool				find_allocation(rbnode_alloc* root, void* ptr, rbnode_alloc*& outNode);
		static void				remove_allocation(rbnode_alloc* root, rbnode_alloc* node);

		static void				insert_address(rbnode_address* root, block_free* block, x_iallocator* node_allocator, rbnode_address*& outNode);
		static bool				find_address(rbnode_address* root, void* ptr, rbnode_address*& outNode);
		static void				remove_address(rbnode_address* root, rbnode_address* node);
		static void				merge_address(rbnode_address* root, rbnode_address*& node, rbnode_size* size_root, x_iallocator* allocator);

		static void				release_tree(xrbnode* root, x_iallocator* allocator);

		static void				insert_block(block_free* head, block_free* node);
		static void				remove_block(block_free*& head, block_free* node);

		static block_free*		split_block(block_free*& block, xsize_t size, u32 alignment, x_iallocator* node_allocator, xsize_t minimum_size, u32 minimum_alignment);

		static block_alloc*		convert_block(block_free* free_block);
		static block_free*		convert_block(block_alloc* alloc_block);

		static void				encode_bin_ptr(block_alloc* block, smallbin* bin);
		static void				encode_bin_ptr(block_alloc* block, largebin* bin);
		static void				decode_bin_ptr(void* bin_ptr, largebin*& large_bin, smallbin*& small_bin);

		static inline void*		get_aligned_ptr(void* ptr, u32 alignment)		{ return (void*)(((X_PTR_SIZED_INT)ptr + (alignment-1)) & ~(alignment-1)); }
		static inline void*		add_to_ptr(void* ptr, xsize_t size)				{ return (void*)((X_PTR_SIZED_INT)ptr + size); }

		/// Block that is free (size = 16 bytes)
		struct block_free
		{
			void				clear()						{ mPtr=mPrev=mNext=0; mSize=0; }
			void				set(void* p, u32 s)			{ mPrev=mNext=0; mPtr=p; mSize=s; }

			void*				mPtr;						/// Start of this free/used memory
			u32					mSize;						/// Size of free/used memory
			block_free*			mPrev;
			block_free*			mNext;
		};

		/// Block that is allocated (size = 16 bytes)
		struct block_alloc
		{
			void*				mPtr;						/// Start of this free/used memory
			u32					mSize;						/// Size of free/used memory
			u32					mLock;
			void*				mBin;						/// (smallbin* or largebin*)
		};

		/// BST node (size = 16 bytes)
		struct rbnode_alloc : public xrbnode
		{
			block_alloc*		mBlock;						/// 
		};

		/// BST node (size = 16 bytes)
		struct rbnode_address : public xrbnode
		{
			block_free*			mBlock;						/// (block_free) Only pointing to 1
		};

		/// BST node (size = 16 bytes)
		struct rbnode_size : public xrbnode
		{
			block_free*			mBlocks;					/// (block_free) Pointing to a ring list of blocks

			void				swap(rbnode_size* other)
			{
				block_free* b = mBlocks;
				mBlocks = other->mBlocks;
				other->mBlocks = b;
			}
		};

		/// Small bin (size = 16 bytes)
		struct smallbin
		{
			static smallbin*		create(x_iallocator* node_allocator, void* mem_begin, u32 mem_size, u16 prev_size, u16 cur_size)
			{
				smallbin* small_bin           = (smallbin*)node_allocator->allocate(sizeof(smallbin), 4);
				small_bin->mAddressTreeRoot   = (rbnode_address*)node_allocator->allocate(sizeof(rbnode_address), 4);
				small_bin->mSizeRange[0]      = prev_size;
				small_bin->mSizeRange[1]      = cur_size;
				small_bin->mNext              = NULL;
				small_bin->mPrev              = NULL;

				rbnode_address* address_node;
				block_free* free_block        = (block_free*)node_allocator->allocate(sizeof(block_free), 4);
				free_block->set(mem_begin, mem_size);
				insert_address(small_bin->mAddressTreeRoot, free_block, node_allocator, address_node);

				return small_bin;
			}

			bool					empty() const
			{
				return mAddressTreeRoot->get_child(rbnode_address::LEFT)==mAddressTreeRoot;
			}

			smallbin*				get_next() const	{ return mNext; }
			smallbin*				get_prev() const	{ return mPrev; }
			
			s32						cmp_range(u16 size) const
			{
				if (size<mSizeRange[0]) 
					return -1; 
				else if (size>mSizeRange[1]) 
					return 1; 
				else 
					return 0; 
			}

			block_alloc*			allocate(xsize_t size, u32 alignment, x_iallocator* allocator)	/// Allocate a block from this bin
			{
				rbnode_address* node = (rbnode_address*)mAddressTreeRoot->get_child(rbnode_address::LEFT);
				if (node == mAddressTreeRoot)
					return NULL;

				block_free* free_block = node->mBlock;		/// Remember the block, this is the return value of this function
				remove_address(mAddressTreeRoot, node);		/// Remove node from the 'address' tree
				
				u32 const fixed_size = mSizeRange[1];
				block_free* left_over_block = split_block(free_block, mSizeRange[0], alignment, allocator, fixed_size, fixed_size);
				if (left_over_block!=NULL)
				{
					// Insert it back into the address tree
					rbnode_address* node;
					insert_address(mAddressTreeRoot, left_over_block, allocator, node);
				}

				block_alloc* alloc_block = convert_block(free_block);
				encode_bin_ptr(alloc_block, this);
				return alloc_block;
			}

			void				deallocate(block_alloc* alloc_block, x_iallocator* allocator)
			{
				block_free* free_block = convert_block(alloc_block);
				rbnode_address* address_node;
				insert_address(mAddressTreeRoot, free_block, allocator, address_node);
				merge_address(mAddressTreeRoot, address_node, NULL, allocator);
			}

			void				release(x_iallocator* allocator)
			{
				release_tree(mAddressTreeRoot, allocator);
				allocator->deallocate(mAddressTreeRoot);
				allocator->deallocate(this);
			}

		private:
			inline				smallbin() : mAddressTreeRoot(NULL), mNext(NULL), mPrev(NULL) { mSizeRange[0]=0; mSizeRange[1]=0; }

			rbnode_address*		mAddressTreeRoot;			/// (rb_node_address) BST organized by address to support coalescing during deallocation
			u16					mSizeRange[2];				/// We do not need a free tree by size since the size is fixed, we can pop from the FreeTreeByAddress
			smallbin*			mNext;						/// (smallbin) List of bins
			smallbin*			mPrev;

		};

		// Large bin (size = <16 bytes)
		struct largebin
		{
			static largebin*	create(x_iallocator* node_allocator, void* mem_begin, u32 mem_size)
			{
				largebin* large_bin         = (largebin      *)node_allocator->allocate(sizeof(largebin      ), 4);
				large_bin->mAddressTreeRoot = (rbnode_address*)node_allocator->allocate(sizeof(rbnode_address), 4);
				large_bin->mSizeTreeRoot    = (rbnode_size   *)node_allocator->allocate(sizeof(rbnode_size   ), 4);
				large_bin->mDummy1          = NULL;

				// Insert the free block that covers all of the managed memory
				rbnode_address* address_node;
				block_free* free_block = (block_free*)node_allocator->allocate(sizeof(block_free), 4);
				free_block->set(mem_begin, mem_size);
				insert_address(large_bin->mAddressTreeRoot, free_block, node_allocator, address_node);

				return large_bin;
			}

			block_alloc*		allocate(xsize_t size, u32 alignment, x_iallocator* node_allocator, xsize_t minimum_size, u32 minimum_alignment)
			{
				block_free* free_block;
				rbnode_size* size_node;
				if (find_size(mSizeTreeRoot, size, alignment, size_node, free_block))
				{
					remove_size(mSizeTreeRoot, size_node, free_block);

					block_free* left_over = split_block(free_block, size, alignment, node_allocator, minimum_size, minimum_alignment);
					if (left_over!=NULL)
						insert_size(mSizeTreeRoot, left_over, node_allocator, size_node);

					void* ptr = free_block->mPtr;
					rbnode_address* address_node;
					find_address(mAddressTreeRoot, ptr, address_node);
					remove_address(mAddressTreeRoot, address_node);

					block_alloc* alloc_block = convert_block(free_block);
					encode_bin_ptr(alloc_block, this);
					return alloc_block;
				}
				return NULL;
			}

			void				deallocate(block_alloc* alloc_block, x_iallocator* node_allocator)
			{
				block_free* free_block = convert_block(alloc_block);
				rbnode_address* address_node;
				insert_address(mAddressTreeRoot, free_block, node_allocator, address_node);
				merge_address(mAddressTreeRoot, address_node, mSizeTreeRoot, node_allocator);
			}

			void				release(x_iallocator* node_allocator)
			{
				release_tree(mAddressTreeRoot, node_allocator);
				node_allocator->deallocate(mAddressTreeRoot);
				
				release_tree(mSizeTreeRoot, node_allocator);
				node_allocator->deallocate(mSizeTreeRoot);

				node_allocator->deallocate(this);
			}
		private:
			inline				largebin() : mAddressTreeRoot(NULL), mSizeTreeRoot(NULL), mDummy1(NULL) {}

			rbnode_address*		mAddressTreeRoot;			/// (rb_node_address) BST organized by address to support coalescing during deallocation
			rbnode_size*		mSizeTreeRoot;				/// (rb_node_size) BST organized by size to support allocation
			void*				mDummy1;
		};

		// Small bin allocator (size = 16 bytes)
		struct smallbin_allocator
		{
			void				init(x_iallocator* heap_allocator, void*& mem_begin, u32& mem_size, u32 binCount, u16* binSizes, x_iallocator* node_allocator)
			{
				mNumSmallBins = binCount;
				mSmallBins    = (smallbin**)heap_allocator->allocate(sizeof(smallbin*) * mNumSmallBins, 4);
				mFullBins     = (smallbin**)heap_allocator->allocate(sizeof(smallbin*) * mNumSmallBins, 4);
				
				u16 prev_bin_size = 0;
				for (u32 i=0; i<mNumSmallBins; ++i)
				{
					u16 const bin_size = binSizes[i];
					mFullBins[i]  = NULL;
					mSmallBins[i] = smallbin::create(node_allocator, mem_begin, 65536, prev_bin_size, bin_size);
					mem_begin     = add_to_ptr(mem_begin, 65536);
					mem_size     -= 65536;

					prev_bin_size = bin_size;
				}
			}

			block_alloc*		allocate(u16 size, u32 alignment, x_iallocator* node_allocator)
			{
				smallbin* sbin = find_bin(size);
				if (sbin != NULL)
				{
					//@TODO: Determine if the smallbin is full, if so allocate a new one
					//       and set it as the current smallbin. Add the full bin to the
					//       list of full bins.

					// Small bin allocation
					block_alloc* alloc_block = sbin->allocate(size, alignment, node_allocator);
					return alloc_block;
				}
				return NULL;
			}

			void				deallocate(smallbin* small_bin, block_alloc* block, x_iallocator* node_allocator)
			{
				small_bin->deallocate(block, node_allocator);
				//@TODO: Determine if the smallbin was full and now has one element free again.
				//       If so we need to unplug it from the mFullBins[] and add it to mSmallBins[]
				//       If the smallbin is full again we could destroy it?

			}

			void				release(x_iallocator* heap_allocator, x_iallocator* node_allocator)
			{
				for (u32 i=0; i<mNumSmallBins; ++i)
				{
					mSmallBins[i]->release(node_allocator);
					smallbin* bin = mFullBins[i];
					while (bin != NULL)
					{
						smallbin* next = bin->get_next();
						next->release(node_allocator);
						bin = next;
					}
				}
				heap_allocator->deallocate(mSmallBins);
				heap_allocator->deallocate(mFullBins);
			}

		private:
			smallbin*			find_bin(u16 size) const
			{
				if (mNumSmallBins == 0)
					return NULL;
				
				s32 const s = mSmallBins[mNumSmallBins-1]->cmp_range(size);
				if (s == 1)
					return NULL;
				else if (s == 0)
					return mSmallBins[mNumSmallBins-1];

				s32 bounds[2] = {0, (s32)mNumSmallBins};
				s32 mid = bounds[1] / 2;
				while (true)
				{
					s32 const r = mSmallBins[mid]->cmp_range(size);
					if (r == 0)
						return mSmallBins[mid];

					bounds[(1-r)>>1] = mid;
					u32 const distance = ((bounds[1] - bounds[0]) / 2);
					if (distance == 0)
						break;
					mid = bounds[0] + distance;
				}
				return NULL;
			}
		
			u32					mNumSmallBins;				/// Number of small bins
			smallbin**			mSmallBins;					/// Small bins
			smallbin**			mFullBins;					/// Full bins
		};

		/// Large Bin Allocator (16 bytes)
		/// Allocates from high to low
		struct largebin_allocator
		{
			void				init(x_iallocator* heap_allocator, void* mem_begin, u32 mem_size, x_iallocator* node_allocator, u16 minimum_alignment, u16 minimum_size)
			{
				mAllocator        = heap_allocator;
				mMinimumAlignment = minimum_alignment;
				mMinimumSize      = minimum_size;
				mLargeBin         = mLargeBin->create(node_allocator, mem_begin, mem_size);
			}

			void				release()
			{
				mLargeBin->release(mAllocator);
			}

			block_alloc*		allocate(xsize_t size, u32 alignment, x_iallocator* node_allocator)
			{
				block_alloc* alloc_block = mLargeBin->allocate(size, alignment, node_allocator, mMinimumSize, mMinimumAlignment);
				return alloc_block;
			}

			void				deallocate(largebin* large_bin, block_alloc* block, x_iallocator* node_allocator)
			{
				ASSERT(large_bin == mLargeBin);
				mLargeBin->deallocate(block, node_allocator);
			}

		private:
			x_iallocator*		mAllocator;
			u16					mMinimumAlignment;
			u16					mMinimumSize;
			largebin*			mLargeBin;
		};

		struct allocator
		{
			void				init(void* mem_begin, u32 mem_size, x_iallocator* heap_allocator, x_iallocator* allocator, u16 num_small_bins, u16* small_bin_sizes, u16 max_num_allocs)
			{
				mAllocator     = allocator;

				mTwoStageDeallocation = false;
				mPageSize      = 65536;

				mSmallBinAllocator.init(heap_allocator, mem_begin, mem_size, num_small_bins, small_bin_sizes, mNodeAllocator);
				mLargeBinAllocator.init(mAllocator, mem_begin, mem_size, mNodeAllocator, 256, 1024);

				// Allocate the root of the allocation tree
				mAllocations   = (rbnode_alloc*)mNodeAllocator->allocate(sizeof(rbnode_alloc), 4);
			}

			void				release()
			{
				mSmallBinAllocator.release(mAllocator, mNodeAllocator);
				mLargeBinAllocator.release();
				release_tree(mAllocations, mNodeAllocator);
				mNodeAllocator->deallocate(mAllocations);
				mNodeAllocator->release();

				mAllocator     = NULL;
				mNodeAllocator = NULL;
				mAllocations   = NULL;
			}

			void*				allocate(xsize_t size, u32 alignment);
			void				deallocate(void* p);

		protected:
			x_iallocator*		mAllocator;					/// Where we and our resources are allocated from
			x_iallocator*		mNodeAllocator;				/// Fixed size pool allocator for all structures of size = 16 bytes

			/// Configuration
			bool				mTwoStageDeallocation;		/// (default = false)
			xsize_t				mPageSize;					/// (default = 65536, 64KB)

			/// Small bins
			smallbin_allocator	mSmallBinAllocator;

			/// Large bin
			largebin_allocator	mLargeBinAllocator;

			/// BST of allocations (key == address)
			rbnode_alloc*		mAllocations;

		private:
			block_free*			allocate(block_free* from);
		};


		/*
		Allocate(size, alignment)

		     +-- small
		size |
		     +-- large


		+-- small

		    Take node out of address tree
		    Split it or not
		    Put back node if left-over from split into address tree
		    Insert node into global allocation tree, with pointer to block
		    Return address of external memory


		+-- large

		    Find best-fit node in size-tree and remove it from the tree
		    Split or not
		    Put back node if left-over from split into size tree & address tree
		                  else remove node from address tree
		    Insert node into global allocation tree, with pointer to block
		    Return address of external memory

		*/
		void*			allocator::allocate(xsize_t size, u32 alignment)
		{
			block_alloc* alloc_block = NULL;
			if (size < mPageSize)
				alloc_block = mSmallBinAllocator.allocate((u16)size, alignment, mNodeAllocator);

			if (alloc_block == NULL)
				alloc_block = mLargeBinAllocator.allocate(size, alignment, mNodeAllocator);

			if (alloc_block == NULL)
				return NULL;

			rbnode_alloc* node;
			insert_allocation(mAllocations, alloc_block, mNodeAllocator, node);
			return get_aligned_ptr(alloc_block->mPtr, alignment);
		}

		/*
		Deallocate(pointer)

		+-- large

		    Find and remove rbnode_alloc/block_alloc from global allocation tree
		    With the info in block_alloc we know the pointer to the bin, small or large
		    Convert block_alloc into a block_free
		    Convert rbnode_alloc to rbnode_address and add in address_tree of bin
		    See if we can coalesce with neighbors in the address tree
		    If not then create a rbnode_size, link block_free and add into size tree
		                or find rbnode_size in size tree and add block_free to it
		    If we can coalesce then pull those rbnode_address objects from the address
		    tree, merge them into one and that one back into the address tree. Also
		    pull out the rbnode_size (block_free) out of the size tree and insert one
		    rbnode_size back into it.

		*/
		void			allocator::deallocate(void* p)
		{
			rbnode_alloc* alloc_node;
			if (find_allocation(mAllocations, p, alloc_node))
			{
				block_alloc* alloc_block = alloc_node->mBlock;
				remove_allocation(mAllocations, alloc_node);

				// This 'alloc_node' has been removed from the tree, but
				// it has not been deallocated. We will pass it on when
				// calling bin->deallocate() so that it can be re-used.

				smallbin* small_bin;
				largebin* large_bin;
				decode_bin_ptr(alloc_block->mBin, large_bin, small_bin);

				if (large_bin != NULL)
				{
					mLargeBinAllocator.deallocate(large_bin, alloc_block, mNodeAllocator);
				}
				else if (small_bin != NULL)
				{
					mSmallBinAllocator.deallocate(small_bin, alloc_block, mNodeAllocator);
				}
			}
			else
			{
				ASSERTS(false, "Error: deallocation failed because incoming pointer is not present in our allocation data.");
			}
		}


		#ifdef DEBUG_RBTREE
			#define CHECK_RBTREE(root)		rb_check(root);
		#else
			#define CHECK_RBTREE(root)	
		#endif

		void		insert_size(rbnode_size* root, block_free* block, x_iallocator* node_allocator, rbnode_size*& outNode)
		{
			rbnode_size* lastNode = root;
			rbnode_size* curNode  = (rbnode_size*)root->get_child(rbnode_size::LEFT);;
			s32 s = rbnode_size::LEFT;
			while (curNode != root)
			{
				lastNode = curNode;
				if (block->mSize < curNode->mBlocks->mSize)
				{
					s = rbnode_size::LEFT;
				}
				else if (block->mSize > curNode->mBlocks->mSize)
				{
					s = rbnode_size::RIGHT;
				}
				else
				{
					// We have found a node that holds the same size, insert the
					// block into the linked list.
					insert_block(curNode->mBlocks, block);
					outNode = curNode;
					return;
				}
				curNode = (rbnode_size*)curNode->get_child(s);
			}

			// Allocate a new node, add 'block' into the linked list of that
			// node and insert it into the red-black tree.
			rbnode_size* node = (rbnode_size*)node_allocator->allocate(sizeof(rbnode_size), 4);
			node->clear(root);
			node->mBlocks = block;
			outNode = node;

			rb_attach_to(node, lastNode, s);
			rb_insert_fixup(*root, node);
			CHECK_RBTREE(root);
		}

		bool		find_size(rbnode_size* root, xsize_t size, u32 alignment, rbnode_size*& outNode, block_free*& outBlock)
		{
			rbnode_size* lastNode = root;
			rbnode_size* curNode  = (rbnode_size*)root->get_child(rbnode_size::LEFT);;
			s32 s = rbnode_size::LEFT;
			while (curNode != root)
			{
				lastNode = curNode;
				if (size < curNode->mBlocks->mSize)
				{
					s = rbnode_size::LEFT;
				}
				else if (size > curNode->mBlocks->mSize)
				{
					s = rbnode_size::RIGHT;
				}
				else
				{
					// We have found a node that holds the same size, we will
					// traverse the tree from here searching for the best-fit.
					break;
				}
				curNode = (rbnode_size*)curNode->get_child(s);
			}

			curNode = lastNode;
			if (s == rbnode_size::RIGHT)
			{
				// Get the successor since our size is bigger than the size
				// that the lastNode is holding
				curNode = (rbnode_size*)rb_inorder(rbnode_size::RIGHT, curNode);
			}

			ASSERT(x_intu::isPowerOf2(alignment));
			u32 align_mask = alignment - 1;

			// Now traverse to the right until we find a block that satisfies our
			// size and alignment.
			while (curNode != root)
			{
				// Iterate over all the blocks, we have to do this since every block
				// might have a different alignment to start with and there is a 
				// possibility that we find a best-fit.
				block_free* block = curNode->mBlocks;
				while (block != NULL)
				{
					// See if we can use this block
					if (size >= block->mSize)
					{
						// Verify the alignment
						u32 align_shift = (uptr)block->mPtr & align_mask;
						if (align_shift!=0)
						{
							// How many bytes do we have to add to the pointer to reach
							// the required alignment?
							align_shift = alignment - align_shift;

							// The pointer of the block does not match our alignment, so
							// here we have to check what happens when we add this difference
							// to the pointer and if the size is still sufficient to hold our
							// required size.
							if ((size + align_shift) <= block->mSize)
							{
								// Ok, we found a block which satisfies our request
								outNode  = curNode;
								outBlock = block;
								return true;
							}
						}
						else
						{
							// The alignment of the pointer is already enough to satisfy
							// our request, so here we can say we have found a best-fit.
							outNode  = curNode;
							outBlock = block;
							return true;
						}
					}
					block = block->mNext;
				}
			}
			return false;
		}

		bool		find_size(rbnode_size* root, block_free* free_block, rbnode_size*& outNode)
		{
			rbnode_size* curNode  = (rbnode_size*)root->get_child(rbnode_size::LEFT);;
			u32 size = free_block->mSize;
			s32 s = rbnode_size::LEFT;
			while (curNode != root)
			{
				if (size < curNode->mBlocks->mSize)
				{
					s = rbnode_size::LEFT;
				}
				else if (size > curNode->mBlocks->mSize)
				{
					s = rbnode_size::RIGHT;
				}
				else
				{
					block_free* block = curNode->mBlocks;
					while (block != NULL)
					{
						if (block->mPtr == free_block->mPtr && block->mSize == free_block->mSize)
						{
							outNode = curNode;
							return true;
						}
						block = block->mNext;
					}					
					break;
				}
				curNode = (rbnode_size*)curNode->get_child(s);
			}
			outNode = NULL;
			return false;
		}


		void		remove_size(rbnode_size* root, rbnode_size* node, block_free* block)
		{
			// Remove the block from the linked list of the node.
			// If there are no more blocks left then remove the node from the tree.
			remove_block(node->mBlocks, block);
			if (node->mBlocks == NULL)
			{
				rbnode_size* nill = root;
				rbnode_size* repl = node;
				s32 s = rbnode_size::LEFT;
				if (node->get_child(rbnode_size::RIGHT) != nill)
				{
					if (node->get_child(rbnode_size::LEFT) != nill)
					{
						repl = (rbnode_size*)node->get_child(rbnode_size::RIGHT);
						while (repl->get_child(rbnode_size::LEFT)->is_nill() == false)
							repl = (rbnode_size*)repl->get_child(rbnode_size::LEFT);
					}
					s = rbnode_size::RIGHT;
				}
				ASSERT(repl->get_child(1-s) == nill);
				bool red = repl->is_red();
				rbnode_size* replChild = (rbnode_size*)repl->get_child(s);

				rb_substitute_with(repl, replChild);
				ASSERT(root->is_black());

				if (repl != node)
					rb_switch_with(repl, node);

				ASSERT(root->is_black());

				if (!red) 
					rb_erase_fixup(root, replChild);

				CHECK_RBTREE(root);
			}
		}

		static inline s32		cmp_ptr(void* a, void* b)
		{
			if (a < b)		return -1;
			else if (a > b)	return 1;
			else			return 0;
		}

		static void				insert_allocation(rbnode_alloc* root, block_alloc* block, x_iallocator* node_allocator, rbnode_alloc*& outNode)
		{
			if (block == NULL)
				return;

			rbnode_alloc* lastNode = root;
			rbnode_alloc* curNode  = (rbnode_alloc*)root->get_child(rbnode_alloc::LEFT);;
			s32 s = rbnode_alloc::LEFT;
			while (curNode != root)
			{
				lastNode = curNode;
				s32 const c = cmp_ptr(block->mPtr, curNode->mBlock->mPtr);
				ASSERTS(c!=0, "Error: Same block already in the allocation tree?");
				s = (c+1)/2;
				curNode = (rbnode_alloc*)curNode->get_child(s);
			}

			// Allocate a new node, add 'block' into the linked list of that
			// node and insert it into the red-black tree.
			rbnode_alloc* node = (rbnode_alloc*)node_allocator->allocate(sizeof(rbnode_alloc), 4);
			node->clear(root);
			node->mBlock = block;
			outNode = node;

			rb_attach_to(node, lastNode, s);
			rb_insert_fixup(*root, node);
			CHECK_RBTREE(root);
		}

		static bool				find_allocation(rbnode_alloc* root, void* ptr, rbnode_alloc*& outNode)
		{
			rbnode_alloc* curNode  = (rbnode_alloc*)root->get_child(rbnode_alloc::LEFT);;
			s32 s = rbnode_alloc::LEFT;
			while (curNode != root)
			{
				s32 const c = cmp_ptr(ptr, curNode->mBlock->mPtr);
				if (c==0)
				{
					outNode = curNode;
					return true;
				}
				s = (c+1)/2;
				curNode = (rbnode_alloc*)curNode->get_child(s);
			}
			outNode = NULL;
			return false;
		}

		static void				remove_allocation(rbnode_alloc* root, rbnode_alloc* node)
		{
			// Remove the block from the linked list of the node.
			// If there are no more blocks left then remove the node from the tree.
			rbnode_alloc* nill = root;
			rbnode_alloc* repl = node;
			s32 s = rbnode_alloc::LEFT;
			if (node->get_child(rbnode_alloc::RIGHT) != nill)
			{
				if (node->get_child(rbnode_alloc::LEFT) != nill)
				{
					repl = (rbnode_alloc*)node->get_child(rbnode_alloc::RIGHT);
					while (repl->get_child(rbnode_alloc::LEFT)->is_nill() == false)
						repl = (rbnode_alloc*)repl->get_child(rbnode_alloc::LEFT);
				}
				s = rbnode_alloc::RIGHT;
			}
			ASSERT(repl->get_child(1-s) == nill);
			bool red = repl->is_red();
			rbnode_alloc* replChild = (rbnode_alloc*)repl->get_child(s);

			rb_substitute_with(repl, replChild);
			ASSERT(root->is_black());

			if (repl != node)
				rb_switch_with(repl, node);

			ASSERT(root->is_black());

			if (!red) 
				rb_erase_fixup(root, replChild);

			CHECK_RBTREE(root);
		}

		static void				insert_address(rbnode_address* root, block_free* block, x_iallocator* node_allocator, rbnode_address*& outNode)
		{
			if (block == NULL)
				return;

			rbnode_address* lastNode = root;
			rbnode_address* curNode  = (rbnode_address*)root->get_child(rbnode_address::LEFT);;
			s32 s = rbnode_address::LEFT;
			while (curNode != root)
			{
				lastNode = curNode;
				s32 const c = cmp_ptr(block->mPtr, curNode->mBlock->mPtr);
				ASSERTS(c!=0, "Error: Same block already in the address tree?");
				s = (c+1)/2;
				ASSERT(s==rbnode_address::LEFT || s==rbnode_address::RIGHT);
				curNode = (rbnode_address*)curNode->get_child(s);
			}

			rbnode_address* node = (rbnode_address*)node_allocator->allocate(sizeof(rbnode_address), 4);
			node->clear(root);
			node->mBlock = block;
			outNode = node;

			rb_attach_to(node, lastNode, s);
			rb_insert_fixup(*root, node);
			CHECK_RBTREE(root);
		}

		static bool				find_address(rbnode_address* root, void* ptr, rbnode_address*& outNode)
		{
			rbnode_address* curNode  = (rbnode_address*)root->get_child(rbnode_address::LEFT);;
			s32 s = rbnode_address::LEFT;
			while (curNode != root)
			{
				s32 const c = cmp_ptr(ptr, curNode->mBlock->mPtr);
				if (c == 0)
				{
					outNode = curNode;
					return true;
				}
				s = (c+1)/2;
				ASSERT(s==rbnode_address::LEFT || s==rbnode_address::RIGHT);
				curNode = (rbnode_address*)curNode->get_child(s);
			}
			outNode = NULL;
			return false;
		}

		static void				remove_address(rbnode_address* root, rbnode_address* node)
		{
			// Remove the block from the linked list of the node.
			// If there are no more blocks left then remove the node from the tree.
			rbnode_address* nill = root;
			rbnode_address* repl = node;
			s32 s = rbnode_address::LEFT;
			if (node->get_child(rbnode_address::RIGHT) != nill)
			{
				if (node->get_child(rbnode_address::LEFT) != nill)
				{
					repl = (rbnode_address*)node->get_child(rbnode_address::RIGHT);
					while (repl->get_child(rbnode_address::LEFT)->is_nill() == false)
						repl = (rbnode_address*)repl->get_child(rbnode_address::LEFT);
				}
				s = rbnode_address::RIGHT;
			}
			ASSERT(repl->get_child(1-s) == nill);
			bool red = repl->is_red();
			rbnode_address* replChild = (rbnode_address*)repl->get_child(s);

			rb_substitute_with(repl, replChild);
			ASSERT(root->is_black());

			if (repl != node)
				rb_switch_with(repl, node);

			ASSERT(root->is_black());

			if (!red) 
				rb_erase_fixup(root, replChild);

			CHECK_RBTREE(root);
		}

		static bool				are_sequential(rbnode_address* left, rbnode_address* right)
		{
			if (left->is_nill() || right->is_nill())
				return false;
			return (add_to_ptr(left->mBlock->mPtr, left->mBlock->mSize) == right->mBlock->mPtr);
		}

		static void				merge_address(rbnode_address* root, rbnode_address*& node, rbnode_size* size_root, x_iallocator* allocator)
		{
			// Get the predecessor and successor of 'node'
			// Check if predecessor-node-successor, predecessor-node or node-successor are sequential
			// Merge the sequential nodes into 1 node and remove the most right sequential address node(s) from the tree
			// Find the associated 'removed' address nodes in the size tree and remove/deallocate them
			// Deallocate the most right sequential address nodes
			rbnode_address*& mid  = node;
			rbnode_address* pred = (rbnode_address*)rb_inorder(xrbnode::LEFT , node);
			rbnode_address* succ = (rbnode_address*)rb_inorder(xrbnode::RIGHT, node);
			rbnode_address* to_remove[2] = { NULL, NULL };
			bool pred_mid_seq = are_sequential(pred, mid);
			bool mid_succ_seq = are_sequential(succ, mid);
			if (pred_mid_seq)
			{
				pred->mBlock->mSize += mid->mBlock->mSize;
				mid = pred;
				to_remove[0] = mid;
			}
			if (mid_succ_seq)
			{
				mid->mBlock->mSize += succ->mBlock->mSize;
				to_remove[1] = succ;
			}

			// Remove the associated size nodes from the size tree
			// and then remove the node from the address tree.
			// Lastly deallocate the node
			for (s32 i=0; i<2; ++i)
			{
				if (to_remove[i] == NULL)
					continue;
				
				rbnode_size* size_node;
				if (find_size(size_root, to_remove[i]->mBlock, size_node))
				{
					remove_size(size_root, size_node, to_remove[i]->mBlock);
					allocator->deallocate(size_node);
				}
				
				remove_address(root, to_remove[i]);
				allocator->deallocate(to_remove[i]);
			}
		}

		static void				release_tree(xrbnode* root, x_iallocator* allocator)
		{
			// Deallocate the whole tree
			xrbnode* it = (xrbnode*)root->get_child(xrbnode::LEFT);
			while ( it != root ) 
			{
				xrbnode* save;
				if ( it->get_child(xrbnode::LEFT) == root ) 
				{
					/* No left links, just kill the node and move on */
					save = (xrbnode*)it->get_child(xrbnode::RIGHT);
					if (it != root)
						allocator->deallocate(it);
				}
				else
				{
					/* Rotate away the left link and check again */
					save = (xrbnode*)it->get_child(xrbnode::LEFT);
					it->set_child(save->get_child(xrbnode::RIGHT), xrbnode::LEFT);
					save->set_child(it, xrbnode::RIGHT);
				}
				it = save;
			}
		}



		static void				insert_block(block_free* head, block_free* node)
		{
			node->mNext = head->mNext;
			node->mPrev = head;
			head->mNext = node;
			node->mNext->mPrev = node;
		}

		static void				remove_block(block_free*& head, block_free* node)
		{
			head = node->mNext;

			// Unlink the node from the linked list ring
			node->mPrev->mNext = node->mNext;
			node->mNext->mPrev = node->mPrev;

			if (head == node)
				head = NULL;
		}

		static block_free*		split_block(block_free*& block, xsize_t size, u32 alignment, x_iallocator* node_allocator, xsize_t minimum_size, u32 minimum_alignment)
		{
			void*     split_ptr  = get_aligned_ptr(add_to_ptr(get_aligned_ptr(block->mPtr, alignment), size), minimum_alignment);
			u32 const split_size = block->mSize - (u32)((uptr)split_ptr - (uptr)block->mPtr);

			block_free* split_block = NULL;
			if (split_size > minimum_size)
			{
				split_block = (block_free*)node_allocator->allocate(sizeof(block_free), sizeof(void*));
				split_block->mNext = NULL;
				split_block->mPrev = NULL;
				split_block->mPtr  = split_ptr;
				split_block->mSize = split_size;
				block->mSize -= split_size;			// Adjust the incoming block due to the split
			}
			return split_block;
		}

		static block_alloc*		convert_block(block_free* free_block)
		{
			block_alloc* block = (block_alloc*)free_block;
			block->mLock = 0;
			block->mBin  = NULL;
			return block;
		}

		static block_free*		convert_block(block_alloc* alloc_block)
		{
			block_free* block = (block_free*)alloc_block;
			block->mNext = NULL;
			block->mPrev = NULL;
			return block;
		}

		static void				encode_bin_ptr(block_alloc* block, smallbin* bin)
		{
			block->mBin = (smallbin*)((uptr)bin | 1);
		}
		static void				encode_bin_ptr(block_alloc* block, largebin* bin)
		{
			block->mBin = bin;
		}

		static void				decode_bin_ptr(void* ptr, largebin*& large_bin, smallbin*& small_bin)
		{
			if (((uptr)ptr & 1) == 0)
			{
				large_bin = (largebin*)ptr;
			}
			else
			{
				small_bin = (smallbin*)((uptr)ptr & ~(uptr)2);
			}
		}

	}

	class x_allocator_hph_ext : public x_iallocator
	{
		xhpeha::allocator		mAllocator;
		x_iallocator*			mHeapAllocator;
		x_iallocator*			mNodeAllocator;

	public:
		inline					x_allocator_hph_ext() : mHeapAllocator(NULL), mNodeAllocator(NULL)		{ }

		virtual const char*		name() const															{ return "External memory allocator"; }

		void					init(x_iallocator* heap_allocator, void* mem_begin, s32 mem_size) 
		{
			mHeapAllocator = heap_allocator;

			u16 max_num_allocs = 1000;

			xpool_params pool_params;
			pool_params.set_elem_size(sizeof(xhpeha::block_alloc));
			pool_params.set_elem_alignment(4);
			pool_params.set_block_size(256);
			pool_params.set_block_initial_count(((max_num_allocs*2)+(max_num_allocs/10))/256);
			pool_params.set_block_growth_count(0);
			pool_params.set_block_max_count(0);
			mNodeAllocator = gCreatePoolAllocator(mHeapAllocator, pool_params);

			u16 num_small_bins = 6;
			u16 small_bin_sizes[] = { 64, 128, 256, 512, 1024, 2048 };

			mAllocator.init(mem_begin, mem_size, mHeapAllocator, mNodeAllocator, num_small_bins, small_bin_sizes, max_num_allocs);
		}

		virtual void*			allocate(xsize_t size, u32 alignment)
		{
			return mAllocator.allocate(size, alignment);
		}

		virtual void*			reallocate(void* ptr, xsize_t size, u32 alignment)
		{
			ASSERT(false);
			return ptr;
		}

		virtual void			deallocate(void* ptr)
		{
			if (ptr!=NULL)
				return mAllocator.deallocate(ptr);
		}


		virtual void			release()
		{
			mAllocator.release();
			mNodeAllocator->release();
		}

		void*					operator new(xsize_t num_bytes)					{ return NULL; }
		void*					operator new(xsize_t num_bytes, void* mem)		{ return mem; }
		void					operator delete(void* pMem)						{ }
		void					operator delete(void* pMem, void* )				{ }
	};

	x_iallocator*		gCreateHphAllocator(x_iallocator* heap_allocator, void* mem_begin, u32 mem_size)
	{
		void* allocator_mem = heap_allocator->allocate(sizeof(x_allocator_hph_ext),4);
		x_allocator_hph_ext* allocator_hph_ext = new (allocator_mem) x_allocator_hph_ext();

		allocator_hph_ext->init(heap_allocator, mem_begin, mem_size);
		return allocator_hph_ext;
	}
};

