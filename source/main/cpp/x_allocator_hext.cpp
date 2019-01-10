#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_memory_std.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"
#include "xbase/x_idx_allocator.h"
#include "xbase/x_integer.h"
#include "xbase/x_tree.h"

#include "xallocator/x_allocator_hext.h"


namespace xcore
{
	namespace Allocator_HEXT
	{
		/*	Example: 2 GB

		Address Table
		- Address space = 2 GB
		- Address space granularity = 1 KB
		- Num Entries = 2 MB
		- Entry is NodeIdx, 4 bytes
		- Total memory is 8 MB

		Size Table
		- MinSize = 1 KB
		- MaxSize = 32 MB
		- Granularity = 1 KB
		- Num Entries = 32 K
		- Entry is NodeIdx, 4 bytes
		- Size-Array is 32K * 4 = 128 KB
		- Bit tree = 8/64/512/4096/32768 = 5 KB
		- Total Memory = 128 KB + 5 KB = 133 KB

		Every allocated/free block uses:
		- 2 LNode
		- 1 TNode
		- 2 * 8 + 1 * 8 = 3 * 8 = 24 bytes

		Behavior:

		- Allocate
		  Will do a search for the appropriate size, when found it could split the size of the found node
		  that is then added back to the size-array/size-tree. The node for the allocation is then added
		  to the address-array.

		- Deallocate
		  Address is searched in the address-array, when found it will go through a coallesce procedure to 
		  merge with @prev and @next blocks if they are 'free'. The final free node will be added back to the
		  size-array/size-tree. Merged nodes will be removed from the size-array/size-tree and the address-list.

		- Remove/Add to Size-Array/Size-Tree
		  This is a simple traversal of the bit-tree after which a remove/add to a linked-list will be executed

		- Remove/Add to Address-Array
		  This is a direct reset/set into an array


		1KB - 8KB - 64KB - 512KB - 4MB - 32MB - 256MB - 2GB

		*/

		struct NodeIdx
		{
			const static u32 cNullNode = 0xFFFFFFFF;

			inline			NodeIdx() : mIndex(cNullNode) {}
			inline			NodeIdx(u32 index) : mIndex(index) {}
			inline			NodeIdx(const NodeIdx& c) : mIndex(c.mIndex) {}

			void			Init()					{ mIndex = cNullNode; }
			bool			IsNull() const			{ return mIndex == cNullNode; }

			inline void		SetIndex(u32 index)		{ mIndex = index; }
			inline u32		GetIndex() const		{ return mIndex; }

			inline bool		operator == (const NodeIdx& other) const { return mIndex == other.mIndex; }
			inline bool		operator != (const NodeIdx& other) const { return mIndex != other.mIndex; }

		private:
			u32				mIndex;
		};

		static inline void*		ToVoidPtr(void* base, u32 offset)
		{
			return (u8*)base + offset;
		}

		template<typename T>
		static inline T*		AllocateAndClear(xalloc* allocator, s32 count)
		{
			T* ptr = (T*)allocator->allocate(count * sizeof(T), sizeof(void*));
			while (--count >= 0)
				ptr[count].Init();
			return ptr;
		}

		static inline u32		Clamp(u32 value, u32 cmin, u32 cmax)
		{
			if (value < cmin) return cmin;
			if (value > cmax) return cmax;
			return value;
		}

		static inline u32		AlignDown(u32 value, u32 align)
		{
			return (value) & ~(align - 1);
		}

		static inline u32		AlignUp(u32 value, u32 align)
		{
			return (value + (align - 1)) & ~(align - 1);
		}

		static inline u32		PowerOf2Up(u32 value)
		{
			u32 t = value - 1;
			t |= t >> 1; t |= t >> 2; t |= t >> 4; t |= t >> 8; t |= t >> 16;
			t++;
			return (value == t) ? (value) : (t);
		}

		struct SizeRange
		{
			inline u32		Set(u32 min_range, u32 max_range, u32 divisor)
			{
				mDivisor = divisor;
				mLevels = 1;
				u32 range = min_range;
				do {
					mLevels += 1;
					range = range * divisor;
				} while (range < max_range);
				mMinRange = min_range;
				mMaxRange = range;
				return mLevels;
			}

			inline u32		LevelSize(u32 level) const
			{
				u32 const level_size = (((1 << (3 * level)) + (8 - 1)) >> 3) << 3;
				return level_size;
			}

			u32	mDivisor;
			u32 mLevels;
			u32	mMinRange;
			u32	mMaxRange;
		};


		struct MNode;
		struct TNode;
		struct LNode;

		// 2 * 4 = 8 bytes
		struct LNode
		{
			inline void		Init()
			{
				mNext.Init();
				mPrev.Init();
			}

			inline void		InsertAfter(NodeIdx self, LNode* noden, NodeIdx node, LNode* nextn, NodeIdx next)
			{
				noden->mNext = mNext;
				noden->mPrev = self;
				mNext = node;
				if (nextn != NULL) nextn->mPrev = node;
			}

			NodeIdx			mNext, mPrev;
		};


		// TNode is a leaf entity of the SizeNodes and AddrNodes
		struct TNode
		{
			inline void		Init()						{ mAddr = 0; mSize = 0; }

			void			SetFree()					{ mSize = (mSize & ~USED); }
			void			SetUsed()					{ mSize = mSize | USED; }

			bool			IsUsed() const				{ return (mSize & USED) == USED; }
			bool			IsFree() const				{ return (mSize & USED) == 0; }

			u32				GetAddr() const				{ return (mAddr); }
			void			SetAddr(u32 addr)			{ mAddr = addr; }
			void*			GetPtr(void* ptr) const		{ return (u8*)ptr + mAddr; }

			void			SetSize(u32 size)			{ mSize = (mSize & FMASK) | ((size >> 4) & ~FMASK); }
			u32				GetSize() const				{ return (mSize & ~FMASK) << 4; }

			bool			HasSizeLeft(u32 size) const	{ return size < GetSize(); }

		private:
			enum EFlags
			{
				USED = 0x80000000,
				FMASK = 0xF0000000,
			};
			u32		mAddr;					// ADDRESS
			u32		mSize;					// SIZE (*cMinimumSize)
		};

		// FNode is a node used in the free-list re-using TNode 
		struct FNode
		{
			NodeIdx			mNext;
		};

		// MNode is an entity of the bit based Size-Tree
		struct MNode
		{
			inline			MNode() : mBits(0) {}

			void			Init()						{ mBits = 0; }

			u8				GetBits() const				{ return mBits; }
			void			SetBits(u8 bits)			{ mBits = bits; }

			bool			GetChildLower(u32 child_index, u32& next_index) const
			{
				// In the child occupancy find an existing child at a lower position then @child_index.
				// Return the existing child-index in @existing_index and return 0 or 1. 
				// Return 0 if @child_index == @existing_index meaning that there is a child at @child_index.
				// Return 1 if at the current @child_index does not have a valid child but we did find an existing child
				//          at a lower index.
				// Return -1 if the child occupancy is empty.
				if (mBits == 0 || child_index == 0)
					return false;

				u32 occupance = mBits & ((1 << child_index) - 1);
				if (occupance == 0)
					return false;
				u32 existing = (occupance ^ (occupance & (occupance - 1)));

				next_index = 0;
				if ((existing & 0x0F) == 0) { next_index += 4;  existing = existing >> 4; }
				if ((existing & 0x03) == 0) { next_index += 2;  existing = existing >> 2; }
				if ((existing & 0x01) == 0) { next_index += 1; }
				return true;
			}

			bool			GetChildLowest(u32& next_index) const
			{
				return GetChildLower(8, next_index);
			}

			bool			GetChildHigher(u32 child_index, u32& next_index) const
			{
				// In the child occupancy find an existing child at the current or higher position then @child_index.
				// Return the existing child-index in @existing_index and return 0 or 1. 
				// Return 0 if @child_index == @existing_index meaning that there is a child at @child_index.
				// Return 1 if at the current @child_index does not have a valid child but we did find an existing child
				//          at a higher index.
				// Return -1 if the child occupancy is empty.
				if (mBits == 0 || child_index == 7)
					return false;

				u32 occupance = mBits & (0xFE << child_index);
				if (occupance == 0)
					return false;
				u32 existing = (occupance ^ (occupance & (occupance - 1)));

				next_index = 0;
				if ((existing & 0x0F) == 0) { next_index += 4;  existing = existing >> 4; }
				if ((existing & 0x03) == 0) { next_index += 2;  existing = existing >> 2; }
				if ((existing & 0x01) == 0) { next_index += 1; }
				return true;
			}

			void			SetChildUsed(u32 index) { u8 const bit = (u8)(1 << index); mBits = mBits | bit; }
			void			SetChildEmpty(u32 index) { u8 const bit = (u8)(1 << index); mBits = mBits & ~bit; }
			bool			GetChildUsed(u32 index) const { u8 const bit = (u8)(1 << index); return (mBits & bit) == bit; }

		private:
			u8				mBits;
		};

		template<>
		static inline MNode**	AllocateAndClear<MNode*>(xalloc* allocator, s32 count)
		{
			MNode** ptr = (MNode**)allocator->allocate(count * sizeof(MNode*), sizeof(void*));
			while (--count >= 0)
				ptr[count] = NULL;
			return ptr;
		}

		class Allocator : public xalloc
		{
		public:
			const char*		name() const;									///< The name of the allocator

			void*			allocate(xsize_t size, u32 align);				///< Allocate memory with alignment
			void*			reallocate(void* p, xsize_t size, u32 align);	///< Reallocate memory
			void			deallocate(void* p);							///< Deallocate/Free memory

			void			release();										///< Release/Destruct this allocator

			void			Initialize(xalloc* allocator, void* mem_base, u32 mem_size, u32 min_alloc_size, u32 max_alloc_size);
			void			Destroy();

			void*			Allocate(u32 size);
			void			Deallocate(void*);

		private:
			NodeIdx			FindSize(u32 size) const;
			NodeIdx			FindAddr(u32 addr) const;

			void			AddToAddr(NodeIdx node, u32 address);
			void			AddToSize(NodeIdx node, u32 size);

			void			RemoveFromAddr(NodeIdx node, u32 addr);
			void			RemoveFromAddrList(NodeIdx node);
			void			RemoveFromSize(NodeIdx node, u32 size);

			s32				Coallesce(NodeIdx& n, TNode*& curr_tnode);

			void*			mMemBase;
			u32				mMemSize;

			u32				mMinAllocSize;
			u32				mMaxAllocSize;

			void			SetSizeUsed(u32 index, u32 level);
			void			SetSizeEmpty(u32 index, u32 level);

			void			AllocNode(TNode*& p, NodeIdx& i);
			void			DeallocNode(TNode* tnode, NodeIdx i);

			TNode*			IndexToTNode(NodeIdx i) const;
			LNode*			IndexToLNode_Size(NodeIdx i) const;
			LNode*			IndexToLNode_Addr(NodeIdx i) const;
			FNode*			IndexToFNode(NodeIdx i) const;

			const static s32	cMaximumBlocks = 256;
			const static s32	cNodesPerBlock = 512;

			xalloc*	mAllocator;

			NodeIdx*		mAddrNodes;

			NodeIdx*		mSizeNodes;
			SizeRange		mSizeRange;
			MNode**			mSizeTree;

			// TNode[]
			NodeIdx			mTNode_FreeList;
			u32				mNumAllocatedTNodeBlocks;
			TNode*			mAllocatedTNodes[cMaximumBlocks];
			LNode*			mAllocatedLNodes_Size[cMaximumBlocks];
			LNode*			mAllocatedLNodes_Addr[cMaximumBlocks];
		};

		const char*		Allocator::name() const
		{
			return "h-ext allocator";
		}

		void*			Allocator::allocate(xsize_t size, u32 align)
		{
			size = (size + (mMinAllocSize - 1)) & ~(mMinAllocSize - 1);
			return Allocate(size);
		}

		void*			Allocator::reallocate(void* p, xsize_t size, u32 align)
		{
			return NULL;
		}

		void			Allocator::deallocate(void* p)
		{
			Deallocate(p);
		}

		void			Allocator::release()
		{
			Destroy();
		}


		NodeIdx		Allocator::FindSize(u32 size) const
		{
			if (size > (mSizeRange.mMaxRange - mSizeRange.mMinRange))
				return NodeIdx();

			u32 index = size / mSizeRange.mMinRange;
			NodeIdx current = mSizeNodes[index];
			TNode* tnode = IndexToTNode(current);

			// So can this TNode fullfill the size request ?
			if (tnode != NULL)
			{
				if (size == tnode->GetSize())
				{
					ASSERT(tnode->IsFree());
					return current;
				}
			}

			// Reset current to invalid 
			current = NodeIdx();

			// Ok, so here we did not find an exact fit, go and find a best fit
			// Move up the tree to find a branch that holds a larger size.
			u32 child_index = index & 0x7;
			u32 level = mSizeRange.mLevels - 1;
			u32 node_index = index >> 3;
			MNode* mnode = &mSizeTree[level][node_index];

			enum ETraverseDir { UP = -1, DOWN = 1 };
			ETraverseDir traverse_dir = UP;
			u32 next_index;
			while (true)
			{
				if (traverse_dir == UP)
				{
					if (mnode->GetChildHigher(child_index, next_index))
					{
						// We found a next size and now we should traverse down to base-level.
						child_index = next_index;
						index = (index & ~0x7) | next_index;
						traverse_dir = DOWN;
					}
					else
					{
						if (level == 1)
							break;
						level -= 1;
						index = index / 8;

						child_index = index & 0x7;
						node_index = index >> 3;
						mnode = &mSizeTree[level][node_index];
					}
				}
				else if (traverse_dir == DOWN)
				{
					if ((level+1) == mSizeRange.mLevels)
					{
						// So do we have an existing TNode here ?
						node_index = index;
						current = mSizeNodes[node_index];
						break;
					}

					level += 1;
					index = index * 8;

					node_index = index >> 3;
					mnode = &mSizeTree[level][node_index];

					// When traversing down we need to traverse to the most minimum existing leaf.
					// So on every level pick the minimum existing child index and traverse down
					// into that child.
					if (mnode->GetChildLowest(child_index) == false)
						break;

					index = index | child_index;
				}
			}

			tnode = IndexToTNode(current);
			return current;
		}

		NodeIdx		Allocator::FindAddr(u32 address) const
		{
			u32 const index = address / mMinAllocSize;
			NodeIdx current = mAddrNodes[index];
			return current;
		}

		void		Allocator::AddToAddr(NodeIdx node, u32 address)
		{
			ASSERT(IndexToTNode(node)->GetAddr() == address);

			// The TNode is already linked into the address-list, so we do not have to search an existing child.
			u32 const index = address / mMinAllocSize;
			NodeIdx head = mAddrNodes[index];

			if (head.IsNull())
			{
				// So the location where we should put this entry is not used yet
				mAddrNodes[index] = node;
			}
			else
			{
				TNode* tnode = IndexToTNode(head);
				ASSERT(head.IsNull());
			}
		}

		void		Allocator::AddToSize(NodeIdx node, u32 size)
		{
			ASSERT(size > 0);
			ASSERT(IndexToTNode(node)->GetSize() == size);

			size = Clamp(size, mSizeRange.mMinRange, mSizeRange.mMaxRange - mSizeRange.mMinRange);
			u32 const index = size / mSizeRange.mMinRange;
			NodeIdx head = mSizeNodes[index];

			// So we found either a TNode as a child or Null
			if (head.IsNull())
			{
				// No existing nodes here yet, just set it
				mSizeNodes[index] = node;
				SetSizeUsed(index, mSizeRange.mLevels);
			}
			else
			{	// We have an existing node here, so we have to add it to the list
				// We do not have to update the Size-Tree
				LNode* head_lnode = IndexToLNode_Size(head);
				LNode* node_lnode = IndexToLNode_Size(node);
				LNode* next_lnode = IndexToLNode_Size(head_lnode->mNext);
				head_lnode->InsertAfter(head, node_lnode, node, next_lnode, head_lnode->mNext);
			}
		}

		void	Allocator::RemoveFromAddrList(NodeIdx node)
		{
			NodeIdx head;
			LNode* lnode = IndexToLNode_Addr(node);
			LNode* prevn = IndexToLNode_Addr(lnode->mPrev);
			LNode* nextn = IndexToLNode_Addr(lnode->mNext);
			if (prevn != NULL) prevn->mNext = lnode->mNext;
			if (nextn != NULL) nextn->mPrev = lnode->mPrev;
			lnode->Init();
		}

		void	Allocator::RemoveFromAddr(NodeIdx node, u32 address)
		{
			ASSERT(IndexToTNode(node)->GetAddr() == address);

			u32 const index = address / mMinAllocSize;
			NodeIdx current = mAddrNodes[index];

			if (current == node)
			{
				mAddrNodes[index] = NodeIdx();
			}
			else
			{
				ASSERT(current == node);
			}
		}

		void	Allocator::RemoveFromSize(NodeIdx node, u32 size)
		{
			ASSERT(IndexToTNode(node)->GetSize() == size);
			size = Clamp(size, mSizeRange.mMinRange, mSizeRange.mMaxRange - mSizeRange.mMinRange);

			u32 const index = size / mSizeRange.mMinRange;
			NodeIdx head = mSizeNodes[index];

			if (head.IsNull() == false)
			{
				NodeIdx current = head;
				while (current != node && !current.IsNull())
				{
					LNode* lnode = IndexToLNode_Size(current);
					current = lnode->mNext;
				};
				
				if (current == node)
				{
					LNode* lnode = IndexToLNode_Size(current);
					LNode* prevn = IndexToLNode_Size(lnode->mPrev);
					LNode* nextn = IndexToLNode_Size(lnode->mNext);
					if (prevn != NULL) prevn->mNext = lnode->mNext;
					if (nextn != NULL) nextn->mPrev = lnode->mPrev;
					if (head == current) head = lnode->mNext;
					lnode->Init();

					mSizeNodes[index] = head;
					if (head.IsNull())
					{
						SetSizeEmpty(index, mSizeRange.mLevels);
					}
				}
				else
				{
					ASSERT(!current.IsNull());
				}
			}
			else
			{
				ASSERT(!head.IsNull());
			}
		}

		void	Allocator::Initialize(xalloc* allocator, void* mem_base, u32 mem_size, u32 min_alloc_size, u32 max_alloc_size)
		{
			mMinAllocSize = PowerOf2Up(Clamp(min_alloc_size, 1024, 64 * 1024));
			mMaxAllocSize = AlignUp(max_alloc_size, mMinAllocSize);

			mMemBase = mem_base;
			mMemSize = AlignDown(mem_size, mMinAllocSize);

			// Initialize size and nodes
			mAllocator = allocator;

			const u32 addr_cnt = mMemSize / mMinAllocSize;
			mAddrNodes = AllocateAndClear<NodeIdx>(allocator, addr_cnt);

			u32 const sizetree_levels = mSizeRange.Set(min_alloc_size, max_alloc_size, 8);
			const u32 size_cnt = mSizeRange.mMaxRange / mSizeRange.mMinRange;
			mSizeNodes = AllocateAndClear<NodeIdx>(allocator, size_cnt);

			mSizeTree = AllocateAndClear<MNode*>(allocator, sizetree_levels);
			u32 sizetree_size = 0;
			for (u32 i = 0; i < sizetree_levels; ++i)
			{
				sizetree_size += mSizeRange.LevelSize(i);
			}
			mSizeTree[0] = AllocateAndClear<MNode>(allocator, sizetree_size >> 3);
			for (u32 i = 1; i < sizetree_levels; ++i)
			{
				u32 numbytes = mSizeRange.LevelSize(i - 1) >> 3;
				mSizeTree[i] = mSizeTree[i - 1] + numbytes;
			}

			mTNode_FreeList = NodeIdx();
			mNumAllocatedTNodeBlocks = 0;
			for (u32 i = 0; i < cMaximumBlocks; ++i)
			{
				mAllocatedTNodes[i] = NULL;
				mAllocatedLNodes_Size[i] = NULL;
				mAllocatedLNodes_Addr[i] = NULL;
			}

			// Create initial node that holds all free memory
			TNode* tnode;
			NodeIdx tnode_index;
			AllocNode(tnode, tnode_index);
			tnode->SetAddr(0);
			tnode->SetSize(mMemSize);
			tnode->SetFree();
			mMemSize = tnode->GetSize();
			AddToSize(tnode_index, mMemSize);
		}

		void	Allocator::Destroy()
		{
			mAllocator->deallocate(mAddrNodes);
			mAllocator->deallocate(mSizeNodes);

			mAllocator->deallocate(mSizeTree[0]);
			mAllocator->deallocate(mSizeTree);

			for (u32 i = 0; i < mNumAllocatedTNodeBlocks; ++i)
			{
				mAllocator->deallocate(mAllocatedTNodes[i]);
				mAllocator->deallocate(mAllocatedLNodes_Size[i]);
				mAllocator->deallocate(mAllocatedLNodes_Addr[i]);
			}
		}

		void*	Allocator::Allocate(u32 size)
		{
			NodeIdx curr = FindSize(size);
			if (curr.IsNull() == false)
			{
				TNode* curr_tnode = IndexToTNode(curr);
				if (!FindAddr(curr_tnode->GetAddr()).IsNull())
				{
					curr_tnode = IndexToTNode(curr);
				}

				// Split of the size that we need if anything left above 
				// the minimum alloc size add it back to @size.
				if (size <= curr_tnode->GetSize())
				{
					RemoveFromSize(curr, curr_tnode->GetSize());
					if (curr_tnode->HasSizeLeft(size))
					{
						NodeIdx new_nodeidx;
						TNode* new_tnode;
						AllocNode(new_tnode, new_nodeidx);
						new_tnode->SetAddr(curr_tnode->GetAddr() + size);
						new_tnode->SetSize(curr_tnode->GetSize() - size);
						new_tnode->SetFree();

						LNode* new_lnode = IndexToLNode_Addr(new_nodeidx);
						LNode* curr_lnode = IndexToLNode_Addr(curr);
						LNode* next_lnode = IndexToLNode_Addr(curr_lnode->mNext);
						curr_lnode->InsertAfter(curr, new_lnode, new_nodeidx, next_lnode, curr_lnode->mNext);

						curr_tnode->SetSize(size);
						AddToSize(new_nodeidx, new_tnode->GetSize());
					}
					curr_tnode->SetUsed();
					AddToAddr(curr, curr_tnode->GetAddr());
					return curr_tnode->GetPtr(mMemBase);
				}
			}
			return NULL;
		}

		void	Allocator::Deallocate(void* p)
		{
			u32 address = (u32)((u8*)p - (u8*)mMemBase);
			NodeIdx curr = FindAddr(address);
			TNode* curr_tnode = IndexToTNode(curr);
			ASSERT(curr_tnode->IsUsed());
			curr_tnode->SetFree();
			RemoveFromAddr(curr, address);
			Coallesce(curr, curr_tnode);
			AddToSize(curr, curr_tnode->GetSize());
		}

		// return: 0 = nothing done
		//         1 = merged with @next
		//         2 = merged with @prev
		//         3 = merged with @next & @prev
		s32	Allocator::Coallesce(NodeIdx& curr, TNode*& curr_tnode)
		{
			LNode* curr_lnode = IndexToLNode_Addr(curr);
			NodeIdx curr_next = curr_lnode->mNext;
			NodeIdx curr_prev = curr_lnode->mPrev;

			s32 c = 0;
			NodeIdx fake_list_head;

			// Is @next address free?
			//   Remove @next from @address 
			//   Remove @next from @size 
			//   Update size of @curr
			//   Deallocate @next
			TNode* next_tnode = IndexToTNode(curr_next);
			if (next_tnode != NULL && next_tnode->IsFree())
			{
				ASSERT(next_tnode->GetSize() > 0);
				// Ok, @next is going to disappear and merged into current
				curr_tnode->SetSize(curr_tnode->GetSize() + next_tnode->GetSize());
				RemoveFromSize(curr_next, next_tnode->GetSize());
				RemoveFromAddrList(curr_next);
				DeallocNode(next_tnode, curr_next);
				c |= 1;
			}

			// Is @prev free?
			//   Remove @curr from @address 
			//   Remove @prev from @size 
			//   Update size of @prev
			//   Deallocate @curr
			//   Add @prev to @size 
			//   @curr = @prev
			TNode* prev_tnode = IndexToTNode(curr_prev);
			if (prev_tnode != NULL && prev_tnode->IsFree())
			{
				ASSERT(prev_tnode->GetSize() > 0);
				// Ok, @curr is going to disappear and be merged into prev
				RemoveFromSize(curr_prev, prev_tnode->GetSize());
				prev_tnode->SetSize(curr_tnode->GetSize() + prev_tnode->GetSize());
				RemoveFromAddrList(curr);
				DeallocNode(curr_tnode, curr);
				curr = curr_prev;
				curr_tnode = prev_tnode;
				c |= 2;
			}

			return c;
		}


		void			Allocator::SetSizeUsed(u32 index, u32 levels)
		{
			u8 bits;
			do
			{
				levels -= 1;
				u32 ei = index >> 3;
				MNode* mnode = &mSizeTree[levels][ei];
				u32 bi = index & 0x7;
				bits = mnode->GetBits();
				mnode->SetChildUsed(bi);
				if (levels == 1)
					break;
				index >>= 3;
			} while (bits == 0);
		}

		void			Allocator::SetSizeEmpty(u32 index, u32 levels)
		{
			u8 bits;
			do
			{
				levels -= 1;
				u32 ei = index >> 3;
				MNode* mnode = &mSizeTree[levels][ei];
				u32 bi = index & 0x7;
				mnode->SetChildEmpty(bi);
				if (levels == 1)
					break;
				bits = mnode->GetBits();
				index >>= 3;
			} while (bits == 0);
		}

		TNode*			Allocator::IndexToTNode(NodeIdx i) const
		{
			if (i.IsNull())
				return NULL;
			u32 const bi = (i.GetIndex() >> 16) & 0xFFFF;
			u32 const ei = i.GetIndex() & 0xFFFF;
			return (TNode*)&mAllocatedTNodes[bi][ei];
		}

		LNode*			Allocator::IndexToLNode_Size(NodeIdx i) const
		{
			if (i.IsNull())
				return NULL;
			u32 const bi = (i.GetIndex() >> 16) & 0xFFFF;
			u32 const ei = i.GetIndex() & 0xFFFF;
			return (LNode*)&mAllocatedLNodes_Size[bi][ei];
		}
		LNode*			Allocator::IndexToLNode_Addr(NodeIdx i) const
		{
			if (i.IsNull())
				return NULL;
			u32 const bi = (i.GetIndex() >> 16) & 0xFFFF;
			u32 const ei = i.GetIndex() & 0xFFFF;
			return (LNode*)&mAllocatedLNodes_Addr[bi][ei];
		}

		FNode*			Allocator::IndexToFNode(NodeIdx i) const
		{
			if (i.IsNull())
				return NULL;
			u32 const bi = (i.GetIndex() >> 16) & 0xFFFF;
			u32 const ei = i.GetIndex() & 0xFFFF;
			return (FNode*)&mAllocatedTNodes[bi][ei];
		}

		void			Allocator::AllocNode(TNode*& p, NodeIdx& i)
		{
			if (mTNode_FreeList.IsNull())
			{
				mAllocatedTNodes[mNumAllocatedTNodeBlocks] = AllocateAndClear<TNode>(mAllocator, cNodesPerBlock);
				mAllocatedLNodes_Size[mNumAllocatedTNodeBlocks] = AllocateAndClear<LNode>(mAllocator, cNodesPerBlock);
				mAllocatedLNodes_Addr[mNumAllocatedTNodeBlocks] = AllocateAndClear<LNode>(mAllocator, cNodesPerBlock);
				const u32 block_index = (mNumAllocatedTNodeBlocks << 16);
				for (u32 i = 0; i < cNodesPerBlock; ++i)
				{
					NodeIdx ni(block_index | i);
					TNode* tnode = IndexToTNode(ni);
					DeallocNode(tnode, ni);
				}
				mNumAllocatedTNodeBlocks += 1;
			}

			i = mTNode_FreeList;
			FNode* fnode = IndexToFNode(i);
			mTNode_FreeList = fnode->mNext;
			p = (TNode*)fnode;
			p->Init();
		}

		void			Allocator::DeallocNode(TNode* tnode, NodeIdx i)
		{
			tnode->Init();
			LNode* lnode_size = IndexToLNode_Size(i);
			lnode_size->Init();
			LNode* lnode_addr = IndexToLNode_Addr(i);
			lnode_addr->Init();
			
			FNode* fnode = IndexToFNode(i);
			ASSERT((void*)fnode == (void*)tnode);
			fnode->mNext = mTNode_FreeList;
			mTNode_FreeList = i;
		}

	};

	xalloc*	gCreateHextAllocator(xalloc* allocator, void* mem_base, u32 mem_size, u32 min_alloc_size, u32 max_alloc_size)
	{
		Allocator_HEXT::Allocator* hext = new Allocator_HEXT::Allocator();
		hext->Initialize(allocator, mem_base, mem_size, min_alloc_size, max_alloc_size);
		return hext;
	}
}



namespace MediumSizedAllocator
{
	// Range = 256 GB
	// 1. Min Size = 4 KB / Max Size = 128 KB
	// 2. Min Size = 128 KB / Max Size = 1 MB

	// 256 GB / 4 KB = 64 MB (26 bits)


	struct HSizeTree
	{
		void		add_size(u32)
	};


	// 2 MB
	struct HPage
	{
		u16			m_refcount;		// We need to know when we can 'release' this page
	};

	// 32 MB
	struct HBlock
	{
		void*		m_baseptr;
		HPage		m_pages[16];	// Page = 2 MB
	};

	// 256 GB / 32 MB = 8192 blocks
	struct HRegion
	{
		u16			m_blocks[8192];
	};

}