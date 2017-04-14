
#include <cstdlib>
#include <stdio.h>
#include <atomic>
#include <assert.h>

namespace HEXT_Allocator
{
	/*
		1 GB / 1024 = 1024 * 1024 = 1

		262144
		4096 x 64
		64 x 64 x 64

		8 x 64 x 64 x 64 x 1024 = 2 GB
		3 x  6 x  6 x  6 x   10 = 31 bits

		8 x 8 x 8 x 8 x 8->Linked - List(max 64 (8 x 8) items)

		Size is broken down in the same way as the address
		Max - size = 32 MB
		Min - size = 1 KB
		Granularity = 1 KB
		(Max - size - Min - size) / Granularity = 32 K num entries
		8 x 8 x 8 x 8 x 8
	*/

	const static uint32_t	cNullNode      = 0x7FFFFFFF;
	const static uint16_t	cNodeTypeMask  = 0x80000000;
	const static uint16_t	cNodeTypeHNode = 0x00000000;
	const static uint16_t	cNodeTypeTNode = 0x80000000;

	struct NodeIdx
	{
		inline			NodeIdx() : mIndex(cNullNode) {}
		inline			NodeIdx(uint32_t index) : mIndex(index) {}
		inline			NodeIdx(const NodeIdx& c) : mIndex(c.mIndex) {}

		void			Reset()										{ mIndex = cNullNode; }
		bool			IsNull() const								{ return mIndex == cNullNode; }

		inline void		SetIndex(uint32_t index)					{ mIndex = (mIndex & cNodeTypeMask) | index; }
		inline uint32_t	GetIndex() const							{ return mIndex & ~cNodeTypeMask; }

		inline void		SetType(uint32_t type)						{ mIndex = (mIndex & ~cNodeTypeMask) | type; }
		inline uint32_t	GetType() const								{ return mIndex & cNodeTypeMask; }
		inline bool		IsType(uint32_t type) const					{ return (mIndex & cNodeTypeMask) == type; }

		inline bool		operator == (const NodeIdx& other) const	{ return mIndex == other.mIndex; }
		inline bool		operator != (const NodeIdx& other) const	{ return mIndex != other.mIndex; }

	private:
		uint32_t		mIndex;
	};

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

		void		Set(uint32_t from, uint32_t to, uint32_t divisor, uint32_t min_range)
		{
			mRange[0] = from;
			mRange[1] = to;
			mDivisor = divisor;
			mMaxRange = Distance();
			mMinRange = min_range;
		}

		uint32_t	Distance() const		{ return mRange[1] - mRange[0]; }
		bool		IsRootLevel() const		{ return Distance() == mMaxRange; }
		bool		IsBaseLevel() const		{ return Distance() == mMinRange; }

		bool		Contains(uint32_t value) const
		{
			return value >= mRange[0] && value < mRange[1];
		}

		uint32_t	TraverseUp(uint32_t value)
		{
			assert(value >= mRange[0] && value < mRange[1]);
			if (IsRootLevel() == false)
			{
				uint32_t s = Distance() * mDivisor;
				mRange[0] = mRange[0] & ~(s - 1);
				mRange[1] = mRange[0] + s;
			}
			uint32_t const subr = Distance() / mDivisor;
			uint32_t const index = (value - mRange[0]) / subr;
			return index;
		}

		uint32_t	TraverseDown(uint32_t value)
		{
			assert(value >= mRange[0] && value < mRange[1]);
			uint32_t const subr = (mRange[1] - mRange[0]) / mDivisor;
			uint32_t const index = (value - mRange[0]) / subr;
			if (IsBaseLevel() == false)
			{
				uint32_t const from = index * subr;
				uint32_t const to = from + subr;
				mRange[0] = from;
				mRange[1] = to;
			}
			return index;
		}

		uint32_t	mMaxRange;
		uint32_t	mMinRange;
		uint32_t	mDivisor;
		uint32_t	mRange[2];
	};


	struct HNode;
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

		// HNode[]
		NodeIdx			mHNode_FreeList;
		uint32_t		mNumAllocatedHNodeBlocks;
		HNode*			mAllocatedHNodes[cMaximumBlocks];
		MNode*			mAllocatedMNodes[cMaximumBlocks];

		// TNode[]
		NodeIdx			mTNode_FreeList;
		uint32_t		mNumAllocatedTNodeBlocks;
		TNode*			mAllocatedTNodes[cMaximumBlocks];
		LNode*			mAllocatedLNodes_Size[cMaximumBlocks];
		LNode*			mAllocatedLNodes_Addr[cMaximumBlocks];

		Nodes::Nodes()
			: mNumAllocatedHNodeBlocks(0)
			, mNumAllocatedTNodeBlocks(0)
		{
			for (uint32_t i = 0; i < cMaximumBlocks; ++i)
			{
				mAllocatedHNodes[i] = NULL;
				mAllocatedTNodes[i] = NULL;
			}
		}
		enum EListType
		{
			SIZE = 0,
			ADDR = 1
		};

		HNode*			IndexToHNode(NodeIdx i) const;
		TNode*			IndexToTNode(NodeIdx i) const;
		LNode*			IndexToLNode(EListType t, NodeIdx i) const;
		LNode*			IndexToLNode_Size(NodeIdx i) const;
		LNode*			IndexToLNode_Addr(NodeIdx i) const;
		MNode*			IndexToMNode(NodeIdx i) const;
		FNode*			IndexToFNodeForHNode(NodeIdx i) const;
		FNode*			IndexToFNodeForTNode(NodeIdx i) const;

		void			Alloc(HNode*& p, NodeIdx& i);
		void			Alloc(TNode*& p, NodeIdx& i);
		void			Dealloc(HNode* hnode, NodeIdx i);
		void			Dealloc(TNode* tnode, NodeIdx i);
	};


	// 2 * 4 = 8 bytes
	struct LNode
	{
		void		InsertBefore(Nodes* nodes, NodeIdx self, NodeIdx& head)
		{
			LNode* headn = nodes->IndexToLNode_Size(head);
			LNode* prevn = nodes->IndexToLNode_Size(headn->mPrev);
			LNode* nextn = nodes->IndexToLNode_Size(headn->mNext);
			mNext = head;
			mPrev = headn->mPrev;
			prevn->mNext = self;
			nextn->mPrev = self;
		}

		void		RemoveFrom(Nodes* nodes, NodeIdx self, NodeIdx& head)
		{
			LNode* prevn = nodes->IndexToLNode_Size(mPrev);
			LNode* nextn = nodes->IndexToLNode_Size(mNext);
			prevn->mNext = mNext;
			nextn->mPrev = mPrev;
			if (head == self)
				head = mNext;
		}

		NodeIdx		Next() const				{ return mNext; }
		NodeIdx		Prev() const				{ return mPrev; }

	private:
		NodeIdx		mNext, mPrev;
	};

	// 8 bytes
	struct TNode
	{
		void			SetFree()				{ mSize = mSize & ~USED; }
		void			SetUsed()				{ mSize = mSize |  USED; }
		bool			IsFree() const			{ return (mSize & USED) == USED; }

		uint32_t		GetAddr() const			{ return (mAddr); }
		void			SetAddr(uint32_t addr)	{ mAddr = addr; }

		void			SetSize(uint32_t size)	{ mSize = (mSize & ~FMASK) | (size & ~FMASK); }
		uint32_t		GetSize() const			{ return (mSize & ~FMASK); }

	private:
		enum EFlags
		{
			USED = 0x80000000,
			FMASK = 0x80000000,

		};
		uint32_t		mAddr;					// ADDRESS
		uint32_t		mSize;					// SIZE (*cMinimumSize)
	};

	struct MNode
	{
		inline			MNode() : mInfo(0) {}

		uint8_t			GetUsed() const								{ return mInfo; }
		void			SetUsed(uint8_t used)						{ mInfo = used; }

		void			SetChildUsed(uint32_t index)				{ uint8_t const bit = (uint8_t)(1 << index); mInfo = mInfo | bit; }
		void			SetChildNull(uint32_t index)				{ uint8_t const bit = (uint8_t)(1 << index); mInfo = mInfo & ~bit; }
		bool			GetChildUsed(uint32_t index) const			{ uint8_t const bit = (uint8_t)(1 << index); return (mInfo & bit) == bit; }

	private:
		uint8_t			mInfo;
	};

	// 32 bytes
	struct HNode
	{
		const static uint32_t	cNumChildren = 8;

		void			Reset()
		{
			for (uint32_t i = 0; i < cNumChildren; ++i)
				mChildren[i].Reset();
		}
		void			SetChild(uint32_t idx, NodeIdx child)
		{
			mChildren[idx] = child;
		}
		NodeIdx			GetChild(uint32_t idx) const
		{
			return mChildren[idx];
		}

	private:
		NodeIdx			mChildren[cNumChildren];
	};

	struct Allocator
	{
	public:
		const uint32_t	cMinimumAllocSize = 1024;
		const uint32_t	cMaximumAllocSize = 32 * 1024 * 1024;
		const uint32_t	cGranulaAllocSize = 1024;

		void		Initialize(void* base, uint32_t size);

		void*		Alloc(uint32_t size);
		void		Deallocate(void*);

	private:
		bool		FindSize(uint32_t size);
		bool		FindAddr(uint32_t addr);
		void		Coallesce(TNode*& curr);

		NodeIdx		FindAddress(Nodes* nodes, NodeIdx current, uint32_t address, Range range);

		NodeIdx		AddToAddrHierarchy(Nodes* nodes, NodeIdx& current, NodeIdx node, uint32_t address, Range range);
		NodeIdx		AddToSizeHierarchy(Nodes* nodes, NodeIdx current, NodeIdx node, uint32_t size, Range range);

		NodeIdx		RemoveFromAddrHierarchy(Nodes* nodes, NodeIdx current, NodeIdx node, uint32_t addr, Range range);
		NodeIdx		RemoveFromSizeHierarchy(Nodes* nodes, NodeIdx current, NodeIdx node, uint32_t size, Range range);

		NodeIdx		FindBestFitInSizeHierarchy(Nodes* nodes, NodeIdx current, uint32_t size, Range range);

		Nodes		mNodes;

		NodeIdx		mAddrHierarchy;
		NodeIdx		mSizeHierarchy;
	};

	// The current HNode has N children where every child represents a part of 
	// the range that this HNode covers. When we ask for an existing child this
	// function will return a child that is at the current index or higher in
	// the range. Of course when this HNode has no children then it will return
	// an invalid
	static inline int32_t	CountLeadingZeros(uint8_t i8)
	{
		int32_t count = 0;
		if ((i8 & 0x0F) == 0) { count += 4;  i8 = i8 >> 4; }
		if ((i8 & 0x03) == 0) { count += 2;  i8 = i8 >> 2; }
		if ((i8 & 0x01) == 0) { count += 1; }
		return count;
	}

	// In the child occupancy find an existing child at the current or higher position then @child_index.
	// Return the existing child-index in @existing_index and return 0 or 1. 
	// Return 0 if @child_index == @existing_index meaning that there is a child at @child_index.
	// Return 1 if at the current @child_index does not have a valid child but we did find an existing child
	//          at a higher index.
	// Return -1 if the child occupancy is empty.
	inline int32_t FindExistingChild_GE(uint8_t child_occupance, uint32_t child_index, uint32_t& existing_index)
	{
		if (child_occupance == 0)
			return -1;

		uint32_t occupance = child_occupance & (0xFF << child_index);
		uint32_t existing = (occupance ^ (occupance & (occupance - 1)));
		existing_index = (1 << child_index) == existing ? 0 : child_index;
		child_index = CountLeadingZeros(existing);
		return existing_index==child_index ? 0 : 1;
	}

	struct HIterator
	{
		inline		HIterator() : mPathLength(0) {}

		bool		NotEmpty() const { return mPathLength > 0; }

		void		Push(NodeIdx node, uint32_t index)
		{
			mNode[mPathLength] = node;
			mIndex[mPathLength] = index;
			++mPathLength;
		}

		NodeIdx		Pop(uint32_t& index)
		{
			--mPathLength;
			index = mIndex[mPathLength];
			return mNode[mPathLength];
		}

		Nodes*		mNodes;
		uint32_t	mPathLength;

		struct PathNode
		{
			NodeIdx		mParent;
			uint32_t	mChildIndex;	// Index of child in parent
			NodeIdx		mChildNode;		// The child
		};

		uint32_t	mIndex[7];
		NodeIdx		mNode[7];
	};

	NodeIdx		Allocator::FindAddress(Nodes* nodes, NodeIdx current, uint32_t address, Range range)
	{
		while (range.IsBaseLevel() == false)
		{
			MNode* mcurrent = nodes->IndexToMNode(current);
			uint8_t child_occupance = mcurrent->GetUsed();
			HNode* hnode = nodes->IndexToHNode(current);
			uint32_t child_index = range.TraverseDown(address);

			// Is this child a TNode or a HNode ? If it is a TNode
			// or Null we cannot iterate further down the hierarchy.
			if (current.IsNull() || current.GetType() == cNodeTypeTNode)
				break;

			current = hnode->GetChild(child_index);
		}

		// So does this TNode actually matches the address?
		if (current.IsNull() == false)
		{
			TNode* tnode = nodes->IndexToTNode(current);

			// This TNode is the start of a list that we need to iterate 
			// over to find the TNode that holds this address. We should
			// stop once we have a node that falls out of 'range'.
			LNode* lnode = nodes->IndexToLNode_Addr(current);
			while (range.Contains(tnode->GetAddr()))
			{
				if (tnode->GetAddr() == address)
					return current;
				current = lnode->Next();
				tnode = nodes->IndexToTNode(current);
				lnode = nodes->IndexToLNode_Addr(current);
			}
		}
		return NodeIdx();
	}


	NodeIdx		Allocator::AddToAddrHierarchy(Nodes* nodes, NodeIdx& current, NodeIdx node, uint32_t address, Range range)
	{
		assert(current.IsType(cNodeTypeHNode));
		assert(node.IsType(cNodeTypeTNode));

		// The TNode is already linked into the address-list, so we do not have to search an existing child.
		// We really need to add it to the exact place in the hierarchy.

		HIterator it;
		while (range.IsBaseLevel() == false)
		{
			MNode* mcurrent = nodes->IndexToMNode(current);
			uint8_t child_occupance = mcurrent->GetUsed();
			HNode* hnode = nodes->IndexToHNode(current);
			uint32_t child_index = range.TraverseDown(address);

			it.Push(current, child_index);
			current = hnode->GetChild(child_index);

			// Does this child have a node and is it a TNode or a HNode ? 
			// If it is TNode we cannot iterate further down the hierarchy.
			if (current.IsNull() == true || current.GetType() == cNodeTypeTNode)
				break;
		}

		if (current.IsNull())
		{
			// So the location where we should put this entry is not used yet
			// Get the parent
			uint32_t child_index;
			NodeIdx parent = it.Pop(child_index);
			HNode* hnode = nodes->IndexToHNode(parent);
			hnode->SetChild(child_index, node);
		}
		else if (current.IsType(cNodeTypeTNode))
		{
			// Are we add the base-level? If not we need to push down both
			// @current and @node until their child-index is different or
			// when reaching base-level.

			// @TODO

		}
		else if (current.IsType(cNodeTypeHNode))
		{
			// We reached base-level

			// @TODO

		}
		return NodeIdx();
	}

	NodeIdx		Allocator::AddToSizeHierarchy(Nodes* nodes, NodeIdx current, NodeIdx node, uint32_t size, Range range)
	{
		assert(current.IsType(cNodeTypeHNode));
		assert(node.IsType(cNodeTypeTNode));

		HIterator it;
		while (range.IsBaseLevel() == false)
		{
			MNode* mcurrent = nodes->IndexToMNode(current);
			uint8_t child_occupance = mcurrent->GetUsed();
			HNode* hnode = nodes->IndexToHNode(current);
			uint32_t child_index = range.TraverseDown(size);
			NodeIdx parent = current;

			it.Push(current, child_index);
			current = hnode->GetChild(child_index);
			if (current.IsNull())
			{
				// No node here, we could set this TNode here at this child.
				mcurrent->SetChildUsed(child_index);
				hnode->SetChild(child_index, node);
				return parent;
			}

			// Is this child a TNode or a HNode ? If it is a TNode
			// we cannot iterate further down the hierarchy.
			if (current.IsType(cNodeTypeTNode))
				break;
		}

		// So we found either a TNode as a child or we reached base-level in the hierarchy
		if (current.IsType(cNodeTypeTNode))
		{
			// Here we found a TNode and we need to push both nodes down the hierarchy
			// Traverse down creating HNodes until the child-index of @current and @node
			// are different or when reaching base-level. Also make sure to mark the
			// occupancy when traversing down.
		}
		else
		{	// We reached base-level and we have a HNode so we just have to insert @node 
			// at the child location.
			// Also mark this child as used in the occupancy.

		}

		return NodeIdx();
	}

	NodeIdx	Allocator::RemoveFromAddrHierarchy(Nodes* nodes, NodeIdx current, NodeIdx node, uint32_t address, Range range)
	{
		HIterator it;
		while (range.IsBaseLevel() == false)
		{
			MNode* mcurrent = nodes->IndexToMNode(current);
			uint8_t child_occupance = mcurrent->GetUsed();
			HNode* hnode = nodes->IndexToHNode(current);
			uint32_t child_index = range.TraverseDown(address);

			it.Push(current, child_index);
			current = hnode->GetChild(child_index);

			// Is this child a TNode or a HNode ? If it is Null or a TNode
			// we cannot iterate further down the hierarchy.
			if (current.IsNull() || current.GetType() == cNodeTypeTNode)
				break;
		}

		if (current.IsNull() == false)
		{
			if (current == node)
			{
				// Found it!
			}
			else
			{
				// Iterate over the addr-list until we move out of the current range.
			}
		}
		return NodeIdx();
	}

	NodeIdx	Allocator::RemoveFromSizeHierarchy(Nodes* nodes, NodeIdx current, NodeIdx node, uint32_t size, Range range)
	{
		HIterator it;
		while (range.IsBaseLevel() == false)
		{
			MNode* mcurrent = nodes->IndexToMNode(current);
			uint8_t child_occupance = mcurrent->GetUsed();
			HNode* hnode = nodes->IndexToHNode(current);
			uint32_t child_index = range.TraverseDown(size);

			it.Push(current, child_index);
			current = hnode->GetChild(child_index);

			// Is this child a TNode or a HNode ? If it is Null or a TNode
			// we cannot iterate further down the hierarchy.
			if (current.IsNull() || current.GetType() == cNodeTypeTNode)
				break;
		}

		if (current.IsNull() == false)
		{
			if (current == node)
			{
				// Found it!
				// When removing it here we need to check if there are any childs left.
				// If not we should remove our mark from the occupancy and if the occupancy
				// then becomes 'zero' we should traverse up and remove our child mark from
				// the parent etc..
			}
			else
			{
				// Iterate over the size-list until we move out of the current range.
				// When we remove it from the list check if we where the only one in the list.
				// If so we need to unmark occupancy and traverse upwards resetting occupancy.
			}
		}
		return NodeIdx();
	}

	NodeIdx	Allocator::FindBestFitInSizeHierarchy(Nodes* nodes, NodeIdx current, uint32_t size, Range range)
	{
		HIterator it;
		while (current.IsNull() == false)
		{
			MNode* mcurrent = nodes->IndexToMNode(current);
			uint8_t child_occupancy = mcurrent->GetUsed();
			HNode* hnode = nodes->IndexToHNode(current);
			uint32_t child_index = range.TraverseDown(size);
			
			uint32_t existing_index;
			if (FindExistingChild_GE(child_occupancy, child_index, existing_index) == -1)
			{
				// This means there are no TNodes in the hierarchy that can match this size
				return NodeIdx();
			}

			it.Push(current, child_index);
			current = hnode->GetChild(existing_index);

			// Is this child a TNode or a HNode ? If it is Null or a TNode
			// we cannot iterate further down the hierarchy.
			if (current.IsNull() || current.GetType() == cNodeTypeTNode)
				break;
		}

		if (current.IsNull() == false)
		{
			TNode* tnode = nodes->IndexToTNode(current);
			if (size >= tnode->GetSize())
			{
				// Ok, this TNode can give us the size that is requested
				// See if there is any size left, if so then this node
				// should be removed and added back to the hierarchy,
				// otherwise take all!
				if ((tnode->GetSize() - size) >= cMinimumAllocSize)
				{
					// Remove current from SizeHierarchy and add it back with
					// the requested size substracted.

				}
			}
		}
		return NodeIdx();
	}


	bool	Allocator::FindAddr(uint32_t addr)
	{
		return false;
	}

	bool	Allocator::FindSize(uint32_t addr)
	{
		return false;
	}

	void	Allocator::Initialize(void* base, uint32_t size)
	{

	}

	void*	Allocator::Alloc(uint32_t size)
	{
		return NULL;
	}

	void	Allocator::Deallocate(void*)
	{
	}

	void	Allocator::Coallesce(TNode*& curr)
	{
		// Is @next address free?
		//   Remove @next from the address hierarchy
		//   Remove @next from the size hierarchy
		//   Deallocate @next
		//   Update size of @curr

		// Is @prev free?
		//   Remove @curr from the address hierarchy
		//   Remove @prev from the size hierarchy
		//   Update size of @prev
		//   Deallocate @curr
		//   Add @prev to the size hierarchy
		//   @curr = @prev
	}









	HNode*			Nodes::IndexToHNode(NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		return &mAllocatedHNodes[bi][ei];
	}
	TNode*			Nodes::IndexToTNode(NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		return (TNode*)&mAllocatedTNodes[bi][ei];
	}

	LNode*			Nodes::IndexToLNode(EListType t, NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		switch (t)
		{
		case SIZE: return (LNode*)&mAllocatedLNodes_Size[bi][ei];
		case ADDR: return (LNode*)&mAllocatedLNodes_Addr[bi][ei];
		}
		return NULL;
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
	MNode*			Nodes::IndexToMNode(NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		return (MNode*)&mAllocatedMNodes[bi][ei];
	}
	Nodes::FNode*	Nodes::IndexToFNodeForHNode(NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		return (FNode*)&mAllocatedHNodes[bi][ei];
	}
	Nodes::FNode*	Nodes::IndexToFNodeForTNode(NodeIdx i) const
	{
		if (i.IsNull())
			return NULL;
		uint32_t bi = (i.GetIndex() >> 16) & 0xFFFF;
		uint32_t ei = i.GetIndex() & 0xFFFF;
		return (FNode*)&mAllocatedTNodes[bi][ei];
	}

	void			Nodes::Alloc(HNode*& p, NodeIdx& i)
	{
		if (mHNode_FreeList.IsNull())
		{
			mAllocatedHNodes[mNumAllocatedHNodeBlocks] = (HNode*)malloc(sizeof(HNode) * cNodesPerBlock);
			mAllocatedMNodes[mNumAllocatedHNodeBlocks] = (MNode*)malloc(sizeof(MNode) * cNodesPerBlock);
			mNumAllocatedHNodeBlocks += 1;
			const uint32_t block_index = (mNumAllocatedHNodeBlocks << 16) | cNodeTypeHNode;;
			for (uint32_t i = 0; i<cNodesPerBlock; ++i)
			{
				NodeIdx ni(block_index | i);
				HNode* hnode = IndexToHNode(ni);
				Dealloc(hnode, ni);
			}
		}

		i = mHNode_FreeList;
		FNode* fnode = IndexToFNodeForHNode(i);
		mHNode_FreeList = fnode->mNext;
		p = (HNode*)fnode;
	}

	void			Nodes::Alloc(TNode*& p, NodeIdx& i)
	{
		if (mTNode_FreeList.IsNull())
		{
			mAllocatedTNodes[mNumAllocatedTNodeBlocks] = (TNode*)malloc(sizeof(TNode) * cNodesPerBlock);
			mAllocatedLNodes_Size[mNumAllocatedTNodeBlocks] = (LNode*)malloc(sizeof(LNode) * cNodesPerBlock);
			mAllocatedLNodes_Addr[mNumAllocatedTNodeBlocks] = (LNode*)malloc(sizeof(LNode) * cNodesPerBlock);
			mNumAllocatedTNodeBlocks += 1;
			const uint32_t block_index = (mNumAllocatedTNodeBlocks << 16) | cNodeTypeTNode;
			for (uint32_t i = 0; i<cNodesPerBlock; ++i)
			{
				NodeIdx ni(block_index | i);
				TNode* tnode = IndexToTNode(ni);
				Dealloc(tnode, ni);
			}
		}

		i = mTNode_FreeList;
		FNode* fnode = IndexToFNodeForTNode(i);
		mTNode_FreeList = fnode->mNext;
		p = (TNode*)fnode;
	}

	void			Nodes::Dealloc(HNode* hnode, NodeIdx i)
	{
		FNode* fnode = IndexToFNodeForHNode(i);
		fnode->mNext = mHNode_FreeList;
		mHNode_FreeList = i;
	}

	void			Nodes::Dealloc(TNode* tnode, NodeIdx i)
	{
		FNode* fnode = IndexToFNodeForTNode(i);
		fnode->mNext = mTNode_FreeList;
		mTNode_FreeList = i;
	}

};