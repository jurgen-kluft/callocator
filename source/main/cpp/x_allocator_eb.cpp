#include "xbase\x_target.h"
#include "xbase\x_types.h"
#include "xbase\x_allocator.h"
#include "xbase\x_integer.h"

#include "xallocator\x_allocator.h"

namespace xcore
{
	class x_allocator_eb : public x_iallocator
	{
	public:
							x_allocator_eb();
							x_allocator_eb(void* beginAddress, u32 size, x_iallocator* allocator, xextmem_copy extmem_copy);
		virtual				~x_allocator_eb();


		// bookkeeping data head
		class block
		{
		public:
								block()										{ }

			void				initialize(void* Addr, u32 Size, xbool IsFree)
			{
				address = Addr;
				size = Size;
				isFree = IsFree;
				previous = NULL;
				next = NULL;
			}

			void*				address;	// offset to the begin address
			u32					size;
			xbool				isFree;
			block*				previous;
			block*				next;
		};

		virtual const char*	name() const									{ return "EB allocator"; }

		void				initialize(void* beginAddress, u32 size, x_iallocator* allocator);		
		virtual void		release();
		
		virtual void*		allocate(u32 size, u32 alignment);
		virtual void*		reallocate(void* ptr, u32 size, u32 alignment);
		virtual void		deallocate(void* ptr);

		u32					getTotalSize()const;
		u32					getFreeSize()const;
		u32					getUsedSize()const;
		
		void*				operator new(xsize_t num_bytes)					{ return NULL; }
		void*				operator new(xsize_t num_bytes, void* mem)		{ return mem; }
		void				operator delete(void* pMem)						{ }
		void				operator delete(void* pMem, void* )				{ }

	private:
		void*				mBeginAddress;
		u32					mTotalSize;
		u32					mFreeSize;
		x_iallocator*		mAllocator;		// for block
		xextmem_copy		mExtmemCopy;
		block*				mElemListHead;	// head of block linked list 

		block*				createNewBlock(u32 addressOffset, u32 blockSize, xbool isFree);
		block*				getTheBestFitBlockForAllocation(u32 allocSize, u32 alignment);
		block*				getBlock(void* ptr);

		// Copy construction and assignment are forbidden
							x_allocator_eb(const x_allocator_eb&);
							x_allocator_eb& operator= (const x_allocator_eb&);
	};

// 	void x_allocator_eb::outputAllBlockState()
// 	{
// 		OutputDebugString("==============allocator states=====================\n");
// 		char temp[100];
// 		sprintf(temp, "Total size = %d\n", mTotalSize);
// 		OutputDebugString(temp);
// 		sprintf(temp, "Used size = %d\n", mTotalSize - mFreeSize);
// 		OutputDebugString(temp);
// 		block* currentBlock = mElemListHead;
// 		int i = 0;
// 		while(currentBlock != NULL)
// 		{
// 			sprintf(temp, "Block %d, isFree = %s, address = %10d, size = %d\n", i, currentBlock->isFree?"True ":"False", currentBlock->address, currentBlock->size);
// 			OutputDebugString(temp);
// 			currentBlock = currentBlock->next;
// 			i++;
// 		}
// 		OutputDebugString("===================================================\n");
// 	}

	x_allocator_eb::x_allocator_eb()
		: mBeginAddress(NULL)
		, mTotalSize(0)
		, mFreeSize(0)
		, mAllocator(NULL)
		, mElemListHead(NULL)
	{

	}

	x_allocator_eb::x_allocator_eb(void* beginAddress, u32 size, x_iallocator* allocator, xextmem_copy extmem_copy)
		: mBeginAddress(beginAddress)
		, mTotalSize(size)
		, mFreeSize(size)
		, mAllocator(allocator)
		, mExtmemCopy(extmem_copy)
		, mElemListHead(NULL)
	{ 
		initialize(beginAddress, size, allocator);
	}

	x_allocator_eb::~x_allocator_eb()
	{
		release();
	}

	void x_allocator_eb::initialize(void* beginAddress, u32 size, x_iallocator* allocator)
	{
		mBeginAddress = beginAddress;
		mTotalSize = size;
		mFreeSize = size;
		mAllocator = allocator;

		block* new_block = createNewBlock(0, mFreeSize, xTRUE);

		// add the block into the linked list
		mElemListHead = new_block;
	}

	void x_allocator_eb::release()
	{
		// Make sure we release our blocks
		block* b = mElemListHead;
		while (b != NULL)
		{
			block* d = b;
			b = b->next;
			mAllocator->deallocate(d);
		}
		mAllocator->deallocate(this);
	}

	void*	x_allocator_eb::allocate(u32 size, u32 alignment)
	{
		block* sourceBlock = getTheBestFitBlockForAllocation(size, alignment);

		u32 alignedSize = x_intu::alignUp(size, alignment);
		u32 unalignedAddr = (u32)(sourceBlock->address) + (u32)mBeginAddress;;
		u32 alignedAddr = (u32)(x_intu::alignUp(unalignedAddr, alignment));
		u32 addrOffset = alignedAddr - unalignedAddr;
		u32 neededSize = addrOffset + alignedSize;

		u32 offsetAddr = alignedAddr - (u32)mBeginAddress;
		block* new_block = createNewBlock( offsetAddr, alignedSize, xFALSE);

		block* firstBlock = new_block;
		block* lastBlock = new_block;

		if (addrOffset > 0)
		{
			// create a new free block before the allocated block
			block* freeBlock1 = createNewBlock( (u32)(sourceBlock->address), addrOffset, xTRUE);
			freeBlock1->next = new_block;
			new_block->previous = freeBlock1;
			firstBlock = freeBlock1;
		}

		if(neededSize < sourceBlock->size)
		{
			// create a new free block after the allocated block
			block* freeBlock2 = createNewBlock( offsetAddr + alignedSize, sourceBlock->size - neededSize, xTRUE);
			freeBlock2->previous = new_block;
			new_block->next = freeBlock2;
			lastBlock = freeBlock2;
		}

		// insert the new blocks into the linked list
		if(sourceBlock->previous != NULL)
		{
			sourceBlock->previous->next = firstBlock;
			firstBlock->previous = sourceBlock->previous;
		}
		else // sourceBlock is the head of linked list
		{
			ASSERT(sourceBlock == mElemListHead);
			mElemListHead = firstBlock;
		}

		if(sourceBlock->next != NULL)
		{
			sourceBlock->next->previous = lastBlock;
			lastBlock->next = sourceBlock->next;
		}

		mAllocator->deallocate(sourceBlock);

		// update the allocator
		mFreeSize-=alignedSize;

		return (void*)((u32)(new_block->address) + (u32)mBeginAddress);
	}

	void* x_allocator_eb::reallocate(void* ptr, u32 size, u32 alignment)
	{
		ASSERT(ptr != NULL);
		block* targetBlock = getBlock(ptr);
		ASSERT(targetBlock != NULL && !targetBlock->isFree);

		u32 alignedSize = x_intu::alignUp(size, alignment);
		u32 neededSize = alignedSize - targetBlock->size;
		void* newPtr = ptr;

		if(neededSize > 0)
		{
			block* nextBlock = targetBlock->next;
			if(nextBlock != NULL && nextBlock->isFree && nextBlock->size >= neededSize)
			{
				// resize the block
				targetBlock->size = alignedSize;

				// update the block next to the reallocated block
				u32 leftSize = nextBlock->size - neededSize;
				if(leftSize > 0)
				{
					nextBlock->address = (void*)((u32)(nextBlock->address) + neededSize);
					nextBlock->size = leftSize;
				}
				else
				{
					// no left memory in the next block, so remove it
					if(nextBlock->next != NULL)
					{
						nextBlock->next->previous = targetBlock;
						targetBlock->next = nextBlock->next;
					}
					else // there is no blocks after the target block
					{
						targetBlock->next = NULL;
					}
					mAllocator->deallocate(nextBlock);
				}

				// update the allocator
				mFreeSize -= neededSize;
			}
			else 
			{
				// Allocate a new block for new size
				newPtr = allocate(size, alignment);
				void* sourcePtr = (void*)((u32)(targetBlock->address) + (u32)mBeginAddress);
				
				// Copy the existing data from old block to the new block.
				mExtmemCopy(sourcePtr, targetBlock->size, newPtr, size);

				// deallocate target block
				deallocate(sourcePtr);

			} // END IF (nextBlock != NULL && nextBlock->isFree && nextBlock->size >= neededSize)
		}

		return newPtr;
	}

	void x_allocator_eb::deallocate(void* ptr)
	{
		ASSERT(ptr != NULL);

		block* targetBlock = getBlock(ptr);
		ASSERT(targetBlock != NULL && !targetBlock->isFree);

		u32 releasedSize = targetBlock->size;
		targetBlock->isFree = xTRUE;

		// IF its neighbors are also free blocks, combine them into a bigger one and deallocate the block
		block* prevBlock = targetBlock->previous;
		block* nextBlock = targetBlock->next;

		if(prevBlock != NULL && prevBlock->isFree)
		{
			targetBlock->size += prevBlock->size;
			targetBlock->previous = prevBlock->previous;
			targetBlock->address = prevBlock->address;
			
			if(prevBlock->previous != NULL)
			{
				prevBlock->previous->next = targetBlock;
			}

			if(prevBlock == mElemListHead)
			{
				mElemListHead = targetBlock;
			}

			mAllocator->deallocate(prevBlock);
		}

		if(nextBlock != NULL && nextBlock->isFree)
		{
			targetBlock->size += nextBlock->size;
			targetBlock->next = nextBlock->next;

			if(nextBlock->next != NULL)
			{
				nextBlock->next->previous = targetBlock;
			}

			mAllocator->deallocate(nextBlock);
		}

		// update the allocator
		mFreeSize += releasedSize;
	}

	x_allocator_eb::block* x_allocator_eb::createNewBlock(u32 addressOffset, u32 blockSize, xbool isFree)
	{
		block* new_block = static_cast<block*>(mAllocator->allocate(sizeof(block),X_MEMALIGN));
		ASSERT(new_block != 0);
		new_block->initialize((void*)addressOffset, blockSize, isFree);
		new_block->previous = NULL;
		new_block->next = NULL;
		return new_block;
	}

	x_allocator_eb::block* x_allocator_eb::getTheBestFitBlockForAllocation(u32 allocSize, u32 alignment)
	{
		block* currentBlock = mElemListHead;
		u32 alignedSize = x_intu::alignUp(allocSize, alignment);

		block* bestFitBlock = NULL;
		u32 bestFitBlockNumber = 3;	// the number of blocks after allocation, the maximum would be 3
		u32 bestFitUnusedSize = 0xFFFFFFFF; // the unused size of the free block after allocation.

		while( currentBlock != NULL)
		{
			if( currentBlock->isFree && alignedSize<= currentBlock->size )
			{
				u32 unalignedAddr = (u32)(currentBlock->address) + (u32)mBeginAddress;
				u32 alignedAddr = (u32)(x_intu::alignUp(unalignedAddr, alignment));
				u32 addrOffset = alignedAddr - unalignedAddr;
				u32 neededSize = addrOffset + alignedSize;

				if (neededSize <= currentBlock->size) // This block has enough room for allocation
				{
					u32 blockNum = 1;
					u32 unusedSize = 0;

					unusedSize += currentBlock->size - alignedSize;
					if(addrOffset > 0)	// need to create a free block before the allocated block
					{
						blockNum+=1;
					}
					if( neededSize < currentBlock->size) // need to create a free block after the allocated block
					{
						blockNum+=1;
					}

					if( (blockNum < bestFitBlockNumber) || (blockNum == bestFitBlockNumber && unusedSize < bestFitUnusedSize))
					{
						bestFitBlockNumber = blockNum;
						bestFitUnusedSize = unusedSize;
						bestFitBlock = currentBlock;
					}
				} //END IF(neededSize <= currentBlock->size)
			} //END IF( currentBlock->isFree)

			currentBlock = currentBlock->next;

		} // END WHILE

		ASSERT(bestFitBlock != NULL); // Fatal error! Could not find a block for allocation. Out of memory.
		return bestFitBlock;
	}

	x_allocator_eb::block* x_allocator_eb::getBlock(void* ptr)
	{
		block* currentBlock = mElemListHead;
		u32 offsetAddress = (u32)ptr - (u32)mBeginAddress;

		while(currentBlock != NULL)
		{
			if( offsetAddress == (u32)(currentBlock->address))
			{
				return currentBlock;
			}

			currentBlock = currentBlock->next;
			
		} // END WHILE

		ASSERT(0); // not found the block
		return NULL;
	}

	u32 x_allocator_eb::getTotalSize()const
	{
		return mTotalSize;
	}

	u32 x_allocator_eb::getFreeSize()const
	{
		return mFreeSize;
	}

	u32 x_allocator_eb::getUsedSize()const
	{
		return mTotalSize - mFreeSize;
	}


	x_iallocator*		gCreateEbAllocator(void* mem, u32 memsize, x_iallocator *allocator, xextmem_copy extmem_copy)
	{
		void* memForEBallocator = allocator->allocate(sizeof(x_allocator_eb), X_MEMALIGN);
		x_allocator_eb* ebAllocator = new (memForEBallocator) x_allocator_eb(mem, memsize, allocator, extmem_copy);
		return ebAllocator;
	}

};

