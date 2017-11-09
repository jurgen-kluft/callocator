#include "xbase/x_target.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"
#include "xbase/x_idx_allocator.h"
#include "xbase/x_memory_std.h"

namespace xcore
{
	/**
	 *  ------------------------------------------------------------------------------
	 *  Indexed pool allocator
	 *  ------------------------------------------------------------------------------
	 *  The indexed pool allocator uses blocks to allocate items from. Initially this
	 *  allocator has 1 or more blocks where every block has a number of items to 
	 *  allocate from. Every block has a free list and the allocator itself has a list
	 *  of blocks which have free items in them.
	 *  So the allocator is block oriented and can thus also free unused blocks (shrink).
	 *  It is also better for the cache to try and keep blocks full.
	 */
	class x_indexed_pool_allocator : public x_iidx_allocator
	{
	public:
		enum { NILL_IDX = 0xffffffff };
		typedef u32	block_idx_t;

		x_indexed_pool_allocator(x_iallocator* allocator) 
			: mAllocator(allocator)
			, mGrowNumBlocks(0)
			, mSizeOfBlockData(0)
			, mBlockIndexBitShift(0)
			, mItemsPerBlock(0)
			, mItemsPerBlockBitMask(0)
			, mSizeOfItem(0)
			, mAlignOfItem(0)
			, mNumBlocks(0)
			, mBlocks(NULL)
			, mBlockHeadAlloc(NILL_IDX)
			, mBlockTailAlloc(NILL_IDX)
			, mBlockHeadFree(NILL_IDX)
			, mAllocCount(0)
			, mBlockAllocator(NULL)
			, mObjectArrayAllocator(NULL)
		{
		}

		const char*			name() const	{ return "x_idx_allocator_pool; an indexed pool allocator that can grow and shrink"; }
		
		void				initialize(x_iallocator* block_allocator, x_iallocator* object_array_allocator, u32 size_of_item, u32 item_alignment, u32 num_items_per_block, u32 num_initial_blocks, u32 num_grow_blocks=0, u32 num_shrink_blocks=0)
		{
			mBlockAllocator = block_allocator;
			mObjectArrayAllocator = object_array_allocator;

			mSizeOfItem = x_intu::alignUp(size_of_item, item_alignment);
			mAlignOfItem = item_alignment;

			mInitialNumBlocks = num_initial_blocks;
			mGrowNumBlocks = num_grow_blocks;
			mShrinkNumBlocks = num_shrink_blocks;
			mSizeOfBlockData = num_items_per_block * mSizeOfItem;

			mItemsPerBlock = num_items_per_block;
			mItemsPerBlockBitMask = x_intu::ceilPower2(num_items_per_block) - 1;
			
			mBlockIndexBitShift = x_intu::ilog2(mItemsPerBlockBitMask+1);
		}

		void				initialize(u32 size_of_item, u32 item_alignment, u32 num_items_per_block, u32 num_initial_blocks, u32 num_grow_blocks=0, u32 num_shrink_blocks=0)
		{
			initialize(mAllocator, mAllocator, size_of_item, item_alignment, num_items_per_block, num_initial_blocks, num_grow_blocks, num_shrink_blocks);
		}

		XCORE_CLASS_PLACEMENT_NEW_DELETE

	protected:

		struct xblock
		{
			xbyte*				mData;

			/**
			 * We need to use indices here (relative) since this block will
			 * move in memory when the block array grows or shrinks.
			 */
			u32					mFreeListCnt;
			u32					mFreeListHead;
			block_idx_t			mNext;
			block_idx_t			mPrev;

			u32					to_idx(xbyte* ptr, u32 size_of_item)
			{
				u32 idx = ((u32)(ptr - mData) / size_of_item);
				return idx;
			}

			xbyte*				pop_free_item(u32 size_of_item)
			{
				if (NILL_IDX == mFreeListHead)
					return NULL;

				u32* p = (u32*)(mData + (mFreeListHead * size_of_item));
				mFreeListHead = *p;
				--mFreeListCnt;
				return (xbyte*)p;
			}

			void				push_free_item(u32 idx, u32 size_of_item)
			{
				u32* ptr = (u32*)(mData + (idx * size_of_item));
				*ptr = mFreeListHead;
				mFreeListHead = idx;
				++mFreeListCnt;
			}

			void				init(x_iallocator* allocator, u32 size_of_object, u32 align_of_object, u32 items_per_block)
			{
				mData = (xbyte*)allocator->allocate(size_of_object * items_per_block, align_of_object);
				make_freelist(items_per_block, size_of_object);
			}

			void				dealloc_data(x_iallocator* allocator)
			{
				allocator->deallocate(mData);
				mData = NULL;
				mFreeListCnt = 0;
				mFreeListHead = NILL_IDX;
			}

			void				make_freelist(u32 items_per_block, u32 size_of_item)
			{
				mFreeListCnt = items_per_block;
				mFreeListHead = 0;

				xbyte* data_ptr = mData;
				for (u32 i=1; i<items_per_block; ++i)
				{
					u32* item_ptr = (u32*)data_ptr;
					*item_ptr = i;
					data_ptr += size_of_item;
				}
				u32* item_ptr = (u32*)data_ptr;
				*item_ptr = NILL_IDX;
			}
		};

		void				grow_blocks(u32 num_blocks)
		{
			if (num_blocks > mNumBlocks)
			{
				xblock* new_blocks = (xblock*)mBlockAllocator->allocate(sizeof(xblock) * num_blocks, sizeof(void*));
				if (NULL!=mBlocks)
					x_memcopy(new_blocks, mBlocks, sizeof(xblock) * mNumBlocks);

				u32 const n = mNumBlocks;
				mNumBlocks = num_blocks;

				for (u32 i = n; i<num_blocks; ++i)
				{
					new_blocks[i].mFreeListCnt = 0;
					new_blocks[i].mFreeListHead = NILL_IDX;
					new_blocks[i].mNext = NILL_IDX;
					new_blocks[i].mPrev = NILL_IDX;
					new_blocks[i].init(mObjectArrayAllocator, mSizeOfItem, mAlignOfItem, mItemsPerBlock);
				}

				if (NULL != mBlocks)
					mBlockAllocator->deallocate(mBlocks);
				
				mBlocks = new_blocks;
				for (u32 i=n; i<num_blocks; ++i)
					link_alloc_block(i);
			}
		}

		void			unlink_alloc_block(block_idx_t idx)
		{
			if (NILL_IDX == idx)
				return;

			ASSERT(idx < mNumBlocks);
			block_idx_t next = mBlocks[idx].mNext;
			block_idx_t prev = mBlocks[idx].mPrev;
			mBlocks[idx].mNext = NILL_IDX;
			mBlocks[idx].mPrev = NILL_IDX;

			if (NILL_IDX != prev)
			{
				ASSERT(prev < mNumBlocks);
				mBlocks[prev].mNext = next;
			}
			if (NILL_IDX != next)
			{
				ASSERT(next < mNumBlocks);
				mBlocks[next].mPrev = prev;
			}
				
			if (idx == mBlockTailAlloc)
				mBlockTailAlloc = prev;
			if (idx == mBlockHeadAlloc)
				mBlockHeadAlloc = next;

			ASSERT(mBlockTailAlloc==NILL_IDX || mBlockTailAlloc < mNumBlocks);
			ASSERT(mBlockHeadAlloc==NILL_IDX || mBlockHeadAlloc < mNumBlocks);
			ASSERT(mBlockHeadFree ==NILL_IDX || mBlockHeadFree < mNumBlocks);
		}

		void			link_alloc_block(block_idx_t block_idx)
		{
			/**
			 *  Note:
			 *       Adding it to the tail gives us more chance to
			 *       shrink the block array. If we insert it as the
			 *       block to allocate from then theoretically the
			 *       chance that it will end up as unused is lower.
			 *       This is only valid when the user is allocating
			 *       and deallocating, once the user is only deallocating
			 *       it does not really matter what we do here.
			 */

			if (NILL_IDX == mBlockTailAlloc)
			{
				mBlockHeadAlloc = block_idx;
				mBlockTailAlloc = block_idx;
				mBlocks[mBlockTailAlloc].mNext = NILL_IDX;
				mBlocks[mBlockTailAlloc].mPrev = NILL_IDX;
			}
			else
			{
				mBlocks[mBlockTailAlloc].mNext = block_idx;
				mBlocks[block_idx].mPrev = mBlockTailAlloc;
				mBlocks[block_idx].mNext = NILL_IDX;
				mBlockTailAlloc = block_idx;
			}

			ASSERT(mBlockTailAlloc==NILL_IDX || mBlockTailAlloc < mNumBlocks);
			ASSERT(mBlockHeadAlloc==NILL_IDX || mBlockHeadAlloc < mNumBlocks);
			ASSERT(mBlockHeadFree ==NILL_IDX || mBlockHeadFree < mNumBlocks);
		}

		void			add_free_block(block_idx_t block_idx)
		{
			ASSERT(block_idx<mNumBlocks);
			if (NILL_IDX == mBlockHeadFree)
			{
				mBlockHeadFree = block_idx;
				mBlocks[mBlockHeadFree].mNext = NILL_IDX;
				mBlocks[mBlockHeadFree].mPrev = NILL_IDX;
			}
			else
			{
				ASSERT(mBlockHeadFree<mNumBlocks);
				mBlocks[mBlockHeadFree].mPrev = block_idx;
				mBlocks[block_idx].mPrev = NILL_IDX;
				mBlocks[block_idx].mNext = mBlockHeadFree;
				mBlockHeadFree = block_idx;
			}

			ASSERT(mBlockTailAlloc==NILL_IDX || mBlockTailAlloc < mNumBlocks);
			ASSERT(mBlockHeadAlloc==NILL_IDX || mBlockHeadAlloc < mNumBlocks);
			ASSERT(mBlockHeadFree ==NILL_IDX || mBlockHeadFree < mNumBlocks);
		}

		void				unlink_block(block_idx_t block_idx)
		{
			block_idx_t prev_block_idx = mBlocks[block_idx].mPrev;
			xblock* prev_block = prev_block_idx==NILL_IDX ? NULL : &mBlocks[prev_block_idx];
			block_idx_t next_block_idx = mBlocks[block_idx].mNext;
			xblock* next_block = next_block_idx==NILL_IDX ? NULL : &mBlocks[next_block_idx];

			xblock* cur_block = &mBlocks[block_idx];
			cur_block->mPrev = NILL_IDX;
			cur_block->mNext = NILL_IDX;

			if (prev_block!=NULL)
				prev_block->mNext = next_block_idx;
			if (next_block!=NULL)
				next_block->mPrev = prev_block_idx;

			if (mBlockHeadAlloc==block_idx) mBlockHeadAlloc = next_block_idx;
			if (mBlockTailAlloc==block_idx) mBlockTailAlloc = prev_block_idx;

			if (mBlockHeadFree==block_idx) mBlockHeadFree = next_block_idx;
		}

		block_idx_t		pop_free_blocks(u32 count)
		{
			if (mBlockHeadFree != NILL_IDX)
			{
				block_idx_t block_head_alloc = mBlockHeadFree;
				ASSERT(mBlockHeadFree < mNumBlocks);

				/// Only give 'mGrowNumBlocks' out of the free blocks
				for (block_idx_t i=1; i<mGrowNumBlocks; i++)
				{
					ASSERT(mBlockHeadFree < mNumBlocks);
					mBlockHeadFree = mBlocks[mBlockHeadFree].mNext;
					if (NILL_IDX == mBlockHeadFree)
						break;
				}

				/// Break the list into 2
				if (NILL_IDX != mBlockHeadFree)
				{
					block_idx_t const new_block_head_free = mBlocks[mBlockHeadFree].mNext;
					mBlocks[mBlockHeadFree].mNext = NILL_IDX;
					mBlockHeadFree = new_block_head_free;
				}

				return block_head_alloc;
			}
			else
			{
				return NILL_IDX;
			}
		}

		bool			should_shrink()
		{
			bool shrink = true;
			for (block_idx_t i=1; i<=mShrinkNumBlocks; ++i)
			{
				if (mBlocks[mNumBlocks - i].mData != NULL)
				{
					shrink = false;
					break;
				}
			}
			return shrink;
		}

		void			shrink_blocks(u32 num_blocks)
		{
			ASSERT(num_blocks < mNumBlocks);

			xblock* new_blocks = NULL;
			if (NULL!=mBlocks && num_blocks>0)
			{
				new_blocks = (xblock*)mBlockAllocator->allocate(sizeof(xblock) * num_blocks, sizeof(void*));
				x_memcopy(new_blocks, mBlocks, sizeof(xblock) * num_blocks);
			}

			for (u32 i=num_blocks; i<mNumBlocks; ++i)
			{
				ASSERT(mBlocks[i].mData == NULL);
				unlink_block(i);
			}

			if (NULL!=mBlocks)
				mBlockAllocator->deallocate(mBlocks);

			mBlocks = new_blocks;
			mNumBlocks = num_blocks;

			ASSERT(mBlockTailAlloc==NILL_IDX || mBlockTailAlloc < mNumBlocks);
			ASSERT(mBlockHeadAlloc==NILL_IDX || mBlockHeadAlloc < mNumBlocks);
			ASSERT(mBlockHeadFree ==NILL_IDX || mBlockHeadFree  < mNumBlocks);
		}		

		virtual void		init()
		{
			clear();
			grow_blocks(mInitialNumBlocks);
		}

		virtual void		clear()
		{
			ASSERT(mAllocCount==0);
			if (NULL!=mBlocks)
			{
				/// Deallocate block data
				for (block_idx_t i=0; i<mNumBlocks; ++i)
				{
					if (mBlocks[i].mData != NULL)
						mObjectArrayAllocator->deallocate(mBlocks[i].mData);
				}
				
				/// Deallocate block array
				mBlockAllocator->deallocate(mBlocks);
				mBlocks = NULL;
				mNumBlocks = 0;
			}

			/// Clear free list
			mBlockHeadAlloc = NILL_IDX;
			mBlockTailAlloc = NILL_IDX;
			mBlockHeadFree  = NILL_IDX;
		}

		virtual u32			size() const
		{
			return mAllocCount;
		}

		virtual u32			max_size() const	
		{
			if (mGrowNumBlocks == 0)
			{
				return mNumBlocks * mItemsPerBlock;
			}
			else
			{
				u32 total_items = (0x7fffffff >> mBlockIndexBitShift) * (mItemsPerBlock);
				return total_items;
			}
		}

		virtual void*		allocate(xsize_t size, u32 alignment)
		{
			ASSERT(size <= mSizeOfItem);
			void* p;
			iallocate(p);
			return p;
		}

		virtual u32			iallocate(void*& p)
		{
			if (NILL_IDX == mBlockHeadAlloc)
			{
				if (mGrowNumBlocks==0)
					return NILL_IDX;

				mBlockHeadAlloc = pop_free_blocks(mGrowNumBlocks);
				if (mBlockHeadAlloc != NILL_IDX)
				{
					/// Allocate data for those blocks taken from the list
					block_idx_t block_idx = mBlockHeadAlloc;
					do 
					{
						ASSERT(block_idx < mNumBlocks);
						mBlocks[block_idx].init(mBlockAllocator, mSizeOfItem, mAlignOfItem, mItemsPerBlock);
						block_idx = mBlocks[block_idx].mNext;
					} while (NILL_IDX != block_idx);
				}
				else
				{
					grow_blocks(mNumBlocks + mGrowNumBlocks);
				}
			}

			++mAllocCount;

			block_idx_t block_idx = mBlockHeadAlloc;
			ASSERT(block_idx < mNumBlocks);
			xblock* block = &mBlocks[block_idx];
			xbyte* ptr = block->pop_free_item(mSizeOfItem);
			u32 index = (block_idx<<mBlockIndexBitShift);
			index = index | block->to_idx(ptr, mSizeOfItem);

			if (0 == block->mFreeListCnt)
			{
				unlink_alloc_block(mBlockHeadAlloc);
			}

			p = (void*)ptr;
			return index;
		}

		virtual void*		reallocate(void* p, xsize_t new_size, u32 new_alignment)
		{
			if (new_size < mSizeOfItem)
			{
				return p;
			}
			ASSERTS(false, "Error: this indexed pool allocator has a fixed size allocation and the requested size is larger");
			return NULL;
		}

		virtual void		deallocate(void* p)
		{
			u32 idx = to_idx(p);
			ideallocate(idx);
		}

		virtual void		ideallocate(u32 idx)
		{
			ASSERT(mAllocCount>0);
			if (NILL_IDX != idx) 
			{
				block_idx_t block_idx = idx >> mBlockIndexBitShift;
				ASSERT(block_idx < mNumBlocks);
				xblock* block = &mBlocks[block_idx];
				ASSERT(block->mFreeListCnt < mItemsPerBlock);
				block->push_free_item(idx & mItemsPerBlockBitMask, mSizeOfItem);

				--mAllocCount;

				if ((block->mFreeListCnt == mItemsPerBlock) && mShrinkNumBlocks>0)
				{
					/**
					 *  This block could be removed since no one is using it
					 *  We can only shrink the mBlock array when this block is
					 *  at the end, we cannot erase items since that will kill
					 *  the indices that we have given to the user.
					 */
					block->dealloc_data(mObjectArrayAllocator);

					/**
					 * Unlink this block and add it to the mBlockHeadFree list
					 */
					unlink_alloc_block(block_idx);
					add_free_block(block_idx);

					if (should_shrink())
					{
						u32 const new_num_blocks = mAllocCount==0 ? 0 : ((mNumBlocks>=mShrinkNumBlocks) ? (mNumBlocks - mShrinkNumBlocks) : 0);
						shrink_blocks(new_num_blocks);
					}
				}
				else if (1 == block->mFreeListCnt)
				{
					ASSERT(block->mNext == NILL_IDX && block->mPrev == NILL_IDX);
					link_alloc_block(block_idx);
				}
			}
		}

		virtual void*		to_ptr(u32 idx) const
		{
			ASSERT(mAllocCount>0);
			block_idx_t block_idx = idx >> mBlockIndexBitShift;
			if (block_idx < mNumBlocks)
			{
				u32 item_idx  = idx & mItemsPerBlockBitMask;
				if (item_idx < mItemsPerBlock)
				{
					xblock* block = &mBlocks[block_idx];
					xbyte* item = block->mData + (mSizeOfItem * item_idx);
					return item;
				}
			}
			return NULL;
		}

		virtual u32			to_idx(void const* p) const
		{
			ASSERT(mAllocCount>0);

			xbyte* ptr = (xbyte*)p;
			/// We have to iterate over all the blocks
			for (block_idx_t i=0; i<mNumBlocks; ++i)
			{
				xblock& block = mBlocks[i];
				if (ptr>=block.mData && ptr<(block.mData + mSizeOfBlockData))
				{
					u32 item_idx = (u32)((ptr - block.mData) / mSizeOfItem);
					return (u32)(i << mBlockIndexBitShift) | item_idx;
				}
			}
			return NILL_IDX;
		}

		virtual void		release()
		{
			clear();
			this->~x_indexed_pool_allocator();
			mAllocator->deallocate(this);
		}

	private:
		x_iallocator*		mAllocator;

		u32					mInitialNumBlocks;
		u32					mGrowNumBlocks;
		u32					mShrinkNumBlocks;
		u32					mSizeOfBlockData;
		s32					mBlockIndexBitShift;
		u32					mItemsPerBlock;
		u32					mItemsPerBlockBitMask;
		
		u32					mSizeOfItem;
		u32					mAlignOfItem;

		u32					mNumBlocks;
		xblock*				mBlocks;
		block_idx_t			mBlockHeadAlloc;
		block_idx_t			mBlockTailAlloc;

		block_idx_t			mBlockHeadFree;

		u32					mAllocCount;

		x_iallocator*		mBlockAllocator;
		x_iallocator*		mObjectArrayAllocator;
	};

	x_iidx_allocator*		gCreatePoolIdxAllocator(x_iallocator* allocator, x_iallocator* block_array_allocator, x_iallocator* object_array_allocator, u32 size_of_object, u32 object_alignment, u32 num_objects_per_block, u32 num_initial_blocks, u32 num_grow_blocks, u32 num_shrink_blocks)
	{
		void* mem = allocator->allocate(sizeof(x_indexed_pool_allocator), sizeof(void*));
		x_indexed_pool_allocator* pool_allocator = new (mem) x_indexed_pool_allocator(allocator);
		pool_allocator->initialize(block_array_allocator, object_array_allocator, size_of_object, object_alignment, num_objects_per_block, num_initial_blocks, num_grow_blocks, num_shrink_blocks);
		return pool_allocator;
	}

	x_iidx_allocator*		gCreatePoolIdxAllocator(x_iallocator* allocator, u32 size_of_object, u32 object_alignment, u32 num_objects_per_block, u32 num_initial_blocks, u32 num_grow_blocks, u32 num_shrink_blocks)
	{
		void* mem = allocator->allocate(sizeof(x_indexed_pool_allocator), sizeof(void*));
		x_indexed_pool_allocator* pool_allocator = new (mem) x_indexed_pool_allocator(allocator);
		pool_allocator->initialize(size_of_object, object_alignment, num_objects_per_block, num_initial_blocks, num_grow_blocks, num_shrink_blocks);
		return pool_allocator;
	}

};
