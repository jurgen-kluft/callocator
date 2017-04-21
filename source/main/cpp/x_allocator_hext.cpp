#include <cstdlib>
#include <stdio.h>
#include <atomic>
#include <assert.h>

class IAllocator
{
public:
	virtual void*			Allocate(uint32_t size) = 0;
	virtual void			Deallocate(void* p) = 0;
};


namespace HEXT_Allocator2
{
	/*	4 GB
	Address Table
	- Address space = 4 GB
	- Address space granularity = 1 KB
	- Num Entries = 4 MB
	- 4 nodes per address location
	- Num Entries, 4 MB / 4 = 1 MB
	- 1 entry is a NodeIdx(4)
	- Total memory is 4 MB

	Size Table
	- MinSize = 1 KB
	- MaxSize = 32 MB
	- Granularity = 1 KB
	- Num Entries = 32 K
	- 1 entry is NodeIdx
	- Bit tree = 8/64/512/4096/32768 = 5 KB
	- Total Memory = 128 + 5 KB
	*/

	const static uint32_t	cNullNode = 0xFFFFFFFF;

	struct NodeIdx
	{
		inline			NodeIdx() : mIndex(cNullNode) {}
		inline			NodeIdx(uint32_t index) : mIndex(index) {}
		inline			NodeIdx(const NodeIdx& c) : mIndex(c.mIndex) {}

		void			Init() { mIndex = cNullNode; }
		bool			IsNull() const { return mIndex == cNullNode; }

		inline void		SetIndex(uint32_t index) { mIndex = index; }
		inline uint32_t	GetIndex() const { return mIndex; }

		inline bool		operator == (const NodeIdx& other) const { return mIndex == other.mIndex; }
		inline bool		operator != (const NodeIdx& other) const { return mIndex != other.mIndex; }

	private:
		uint32_t		mIndex;
	};

	static inline void*		ToVoidPtr(void* base, uint32_t offset)
	{
		return (uint8_t*)base + offset;
	}

	static inline void*		AllocateAndClear(uint32_t size)
	{
		void* ptr = malloc(size);
		memset(ptr, 0, size);
		return ptr;
	}

	template<typename T>
	static inline T*		AllocateAndClear(int32_t count)
	{
		T* ptr = (T*)malloc(count * sizeof(T));
		while (--count >= 0)
			ptr[count].Init();
		return ptr;
	}

	static inline uint32_t	Clamp(uint32_t value, uint32_t cmin, uint32_t cmax)
	{
		if (value < cmin) return cmin;
		if (value > cmax) return cmax;
		return value;
	}

	struct Range
	{
		/*
		Range with a divisor of 8 creates 8 sub-ranges

		from_____________________________________________________ to
		|      |      |      |      |      |      |      |      |
		---------------------------------------------------------
		Example
		Range = 0 - 64 MB, Divisor = 8
		Sub-Ranges = 0:0-8, 1:8-16, 2:16-24, 3:24-32, 4:32-40, 5:40-48, 6:48-56, 7:56-64
		value:34 => index:4 => from:32 - to:40
		*/

		// E.g: 
		//      Range r;
		//      r.Set(32 * _MB, 1 * _KB, 8);
		//
		void		Set(uint32_t min_range, uint32_t max_range, uint32_t divisor, uint32_t root_level)
		{
			mMaxRange = max_range;
			mMinRange = min_range;
			mLevel = root_level;
			mBaseLevel = 0;
			mRootLevel = root_level;

			uint32_t range = max_range;
			do {
				mBaseLevel += 1;
				range = range / divisor;
			} while (range > min_range);

			mRange[0] = 0;
			mRange[1] = max_range;
			mDivisor = divisor;
		}

		void		Set(uint32_t min_range, uint32_t max_range, uint32_t divisor)
		{
			Set(min_range, max_range, divisor, 0);
		}

		uint32_t	AlignValue(uint32_t value) const
		{
			value = (value + (mMinRange - 1)) & ~(mMinRange - 1);
			return value;
		}

		bool		IsRootLevel() const { return mLevel == mRootLevel; }
		bool		IsBaseLevel() const { return mLevel == mBaseLevel; }
		uint32_t	GetLevel() const { return mLevel; }
		uint32_t	GetRootLevel() const { return mRootLevel; }
		uint32_t	GetBaseLevel() const { return mBaseLevel; }
		uint32_t	GetNumLevels() const { return mBaseLevel + 1 - mRootLevel; }

		void		SetChildIndex(uint32_t child_index)
		{
			uint32_t const divisor = 1 << (mLevel * 3);
			uint32_t const subrange = mMaxRange / divisor;
			uint32_t const from = child_index * subrange;
			uint32_t const to = from + subrange;
			mRange[0] = from;
			mRange[1] = to;
		}

		uint32_t	GetChildIndexFromValue(uint32_t value) const
		{
			value = Clamp(value, 0, mMaxRange - mMinRange);
			uint32_t const divisor = 1 << (mLevel * 3);
			uint32_t const subrange = mMaxRange / divisor;
			uint32_t const index = (value / subrange) & (mDivisor - 1);
			return index;
		}

		uint32_t	GetIndexFromValue(uint32_t value) const
		{
			value = Clamp(value, 0, mMaxRange - mMinRange);
			uint32_t const divisor = 1 << (mLevel * 3);
			uint32_t const subrange = mMaxRange / divisor;
			uint32_t const index = value / subrange;
			return index;
		}

		uint32_t	GetIndex() const
		{
			uint32_t const value = mRange[0];
			uint32_t const divisor = 1 << (mLevel * 3);
			uint32_t const subrange = mMaxRange / divisor;
			uint32_t const index = value / subrange;
			return index;
		}

		uint32_t	GetIndexFromValueAtBaseLevel(uint32_t value) const
		{
			value = Clamp(value, 0, mMaxRange - mMinRange);
			uint32_t const divisor = 1 << (mBaseLevel * 3);
			uint32_t const subrange = mMaxRange / divisor;
			uint32_t const index = value / subrange;
			return index;
		}

		uint32_t	GetIndexFromChildIndex(uint32_t index) const
		{
			assert(index >= 0 && index < mDivisor);
			uint32_t const divisor = 1 << (mLevel * 3);
			uint32_t const subrange = mMaxRange / divisor;
			uint32_t const value = mRange[0] + index * subrange;
			return GetIndexFromValue(value);
		}

		bool		Contains(uint32_t value) const
		{
			value = Clamp(value, 0, mMaxRange - mMinRange);
			return value >= mRange[0] && value < mRange[1];
		}

		Range		ToBaseLevel(uint32_t value) const
		{
			value = Clamp(value, 0, mMaxRange - mMinRange);

			// Align value to minimum range
			value = value & ~(mMinRange - 1);
			// Set Range[from,to]
			Range r(*this);
			r.mRange[0] = value;
			r.mRange[1] = value + mMinRange;
			// This is base-level
			r.mLevel = mBaseLevel;
			return r;
		}

		uint32_t	GetLevelSize(uint32_t level) const
		{
			assert(level >= 0 && level <= mBaseLevel);
			uint32_t const size = 1 << (level * 3);
			return size;
		}

		uint32_t	GetBaseLevelSize() const
		{
			return GetLevelSize(mBaseLevel);
		}

		uint32_t	GetTotalSize() const
		{
			uint32_t size = 0;
			for (uint32_t l = mRootLevel; l <= mBaseLevel; ++l)
				size += GetLevelSize(l);
			return size;
		}

		uint32_t	TraverseUp(uint32_t value)
		{
			value = Clamp(value, 0, mMaxRange - mMinRange);
			if (IsRootLevel() == false)
			{
				mLevel--;

				uint32_t const divisor = 1 << (mLevel * 3);
				uint32_t const subrange = mMaxRange / divisor;

				// Align value to sub range
				value = value & ~(subrange - 1);

				mRange[0] = value;
				mRange[1] = value + subrange;
			}
			return GetChildIndexFromValue(value);
		}

		uint32_t	TraverseDown(uint32_t value)
		{
			value = Clamp(value, 0, mMaxRange - mMinRange);
			if (IsBaseLevel() == false)
			{
				mLevel++;

				uint32_t const divisor = 1 << (mLevel * 3);
				uint32_t const subrange = mMaxRange / divisor;

				// Align value to sub range
				value = value & ~(subrange - 1);

				mRange[0] = value;
				mRange[1] = value + subrange;
			}
			return GetChildIndexFromValue(value);
		}

		uint32_t	TraverseDownChild(uint32_t child_index)
		{
			uint32_t const subr = (mRange[1] - mRange[0]) / mDivisor;
			uint32_t const value = mRange[0] + child_index * subr;
			return TraverseDown(value);
		}

		uint32_t	mMaxRange;
		uint32_t	mMinRange;
		uint32_t	mLevel;
		uint32_t	mRootLevel;
		uint32_t	mBaseLevel;
		uint32_t	mDivisor;
		uint32_t	mRange[2];
	};


	struct MNode;
	struct TNode;
	struct LNode;

	struct Nodes
	{
		const static int32_t	cMaximumBlocks = 256;
		const static int32_t	cNodesPerBlock = 512;

		struct FNode
		{
			NodeIdx			mNext;
		};

		Range			mAddrRange;
		NodeIdx*		mAddrNodes;

		NodeIdx*		mSizeNodes;
		Range			mSizeRange;
		MNode**			mSizeTree;

		// TNode[]
		NodeIdx			mTNode_FreeList;
		uint32_t		mNumAllocatedTNodeBlocks;
		TNode*			mAllocatedTNodes[cMaximumBlocks];
		LNode*			mAllocatedLNodes_Size[cMaximumBlocks];
		LNode*			mAllocatedLNodes_Addr[cMaximumBlocks];

		Nodes::Nodes()
			: mNumAllocatedTNodeBlocks(0)
		{
		}

		void			Initialize(uint32_t min_range, uint32_t max_range, uint32_t min_alloc_size, uint32_t max_alloc_size);

		void			SetSizeUsed(Range range, uint32_t value);
		void			SetSizeEmpty(Range range, uint32_t value);

		void			Alloc(TNode*& p, NodeIdx& i);
		void			Dealloc(TNode* tnode, NodeIdx i);

		TNode*			IndexToTNode(NodeIdx i) const;
		LNode*			IndexToLNode_Size(NodeIdx i) const;
		LNode*			IndexToLNode_Addr(NodeIdx i) const;
		FNode*			IndexToFNode(NodeIdx i) const;
	};


	// 2 * 4 = 8 bytes
	struct LNode
	{
		void			Init()
		{
			mNext = cNullNode;
			mPrev = cNullNode;
		}

		void			InsertBefore(Nodes* nodes, NodeIdx self, NodeIdx node)
		{
			LNode* prevn = nodes->IndexToLNode_Size(mPrev);
			LNode* noden = nodes->IndexToLNode_Size(node);
			noden->mNext = self;
			noden->mPrev = mPrev;
			mPrev = node;
			if (prevn != NULL) prevn->mNext = node;
		}

		void			InsertAfter(Nodes* nodes, NodeIdx self, NodeIdx node)
		{
			LNode* nextn = nodes->IndexToLNode_Size(mNext);
			LNode* noden = nodes->IndexToLNode_Size(node);
			noden->mNext = mNext;
			noden->mPrev = self;
			mNext = node;
			if (nextn != NULL) nextn->mPrev = node;
		}

		void			RemoveFrom(Nodes* nodes, NodeIdx self, NodeIdx& head)
		{
			LNode* prevn = nodes->IndexToLNode_Size(mPrev);
			LNode* nextn = nodes->IndexToLNode_Size(mNext);
			if (prevn != NULL) prevn->mNext = mNext;
			if (nextn != NULL) nextn->mPrev = mPrev;
			if (head == self)
				head = mNext;
		}

		NodeIdx			Next() const { return mNext; }
		NodeIdx			Prev() const { return mPrev; }

	private:
		NodeIdx			mNext, mPrev;
	};


	// TNode is a leaf entity of the SizeNodes and AddrNodes
	struct TNode
	{
		void			Init()
		{
			mAddr = 0;
			mSize = 0;
		}

		void			SetFree() { mSize = (mSize & ~USED); }
		void			SetUsed() { mSize = mSize | USED; }
		bool			IsFree() const { return (mSize & USED) == USED; }

		uint32_t		GetAddr() const { return (mAddr); }
		void			SetAddr(uint32_t addr) { mAddr = addr; }
		void*			GetPtr(void* ptr) const { return (uint8_t*)ptr + mAddr; }

		void			SetSize(uint32_t size) { mSize = (mSize & FMASK) | ((size >> 4) & ~FMASK); }
		uint32_t		GetSize() const { return (mSize & ~FMASK) << 4; }

		bool			HasSizeLeft(uint32_t size) const
		{
			return size < GetSize();
		}

		void			Split(Nodes* nodes, NodeIdx self, uint32_t size, TNode*& next_node, NodeIdx& next_nodeidx)
		{
			nodes->Alloc(next_node, next_nodeidx);
			next_node->mAddr = mAddr + size;
			next_node->mSize = mSize;
			next_node->SetSize(GetSize() - size);

			LNode* lnode_self = nodes->IndexToLNode_Addr(self);
			lnode_self->InsertAfter(nodes, self, next_nodeidx);

			SetSize(size);
		}

	private:
		enum EFlags
		{
			USED = 0x80000000,
			FMASK = 0xF0000000,
		};
		uint32_t		mAddr;					// ADDRESS
		uint32_t		mSize;					// SIZE (*cMinimumSize)
	};

	// MNode is an entity of the bit based Size-Tree
	struct MNode
	{
		inline			MNode() : mBits(0) {}

		void			Init()
		{
			mBits = 0;
		}

		uint8_t			GetBits() const { return mBits; }
		void			SetBits(uint8_t bits) { mBits = bits; }

		bool			GetChildLower(uint32_t child_index, uint32_t& next_index) const
		{
			// In the child occupancy find an existing child at a lower position then @child_index.
			// Return the existing child-index in @existing_index and return 0 or 1. 
			// Return 0 if @child_index == @existing_index meaning that there is a child at @child_index.
			// Return 1 if at the current @child_index does not have a valid child but we did find an existing child
			//          at a lower index.
			// Return -1 if the child occupancy is empty.
			if (mBits == 0 || child_index == 0)
				return false;

			uint32_t occupance = mBits & ((1 << child_index) - 1);
			if (occupance == 0)
				return false;
			uint32_t existing = (occupance ^ (occupance & (occupance - 1)));

			next_index = 0;
			if ((existing & 0x0F) == 0) { next_index += 4;  existing = existing >> 4; }
			if ((existing & 0x03) == 0) { next_index += 2;  existing = existing >> 2; }
			if ((existing & 0x01) == 0) { next_index += 1; }
			return true;
		}

		bool			GetChildLowest(uint32_t& next_index) const
		{
			return GetChildLower(8, next_index);
		}

		bool			GetChildHigher(uint32_t child_index, uint32_t& next_index) const
		{
			// In the child occupancy find an existing child at the current or higher position then @child_index.
			// Return the existing child-index in @existing_index and return 0 or 1. 
			// Return 0 if @child_index == @existing_index meaning that there is a child at @child_index.
			// Return 1 if at the current @child_index does not have a valid child but we did find an existing child
			//          at a higher index.
			// Return -1 if the child occupancy is empty.
			if (mBits == 0 || child_index == 7)
				return false;

			uint32_t occupance = mBits & (0xFE << child_index);
			if (occupance == 0)
				return false;
			uint32_t existing = (occupance ^ (occupance & (occupance - 1)));

			next_index = 0;
			if ((existing & 0x0F) == 0) { next_index += 4;  existing = existing >> 4; }
			if ((existing & 0x03) == 0) { next_index += 2;  existing = existing >> 2; }
			if ((existing & 0x01) == 0) { next_index += 1; }
			return true;
		}

		void			SetChildUsed(uint32_t index) { uint8_t const bit = (uint8_t)(1 << index); mBits = mBits | bit; }
		void			SetChildEmpty(uint32_t index) { uint8_t const bit = (uint8_t)(1 << index); mBits = mBits & ~bit; }
		bool			GetChildUsed(uint32_t index) const { uint8_t const bit = (uint8_t)(1 << index); return (mBits & bit) == bit; }

	private:
		uint8_t			mBits;
	};

	template<>
	static inline MNode**	AllocateAndClear<MNode*>(int32_t count)
	{
		MNode** ptr = (MNode**)malloc(count * sizeof(MNode*));
		while (--count >= 0)
			ptr[count] = NULL;
		return ptr;
	}


	void			Nodes::Initialize(uint32_t min_range, uint32_t max_range, uint32_t min_alloc_size, uint32_t max_alloc_size)
	{
		mAddrRange.Set(min_range, max_range, 8);
		mAddrNodes = AllocateAndClear<NodeIdx>(mAddrRange.GetBaseLevelSize());

		mSizeRange.Set(min_alloc_size, max_alloc_size, 8, 1);
		mSizeNodes = AllocateAndClear<NodeIdx>(mSizeRange.GetBaseLevelSize());

		mSizeTree = AllocateAndClear<MNode*>(mSizeRange.GetBaseLevel() + 1);
		mSizeTree[0] = AllocateAndClear<MNode>(mSizeRange.GetTotalSize() >> 3);
		for (uint32_t i = mSizeRange.GetRootLevel(); i <= mSizeRange.GetBaseLevel(); ++i)
		{
			uint32_t numbytes = mSizeRange.GetLevelSize(i - 1) >> 3;
			mSizeTree[i] = mSizeTree[i - 1] + numbytes;
		}

		mTNode_FreeList = NodeIdx();
		mNumAllocatedTNodeBlocks = 0;
		for (uint32_t i = 0; i < cMaximumBlocks; ++i)
		{
			mAllocatedTNodes[i] = NULL;
			mAllocatedLNodes_Size[i] = NULL;
			mAllocatedLNodes_Addr[i] = NULL;
		}
	}

	void			Nodes::SetSizeUsed(Range range, uint32_t value)
	{
		assert(range.IsBaseLevel());

		uint8_t bits;
		do
		{
			uint32_t li = range.GetLevel();
			uint32_t ii = range.GetIndexFromValue(value);
			uint32_t ei = ii >> 3;
			MNode* mnode = &mSizeTree[li][ei];
			uint32_t bi = ii & 0x7;
			bits = mnode->GetBits();
			mnode->SetChildUsed(bi);
			if (range.IsRootLevel())
				break;
			range.TraverseUp(value);
		} while (bits == 0);
	}

	void			Nodes::SetSizeEmpty(Range range, uint32_t value)
	{
		assert(range.IsBaseLevel());

		uint8_t bits;
		do
		{
			uint32_t li = range.GetLevel();
			uint32_t ii = range.GetIndexFromValue(value);
			uint32_t ei = ii >> 3;
			MNode* mnode = &mSizeTree[li][ei];
			uint32_t bi = ii & 0x7;
			mnode->SetChildEmpty(bi);
			if (range.IsRootLevel())
				break;

			bits = mnode->GetBits();
			range.TraverseUp(value);
		} while (bits == 0);
	}

	class Allocator : public IAllocator
	{
	public:
		const uint32_t	cMinimumAllocSize = 1024;
		const uint32_t	cMaximumAllocSize = 32 * 1024 * 1024;
		const uint32_t	cGranulaAllocSize = 1024;

		void			Initialize(void* mem_base, uint32_t mem_size, uint32_t min_alloc_size, uint32_t max_alloc_size);
		void			Destroy();

		void*			Allocate(uint32_t size);
		void			Deallocate(void*);

	private:
		NodeIdx			FindSize(uint32_t size) const;
		NodeIdx			FindAddr(uint32_t addr) const;

		void			AddToAddr(NodeIdx node, uint32_t address);
		void			AddToSize(NodeIdx node, uint32_t size);

		void			RemoveFromAddr(NodeIdx node, uint32_t addr);
		void			RemoveFromSize(NodeIdx node, uint32_t size);

		int32_t			Coallesce(NodeIdx& n, TNode*& curr_tnode);

		void*			mMemBase;
		Nodes			mNodes;
	};

	NodeIdx		Allocator::FindSize(uint32_t size) const
	{
		assert(mNodes.mSizeRange.Contains(size));

		size = mNodes.mSizeRange.AlignValue(size);
		uint32_t const index = mNodes.mSizeRange.GetIndexFromValueAtBaseLevel(size);
		NodeIdx current = mNodes.mSizeNodes[index];

		// So can this TNode fullfill the size request ?
		if (current.IsNull() == false)
		{
			LNode* lnode = mNodes.IndexToLNode_Size(current);
			TNode* tnode = mNodes.IndexToTNode(current);
			while (true)
			{
				if (size >= tnode->GetSize())
					return current;
				if (lnode->Next().IsNull())
					break;
				current = lnode->Next();
				tnode = mNodes.IndexToTNode(current);
				lnode = mNodes.IndexToLNode_Addr(current);
			}
		}

		// Ok, so here we did not find an exact fit, go and find a best fit
		// Move up the tree to find a branch that holds a larger size.
		Range r = mNodes.mSizeRange.ToBaseLevel(size);

		uint32_t child_index = r.GetIndexFromValue(size) & 0x7;
		uint32_t mlevel_index = r.GetLevel();
		uint32_t mnode_index = r.GetIndexFromValue(size) >> 3;
		MNode* mnode = &mNodes.mSizeTree[mlevel_index][mnode_index];

		enum ETraverseDir { UP = -1, DOWN = 1 };
		ETraverseDir traverse_dir = UP;
		uint32_t next_index;
		while (true)
		{
			if (traverse_dir == UP)
			{
				if (mnode->GetChildHigher(child_index, next_index))
				{
					// We found a next size and now we should traverse down to
					// the base-level.
					child_index = next_index;
					r.SetChildIndex(child_index);
					traverse_dir = DOWN;
				}
				else
				{
					if (r.IsRootLevel())
						break;
					r.TraverseUp(size);

					child_index = r.GetIndexFromValue(size) & 0x7;
					mlevel_index = r.GetLevel();
					mnode_index = r.GetIndexFromValue(size) >> 3;
					mnode = &mNodes.mSizeTree[mlevel_index][mnode_index];
				}
			}
			else if (traverse_dir == DOWN)
			{
				if (r.IsBaseLevel())
				{
					// So do we have an existing TNode here ?
					mnode_index = r.GetIndex();
					current = mNodes.mSizeNodes[mnode_index];
					break;
				}

				r.TraverseDownChild(child_index);
				mlevel_index = r.GetLevel();
				mnode_index = r.GetIndex() >> 3;
				mnode = &mNodes.mSizeTree[mlevel_index][mnode_index];

				// When traversing down we need to traverse to the most minimum existing leaf.
				// So on every level pick the minimum existing child index and traverse down
				// into that child.
				if (mnode->GetChildLowest(child_index) == false)
					break;
			}
		}
		return current;
	}

	NodeIdx		Allocator::FindAddr(uint32_t address) const
	{
		assert(mNodes.mAddrRange.Contains(address));
		uint32_t const index = mNodes.mAddrRange.GetIndexFromValueAtBaseLevel(address);
		NodeIdx current = mNodes.mAddrNodes[index];

		// So does this TNode actually matches the address?
		if (current.IsNull() == false)
		{
			// This TNode is the start of a list that we need to iterate 
			// over to find the TNode that holds this address. We should
			// stop once we have a node that falls out of 'range'.
			LNode* lnode = mNodes.IndexToLNode_Addr(current);
			TNode* tnode = mNodes.IndexToTNode(current);
			Range baser = mNodes.mAddrRange.ToBaseLevel(address);
			while (baser.Contains(tnode->GetAddr()))
			{
				if (tnode->GetAddr() == address)
					return current;
				if (lnode->Next().IsNull())
					break;
				current = lnode->Next();
				tnode = mNodes.IndexToTNode(current);
				lnode = mNodes.IndexToLNode_Addr(current);
			}
		}
		return NodeIdx();
	}

	void		Allocator::AddToAddr(NodeIdx node, uint32_t address)
	{
		assert(mNodes.mAddrRange.Contains(address));

		// The TNode is already linked into the address-list, so we do not have to search an existing child.
		uint32_t const index = mNodes.mAddrRange.GetIndexFromValueAtBaseLevel(address);
		NodeIdx head = mNodes.mAddrNodes[index];

		if (head.IsNull())
		{
			// So the location where we should put this entry is not used yet
			mNodes.mAddrNodes[index] = node;
		}
		else
		{
			// Check if the address of us is smaller than the address of the current
			// node, if so we need to set node.
			TNode* head_tnode = mNodes.IndexToTNode(head);
			LNode* head_lnode = mNodes.IndexToLNode_Addr(head);
			if (address < head_tnode->GetAddr())
			{
				head_lnode->InsertBefore(&mNodes, head, node);
			}
			else
			{
				head_lnode->InsertAfter(&mNodes, head, node);
			}
		}
	}

	void		Allocator::AddToSize(NodeIdx node, uint32_t size)
	{
		uint32_t const index = mNodes.mSizeRange.GetIndexFromValueAtBaseLevel(size);
		NodeIdx head = mNodes.mSizeNodes[index];

		// So we found either a TNode as a child or Null
		if (head.IsNull())
		{
			// No existing nodes here yet, just set it
			mNodes.mSizeNodes[index] = node;

			Range r = mNodes.mSizeRange.ToBaseLevel(size);
			mNodes.SetSizeUsed(r, size);
		}
		else
		{	// We have an existing node here, so we have to add it to the list
			// We do not have to update the Size-Tree
			TNode* head_tnode = mNodes.IndexToTNode(head);
			LNode* head_lnode = mNodes.IndexToLNode_Size(head);
			if (size > head_tnode->GetSize())
			{
				head_lnode->InsertAfter(&mNodes, head, node);
			}
			else
			{
				head_lnode->InsertBefore(&mNodes, head, node);
			}
		}
	}

	void	Allocator::RemoveFromAddr(NodeIdx node, uint32_t address)
	{
		uint32_t const index = mNodes.mAddrRange.GetIndexFromValueAtBaseLevel(address);
		NodeIdx head = mNodes.mAddrNodes[index];

		if (head.IsNull() == false)
		{
			Range r = mNodes.mAddrRange.ToBaseLevel(address);
			NodeIdx current = head;
			TNode* tnode = mNodes.IndexToTNode(current);
			while (current != node && r.Contains(tnode->GetAddr()))
			{
				tnode = mNodes.IndexToTNode(current);
				LNode* lnode = mNodes.IndexToLNode_Addr(current);
				current = lnode->Next();
			};

			if (current == node)
			{
				LNode* lnode = mNodes.IndexToLNode_Addr(current);
				lnode->RemoveFrom(&mNodes, current, head);
				mNodes.mAddrNodes[index] = head;
			}
			else
			{
				// ? error
			}
		}
		else
		{
			// ? error
		}
	}

	void	Allocator::RemoveFromSize(NodeIdx node, uint32_t size)
	{
		uint32_t const index = mNodes.mSizeRange.GetIndexFromValueAtBaseLevel(size);
		NodeIdx head = mNodes.mSizeNodes[index];

		if (head.IsNull() == false)
		{
			Range r = mNodes.mAddrRange.ToBaseLevel(size);
			NodeIdx current = head;
			TNode* tnode = mNodes.IndexToTNode(current);
			while (current != node && r.Contains(tnode->GetAddr()))
			{
				tnode = mNodes.IndexToTNode(current);
				LNode* lnode = mNodes.IndexToLNode_Size(current);
				current = lnode->Next();
			};

			if (current == node)
			{
				LNode* lnode = mNodes.IndexToLNode_Size(current);
				lnode->RemoveFrom(&mNodes, current, head);
				mNodes.mAddrNodes[index] = head;
			}
			else
			{
				// ? error
			}
		}
		else
		{
			// ? error
		}
	}

	void	Allocator::Initialize(void* mem_base, uint32_t mem_size, uint32_t min_alloc_size, uint32_t max_alloc_size)
	{
		mMemBase = mem_base;
		mNodes.Initialize(min_alloc_size, mem_size, min_alloc_size, max_alloc_size);

		// Create initial node that holds all free memory
		TNode* tnode;
		NodeIdx tnode_index;
		mNodes.Alloc(tnode, tnode_index);
		tnode->SetAddr(0);
		tnode->SetSize(mem_size);
		tnode->SetFree();
		mem_size = tnode->GetSize();
		AddToSize(tnode_index, mem_size);
	}

	void*	Allocator::Allocate(uint32_t size)
	{
		NodeIdx curr = FindSize(size);
		if (curr.IsNull() == false)
		{
			TNode* curr_tnode = mNodes.IndexToTNode(curr);

			// Split of the size that we need if anything left above 
			// the minimum alloc size add it back to @size.
			if (size <= curr_tnode->GetSize())
			{
				RemoveFromSize(curr, curr_tnode->GetSize());
				if (curr_tnode->HasSizeLeft(size))
				{
					NodeIdx left_nodeidx;
					TNode* left_tnode;
					curr_tnode->Split(&mNodes, curr, size, left_tnode, left_nodeidx);
					left_tnode->SetFree();
					AddToSize(left_nodeidx, left_tnode->GetSize());
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
		uint32_t address = (uint32_t)((uint8_t*)p - (uint8_t*)mMemBase);
		NodeIdx curr = FindAddr(address);
		TNode* curr_tnode = mNodes.IndexToTNode(curr);
		curr_tnode->SetFree();
		RemoveFromAddr(curr, address);
		Coallesce(curr, curr_tnode);
		AddToSize(curr, curr_tnode->GetSize());
	}

	// return: 0 = nothing done
	//         1 = merged with @next
	//         2 = merged with @prev
	//         3 = merged with @next & @prev
	int32_t	Allocator::Coallesce(NodeIdx& curr, TNode*& curr_tnode)
	{
		LNode* curr_lnode = mNodes.IndexToLNode_Addr(curr);
		NodeIdx curr_next = curr_lnode->Next();
		NodeIdx curr_prev = curr_lnode->Prev();

		int32_t c = 0;

		// Is @next address free?
		//   Remove @next from @address 
		//   Remove @next from @size 
		//   Update size of @curr
		//   Deallocate @next
		TNode* next_tnode = mNodes.IndexToTNode(curr_next);
		if (next_tnode != NULL && next_tnode->IsFree())
		{
			// Ok, @next is going to disappear and merged into current
			curr_tnode->SetSize(curr_tnode->GetSize() + next_tnode->GetSize());
			RemoveFromAddr(curr_next, next_tnode->GetAddr());
			RemoveFromSize(curr_next, next_tnode->GetSize());
			mNodes.Dealloc(next_tnode, curr_next);
			c |= 1;
		}

		// Is @prev free?
		//   Remove @curr from @address 
		//   Remove @prev from @size 
		//   Update size of @prev
		//   Deallocate @curr
		//   Add @prev to @size 
		//   @curr = @prev
		TNode* prev_tnode = mNodes.IndexToTNode(curr_prev);
		if (prev_tnode != NULL && prev_tnode->IsFree())
		{
			prev_tnode->SetSize(curr_tnode->GetSize() + prev_tnode->GetSize());
			RemoveFromAddr(curr, curr_tnode->GetAddr());
			RemoveFromSize(curr, curr_tnode->GetSize());
			mNodes.Dealloc(curr_tnode, curr);
			curr = curr_prev;
			curr_tnode = prev_tnode;
			c |= 2;
		}

		return c;
	}



	TNode*			Nodes::IndexToTNode(NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		return (TNode*)&mAllocatedTNodes[bi][ei];
	}

	LNode*			Nodes::IndexToLNode_Size(NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		return (LNode*)&mAllocatedLNodes_Size[bi][ei];
	}
	LNode*			Nodes::IndexToLNode_Addr(NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		return (LNode*)&mAllocatedLNodes_Addr[bi][ei];
	}

	Nodes::FNode*	Nodes::IndexToFNode(NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		return (FNode*)&mAllocatedTNodes[bi][ei];
	}

	void			Nodes::Alloc(TNode*& p, NodeIdx& i)
	{
		if (mTNode_FreeList.IsNull())
		{
			mAllocatedTNodes[mNumAllocatedTNodeBlocks] = AllocateAndClear<TNode>(cNodesPerBlock);
			mAllocatedLNodes_Size[mNumAllocatedTNodeBlocks] = AllocateAndClear<LNode>(cNodesPerBlock);
			mAllocatedLNodes_Addr[mNumAllocatedTNodeBlocks] = AllocateAndClear<LNode>(cNodesPerBlock);
			const uint32_t block_index = (mNumAllocatedTNodeBlocks << 16);
			for (uint32_t i = 0; i<cNodesPerBlock; ++i)
			{
				NodeIdx ni(block_index | i);
				TNode* tnode = IndexToTNode(ni);
				Dealloc(tnode, ni);
			}
			mNumAllocatedTNodeBlocks += 1;
		}

		i = mTNode_FreeList;
		FNode* fnode = IndexToFNode(i);
		mTNode_FreeList = fnode->mNext;
		p = (TNode*)fnode;
	}

	void			Nodes::Dealloc(TNode* tnode, NodeIdx i)
	{
		FNode* fnode = IndexToFNode(i);
		fnode->mNext = mTNode_FreeList;
		mTNode_FreeList = i;
	}

};

IAllocator*		gCreateExtAllocator2(void* mem_base, uint32_t mem_size, uint32_t min_alloc_size, uint32_t max_alloc_size)
{
	HEXT_Allocator2::Allocator* allocator = new HEXT_Allocator2::Allocator();
	allocator->Initialize(mem_base, mem_size, min_alloc_size, max_alloc_size);
	return allocator;
}