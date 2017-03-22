#include "xbase\x_target.h"
#include "xbase\x_debug.h"
#include "xbase\x_memory_std.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xbase\x_tree.h"

#include "xallocator\x_allocator_pool.h"

namespace xcore
{
	void	xpool_params::set_elem_size(u32 size)									{ mElemSize = size; }
	void	xpool_params::set_elem_alignment(u32 alignment)							{ mElemAlignment = alignment; }
	void	xpool_params::set_block_size(u32 num_elements)							{ mBlockElemCount = num_elements; }
	void	xpool_params::set_block_initial_count(u32 initial_num_blocks)			{ mBlockInitialCount = initial_num_blocks; }
	void	xpool_params::set_block_growth_count(u32 growth_num_blocks)				{ mBlockGrowthCount = growth_num_blocks; }
	void	xpool_params::set_block_max_count(u32 max_num_blocks)					{ mBlockMaxCount = max_num_blocks; }

	u32		xpool_params::get_elem_size() const										{ return mElemSize; }
	u32		xpool_params::get_elem_alignment() const								{ return mElemAlignment; }
	u32		xpool_params::get_block_size() const									{ return mBlockElemCount; }
	u32		xpool_params::get_block_initial_count() const							{ return mBlockInitialCount; }
	u32		xpool_params::get_block_growth_count() const							{ return mBlockGrowthCount; }
	u32		xpool_params::get_block_max_count() const								{ return mBlockMaxCount; }


	namespace xpool_allocator
	{
		/**
		@brief		Fixed size type, element
		@desc		It implements linked list behavior for free elements in the block.
		**/
		class xelement	
		{
		public:
			// This part is a little bit dirty...
			xelement*			getNext()							{ return *reinterpret_cast<xelement**>(&mData); }
			void				setNext(xelement* next)				{ xelement** temp = reinterpret_cast<xelement**>(&mData); *temp = next; }
			void*				getObject()							{ return (void*)&mData; }
		private:
			u32					mData;	
		};

		/**
		@brief	xblock contains an array of xelement objects. This is the smallest
				memory chunk which is allocated from system (via allocated/deallocator) and is the
				smallest unit by which the allocator can grow. The inElemSize and inNumElements 
				parameter sent to init function determines the size of the block.

				xblock has a 32 bytes overhead caused by the bookkeeping data.
		**/
		class xblock : public xrbnode
		{
		public:
								xblock() : mElementArrayBegin (0), mElementArrayEnd (0), mFreeList (0), mNumManagedElements(0), mNumFreeElements(0) { clear(this); }

			static xblock*		create(x_iallocator* allocator, u32 inElemSize, u32 inNumElements, s32 inAlignment);

			void				reset(u32 sizeOfElement);
			void				release(x_iallocator* allocator);
			
			inline bool			full() const						{ return mFreeList == NULL; }
			inline bool			empty() const						{ return mNumManagedElements == mNumFreeElements; }	

			inline s32			compare(xblock* block)
			{ 
				if (block == this) 
					return 0; 
				else if (block->mElementArrayBegin < mElementArrayBegin) 
					return -1;
				else 
					return 1; 
			}

			inline s32			compare_ptr(void* ptr)
			{
				if (ptr < mElementArrayBegin) 
					return -1;
				else if (ptr > mElementArrayEnd) 
					return 1; 
				else
					return 0; 
			}

			inline xelement*	pop()
			{
				xelement* e = mFreeList;
				mFreeList = e->getNext();
				--mNumFreeElements;
				return e;
			}
			
			inline void			push(xelement* element)
			{
				ASSERT(element >= mElementArrayBegin && element < mElementArrayEnd);
				element->setNext(mFreeList);
				mFreeList = element;
				++mNumFreeElements;
			}

			XCORE_CLASS_PLACEMENT_NEW_DELETE

		private:
			void				init(xbyte* elementArray, u32 inElemSize, u32 inNumElements, s32 inAlignment);
			inline xelement*	at(u32 index, u32 sizeOfElement)		{ return (xelement*)((xbyte*)mElementArrayBegin + (index * sizeOfElement)); }

			xelement*			mElementArrayBegin;
			void*				mElementArrayEnd;
			xelement*			mFreeList;
			u32					mNumManagedElements;
			u32					mNumFreeElements;
		};

		xblock*		xblock::create(x_iallocator* allocator, u32 inElemSize, u32 inNumElements, s32 inAlignment)
		{
			ASSERT(inElemSize != 0);			// Check input parameters
			ASSERT(inNumElements > 0);
		
			u32 const elementArraySize = inElemSize * inNumElements;
			u32 const allocationSize = elementArraySize + sizeof(xblock);
			xbyte* p = (xbyte*)allocator->allocate(allocationSize, inAlignment);

			xblock* block = new (p + elementArraySize) xblock();
			block->init(p, inElemSize, inNumElements, inAlignment);
			return block;
		}

		void		xblock::init(xbyte* elementArray, u32 inElemSize, u32 inNumElements, s32 inAlignment)
		{
			mNumManagedElements = inNumElements;
			mNumFreeElements    = inNumElements;

			mElementArrayBegin = static_cast<xelement*>((void*)elementArray);
			mElementArrayEnd   = static_cast<void    *>((xbyte*)elementArray + (mNumManagedElements*inElemSize));
			ASSERT(mElementArrayBegin != 0);
			reset(inElemSize);
		}

		void		xblock::reset(u32 sizeOfElement)
		{
			mFreeList = NULL;
			for (s32 i=mNumManagedElements-1; i>=0; --i)
			{
				xelement* e = at(i, sizeOfElement);
				e->setNext(mFreeList);
				mFreeList = e;
			}
		}


		void xblock::release(x_iallocator* allocator)
		{ 
			allocator->deallocate(mElementArrayBegin);
		}

		/**
		@brief	xblocktree

		@desc	It holds a tree of blocks

		@note	Used a red-black tree implementation to insert/find/remove blocks in a tree.
		**/
		struct xblocktree
		{
			inline				xblocktree() : mNumBlocks(0)		{ mRoot.clear(&mRoot); }

			u32					size() const						{ return mNumBlocks; }
			bool				empty() const						{ return mNumBlocks == 0; }

			xblock*				pop()
			{
				if (mNumBlocks == 0)
					return NULL;

				// The node to remove
				xblock* root = &mRoot;
				xblock* node = root;
				while (node->get_child(xblock::LEFT)!=root)
					node = (xblock*)node->get_child(xblock::LEFT);

				if (node == &mRoot)
					return NULL;

				xblock* endNode = root;
				xblock* repl = node;
				s32 s = xblock::LEFT;
				if (node->get_child(xblock::RIGHT) != endNode)
				{
					if (node->get_child(xblock::LEFT) != endNode)
					{
						repl = (xblock*)node->get_child(xblock::RIGHT);
						while (repl->get_child(xblock::LEFT) != endNode)
							repl = (xblock*)repl->get_child(xblock::LEFT);
					}
					s = xblock::RIGHT;
				}
				ASSERT(repl->get_child(1-s) == endNode);
				bool red = repl->is_red();
				xblock* replChild = (xblock*)repl->get_child(s);

				rb_substitute_with(repl, replChild);
				ASSERT(endNode->is_black());

				if (repl != node)
					rb_switch_with(repl, node);

				ASSERT(endNode->is_black());

				if (!red) 
					rb_erase_fixup(root, replChild);

#ifdef DEBUG_RBTREE
				rb_check(root);
#endif		
				--mNumBlocks;
				node->clear(node);
				return node;
			}

			void				push(xblock* block)
			{
				xblock* root     = &mRoot;
				xblock* endNode  = root;
				xblock* lastNode = root;
				xblock* curNode  = (xblock*)root->get_child(xblock::LEFT);;
				s32 s = xblock::LEFT;

				while (curNode != endNode)
				{
					lastNode = curNode;
					s32 c = curNode->compare(block);
					s = (c < 0) ? xblock::LEFT : xblock::RIGHT;
					curNode = (xblock*)curNode->get_child(s);
				}

				rb_attach_to(block, lastNode, s);
				rb_insert_fixup(*root, block);
				++mNumBlocks;

#ifdef DEBUG_RBTREE
				rb_check(root);
#endif			
			}

			xblock*				find(void* ptr)
			{
				xblock* root = &mRoot;
				xblock* nill = root;
				xblock* it = (xblock*)root->get_child(xblock::LEFT);
				while ( it != nill )
				{
					s32 cmp = it->compare_ptr(ptr);
					if ( cmp == 0 )
						break;
					it = (xblock*)it->get_child((++cmp)>>1);
				}

				// 'it' is the block that contains this pointer (element)
				return it!=root ? it : NULL;
			}

			void				remove(xblock* block)
			{
				xblock* root    = &mRoot;
				xblock* endNode = root;
				ASSERT(block != root);

				xblock* repl = block;
				s32 s = xblock::LEFT;
				if (block->get_child(xblock::RIGHT) != endNode)
				{
					if (block->get_child(xblock::LEFT) != endNode)
					{
						repl = (xblock*)block->get_child(xblock::RIGHT);
						while (repl->get_child(xblock::LEFT) != endNode)
							repl = (xblock*)repl->get_child(xblock::LEFT);
					}
					s = xblock::RIGHT;
				}
				
				ASSERT(repl->get_child(1-s) == endNode);

				bool red = repl->is_red();
				xblock* replChild = (xblock*)repl->get_child(s);

				rb_substitute_with(repl, replChild);
				ASSERT(endNode->is_black());

				if (repl != block)
					rb_switch_with(repl, block);

				ASSERT(endNode->is_black());

				if (!red) 
					rb_erase_fixup(root, replChild);

				--mNumBlocks;

#ifdef DEBUG_RBTREE
				rb_check(root);
#endif
				block->clear(block);
			}

			void				reset(u32 inNumElements, u32 inElemSize)
			{
				xblock* root = &mRoot;
				xblock* node = (xblock*)mRoot.get_child(xblock::LEFT);
				while (node->get_child(xblock::LEFT)!=root)
					node = (xblock*)node->get_child(xblock::LEFT);
				while (node != root)
				{
					node->reset(inElemSize);
					node = (xblock*)rb_inorder(0, node);
				}
			}

			void				release(x_iallocator* allocator)
			{
				xblock* root = &mRoot;
				xblock* it   = (xblock*)mRoot.get_child(xblock::LEFT);
				while ( it != root ) 
				{
					xblock* save;
					if ( it->get_child(xblock::LEFT) == root ) 
					{
						/* No left links, just kill the node and move on */
						save = (xblock*)it->get_child(xblock::RIGHT);
						if (it != root)
						{
							it->release(allocator);
							--mNumBlocks;
						}
					}
					else
					{
						/* Rotate away the left link and check again */
						save = (xblock*)it->get_child(xblock::LEFT);
						it->set_child(save->get_child(xblock::RIGHT), xblock::LEFT);
						save->set_child(it, xblock::RIGHT);
					}
					it = save;
				}
			}
		private:
			xblock				mRoot;
			u32					mNumBlocks;
		};


		/**
		@brief	xallocator_imp is a fast allocator for objects of fixed size.

		@desc	It preallocates (from @allocator) @inInitialBlockCount blocks with @inBlockSize T elements.
				By calling Allocate an application can fetch one T object. By calling Deallocate
				an application returns one T object to pool.
				When all objects on the pool are used, pool can grow to accommodate new requests.
				@inGrowthCount specifies by how many blocks the pool will grow.
				Reset reclaims all objects and reinitializes the pool. The parameter RestoreToInitialSize
				can be used to resize the pool to initial size in case it grew.

		@note	This allocator does not guarantee that two objects allocated sequentially are sequential in memory.
		**/
		class xallocator_imp : public x_iallocator
		{
		public:
									xallocator_imp();

			// @inElemSize			This determines the size in bytes of an element
			// @inBlockElemCnt		This determines the number of elements that are part of a block
			// @inInitialBlockCount	Initial number of blocks in the memory pool
			// @inBlockGrowthCount	Number of blocks by which it will grow if all space is used
			// @inElemAlignment		Alignment of the start of each pool (can be 0, which creates fixed size memory pool)
									xallocator_imp(x_iallocator* allocator, u32 inElemSize, u32 inBlockElemCnt, u32 inInitialBlockCount, u32 inBlockGrowthCount, u32 inBlockMaxCount, u32 inElemAlignment = 0);
			virtual					~xallocator_imp();

			virtual const char*		name() const									{ return TARGET_FULL_DESCR_STR " Fixed size pool allocator"; }

			///@name	Should be called when created with default constructor
			//			Parameters are the same as for constructor with parameters
			void					init();

			virtual void*			allocate(xsize_t size, u32 alignment);
			virtual void*			reallocate(void* p, xsize_t size, u32 alignment);
			virtual void			deallocate(void* p);

			///@name	Placement new/delete
			XCORE_CLASS_PLACEMENT_NEW_DELETE

		protected:
			///@name	Resets allocator
			void					reset(xbool inRestoreToInitialSize = xFALSE);

			struct Blocks
			{
										Blocks() : mCurrent(0), mSize(0)	{ }

				u32						size() const
				{
					ASSERT(mSize == (mFree.size() + mFull.size() + (mCurrent!=NULL?1:0)));
					return mSize;
				}

				void					reset(u32 inNumElements, u32 inElemSize)
				{
					while (!mFull.empty())
					{
						xblock* block = mFull.pop();
						mFree.push(block);
					}
					mFree.reset(inNumElements, inElemSize);
					mCurrent = NULL;
				}

				void*					allocate()
				{
					if (mCurrent == NULL)
						mCurrent = mFree.pop();

					void* p = NULL;
					if (mCurrent != NULL)
					{
						xelement* element = mCurrent->pop();
						p = element->getObject();

						if (mCurrent->full())
						{
							mFull.push(mCurrent);
							mCurrent = NULL;
						}
					}
					return p;
				}

				s32						deallocate(void* ptr)
				{
					if (mCurrent!=NULL && mCurrent->compare_ptr(ptr)==0)
					{
						mCurrent->push((xelement*)ptr);
						if (mCurrent->full())
						{
							mFree.push(mCurrent);
							mCurrent = NULL;
						}
						return 1;
					}
					else
					{
						xblock* block = mFree.find(ptr);
						if (block == NULL)
						{
							block = mFull.find(ptr);
							if (block != NULL)
							{
								block->push((xelement*)ptr);

								// Swap this block from Full to Free
								mFull.remove(block);
								mFree.push(block);
							}
						}
						else
						{
							block->push((xelement*)ptr);
						}
						return block!=NULL ? 1 : 0;
					}
				}

				void					release(x_iallocator* allocator)
				{
					mFree.release(allocator);
					mFull.release(allocator);

					if (mCurrent!=NULL)
					{
						mCurrent->release(allocator);
						mCurrent = NULL;
					}
				}

				xblocktree				mFree;
				xblocktree				mFull;
				xblock*					mCurrent;						///< The block where we allocate from (does not belong to any tree yet)
				u32						mSize;							///< The number of blocks
			};

		protected:
			///< Grows memory pool by blockSize * blockCount
			void					extend (Blocks& blocks, u32 inBlockCount, u32 inBlockMaxCount) const;
			virtual void			release ();

		protected:
			bool					mIsInitialized;
			x_iallocator*			mAllocator;

			Blocks					mStaticBlocks;						///< These are the initial blocks
			Blocks					mDynamicBlocks;						///< These are the blocks that are create/destroyed during runtime

			// Save initial parameters
			u32						mElemSize;
			u32						mElemAlignment;
			u32 					mBlockElemCount;
			u32 					mBlockInitialCount;
			u32 					mBlockGrowthCount;
			u32 					mBlockMaxCount;

			// Helper members
			s32						mUsedItems;

		private:
			// Copy construction and assignment are forbidden
									xallocator_imp(const xallocator_imp&);
			xallocator_imp&			operator= (const xallocator_imp&);
		};

		xallocator_imp::xallocator_imp()
			: mIsInitialized(false)
			, mAllocator(NULL)
			, mElemSize(4)
			, mElemAlignment(X_ALIGNMENT_DEFAULT)
			, mBlockElemCount(0)
			, mBlockInitialCount(0)
			, mBlockGrowthCount(0)
			, mUsedItems(0)
		{
		}

		xallocator_imp::xallocator_imp(x_iallocator* allocator, u32 inElemSize, u32 inBlockElemCnt, u32 inBlockInitialCount, u32 inBlockGrowthCount, u32 inBlockMaxCount, u32 inElemAlignment)
			: mIsInitialized(false)
			, mAllocator(allocator)
			, mElemSize(inElemSize)
			, mElemAlignment(inElemAlignment)
			, mBlockElemCount(inBlockElemCnt)
			, mBlockInitialCount(inBlockInitialCount)
			, mBlockGrowthCount(inBlockGrowthCount)
			, mBlockMaxCount(inBlockMaxCount)
			, mUsedItems(0)
		{
			init();
		}

		xallocator_imp::~xallocator_imp ()
		{
			ASSERT(mUsedItems==0);
		}

		void xallocator_imp::init()
		{
			// Check if memory pool is not already initialized
			ASSERT(mIsInitialized == false);

			// Check input parameters	
			ASSERT(mElemSize >= 4);
			ASSERT(mBlockElemCount > 0);
			ASSERT(mBlockInitialCount > 0);

			// Save initial parameters
			mElemAlignment     = mElemAlignment==0 ? X_ALIGNMENT_DEFAULT : mElemAlignment;
			mElemSize          = x_intu::alignUp(mElemSize, mElemAlignment);			// Align element size to a multiple of element alignment

			extend(mStaticBlocks, mBlockInitialCount, mBlockMaxCount);
		}

		void		xallocator_imp::reset(xbool inRestoreToInitialSize)
		{
			// Check if pool is empty
			ASSERT(mUsedItems == 0);

			if (inRestoreToInitialSize)
			{
				mDynamicBlocks.release(mAllocator);
				mStaticBlocks.reset(mBlockElemCount, mElemSize);
			}

			mUsedItems = 0;
		}

		void*		xallocator_imp::allocate(xsize_t size, u32 alignment)
		{
			ASSERT((u32)size <= mElemSize);

			void* p = mStaticBlocks.allocate();
			if (p == NULL)
			{
				p = mDynamicBlocks.allocate();
				if (p == NULL)
				{
					extend(mDynamicBlocks, mBlockGrowthCount, mBlockMaxCount - mStaticBlocks.size());
					p = mDynamicBlocks.allocate();
				}
			}
			mUsedItems += p!=NULL ? 1 : 0;
			return p;
		}

		void*		xallocator_imp::reallocate(void* inObject, xsize_t size, u32 alignment)
		{
			ASSERT((u32)size <= mElemSize);
			ASSERT(alignment <= mElemAlignment);

			//TODO: We could reallocate from DynamicBlocks to StaticBlocks. That is if
			//      the incoming object comes from DynamicBlocks, if so and we can
			//      allocate from StaticBlocks we could reallocate it.

			return inObject;
		}

		void		xallocator_imp::deallocate(void* inObject)
		{
			// Check input parameters
			if (inObject == NULL)
				return;

			// First check the StaticBlocks
			s32 c = mStaticBlocks.deallocate(inObject);
			if (c==0)
				c = mDynamicBlocks.deallocate(inObject);
			
			ASSERTS(c==1, "Error: did not find that address in this pool allocator, are you sure you are deallocating with the right allocator?");
			mUsedItems -= c;
		}

		void		xallocator_imp::extend(Blocks& blocks, u32 inBlockCount, u32 inBlockMaxCount) const
		{
			// In case of fixed pool it is legal to call extend with blockCount = 0
			if (inBlockCount == 0)
				return;

			for (u32 i=0; i<inBlockCount; ++i)
			{
				if (blocks.size() >= inBlockMaxCount)
					return;
				xblock* new_block = xblock::create(mAllocator, mElemSize, mBlockElemCount, mElemAlignment);
				blocks.mFree.push(new_block);
				blocks.mSize += 1;
			}
		}

		void	xallocator_imp::release()
		{
			ASSERT(mUsedItems == 0);

			mDynamicBlocks.release(mAllocator);
			mStaticBlocks.release(mAllocator);

			mAllocator->deallocate(this);
		}

	}	// End of xpool_allocator namespace

	x_iallocator*		gCreatePoolAllocator(x_iallocator* allocator, xpool_params const& params)
	{
		void* mem = allocator->allocate(sizeof(xpool_allocator::xallocator_imp), X_ALIGNMENT_DEFAULT);
		xpool_allocator::xallocator_imp* pool_allocator = new (mem) xpool_allocator::xallocator_imp(allocator, params.get_elem_size(), params.get_block_size(), params.get_block_initial_count(), params.get_block_growth_count(), params.get_block_max_count(), params.get_elem_alignment());
		pool_allocator->init();
		return pool_allocator;
	}

};
