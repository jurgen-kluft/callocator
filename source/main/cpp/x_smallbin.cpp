#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_allocator.h"
#include "xbase/x_integer.h"

#include "xallocator/private/x_smallbin.h"

namespace xcore
{
	namespace xexternal
	{
		struct xsnode
		{
			xsnode*			mNodes[4];
		};

		struct xbnode
		{
			u32				mBitmap[4];
		};

		static inline void		clear(xsnode* snode)						{ snode->mNodes[0]=snode->mNodes[1]=snode->mNodes[2]=snode->mNodes[3]=NULL; }
		static inline void		clear(xbnode* bnode)						{ bnode->mBitmap[0]=bnode->mBitmap[1]=bnode->mBitmap[2]=bnode->mBitmap[3]=0xffffffff; }

		static inline void*		ptr_relative(void* base, void* ptr)			{ return (void*)((xbyte*)ptr - (xbyte*)base); }
		static inline void		setbit(u32& bitmap, u8 bit_idx)				{ bitmap |= (1<<bit_idx); }
		static inline void		clrbit(u32& bitmap, u8 bit_idx)				{ bitmap = bitmap & (~(1<<bit_idx)); }
		static inline u8		ffsbit(u32 d)										
		{
			if (d==0)
				return 32;
			u8 bi = 0;
			if ((d & 0x0000FFFF) == 0) {bi += 16; d = d >>16;}
			if ((d & 0x000000FF) == 0) {bi +=  8; d = d >> 8;}
			if ((d & 0x0000000F) == 0) {bi +=  4; d = d >> 4;}
			if ((d & 0x00000003) == 0) {bi +=  2; d = d >> 2;}
			if ((d & 0x00000001) == 0) {bi +=  1;}
			return bi;
		}

		// chunk : the chunk index, this has been computed with the bin-size and the pointer.
		//         At this stage the index is also relative to the node where we are now.
		// bitmap: The map of bits that represent the chunks of this page
		static u32		sb_allocate(u32 const level_idxs, xbnode* bnode, u32& outChunkIdx)
		{
			// Determine the maximum index into our integer array
			u32 const max_idx = (level_idxs >> 5) & 0x03;
		
			// Find a free bit (a '1' bit)
			u32 state = 0;
			for (u32 i=0; i<=max_idx; ++i)
			{
				u32& bitmap = bnode->mBitmap[i];
				if (bitmap!=0)
				{
					u8 const bit_idx = ffsbit(bitmap);
					ASSERT(bit_idx < 32);
					clrbit(bitmap, bit_idx);
					outChunkIdx = (i<<5) + bit_idx;

					state = (bitmap==0) ? 0 : 1;
					for (++i; i<=max_idx; ++i)
					{
						if (bnode->mBitmap[i] == 0)
							state = (state<<1) + 0;
						else
							state = (state<<1) | 1;
					}
					break;
				}
			}
			return state;	// We have no more free chunks
		}

		static u32		sb_allocate(u32 level_idx, u32 const level_idxs, xsnode* snode, x_iallocator* node_allocator, u32& outChunkIdx)
		{
			// We need to get to a level where there are free bits
			
			if (level_idx > 0)
			{
				// Determine the maximum index into our node array
				u32 const max_idx = (level_idxs >> ((level_idx*2)+5)) & 0x03;

				// Find a node that has children with free bits
				// Track our state as well to propagate it upwards
				u32 our_state = 0;
				s32 child_idx = 4;
				s32 null_branch_idx = 4;
				for (s32 i=max_idx; i>=0; --i)
				{
					xsnode*& node = snode->mNodes[i];
					if (node == NULL)
					{
						// This branch doesn't exist
						null_branch_idx = i;
						our_state = (our_state<<1) | 1;								// Shift in a '1', meaning that we have free chunks
					}
					else if (((uptr)node & 1) == 0)									// This node is not full if the low-bit == 0
					{
						child_idx = i;
						our_state = (our_state<<1) | 1;								// Shift in a '1', meaning that we have free chunks
					}
					else
					{
						// This child and the tree below is marked to have no free chunk(s)
						our_state = our_state << 1;
					}
				}

				// Catch incorrect behavior: we should have a free chunk available otherwise
				// we would not have been selected to allocate. It seems our parent didn't 
				// know that we are full.
				ASSERT(our_state!=0);

				if (child_idx < 4)
				{
					xsnode** child = &snode->mNodes[child_idx];
					u32 const state = sb_allocate(--level_idx, level_idxs, *child, node_allocator, outChunkIdx);
					if (state == 0)
					{
						our_state = our_state & ~(1<<child_idx);					// This branch is full, mark it in our state
						*child = (xsnode*)((uptr)(*child) | 1);				// This child has no more free chunks, mark our child pointer
					}
				}
				else if (null_branch_idx>=0 && null_branch_idx<4)
				{
					xsnode** child = &snode->mNodes[null_branch_idx];
					// Allocate a new node
					if (level_idx == 1)
					{
						xbnode* bnode = (xbnode*)node_allocator->allocate(sizeof(xsnode), 4);
						clear(bnode);
						*child = (xsnode*)bnode;
						sb_allocate(level_idxs, (xbnode*)(*child), outChunkIdx);
					}
					else
					{
						xsnode* snode = (xsnode*)node_allocator->allocate(sizeof(xsnode), 4);
						clear(snode);
						*child = snode;
						sb_allocate(--level_idx, level_idxs, snode, node_allocator, outChunkIdx);
					}
				}
				else
				{
					// No free bits in here, which should not be possible, since our
					// parent branch should have marked us as full.
					ASSERTS(false, "Error: this branch has no free chunk");
				}
				return our_state;
			}
			else
			{
				xbnode* node = (xbnode*)snode;
				return sb_allocate(level_idxs, node, outChunkIdx);
			}
		}

		static void		sb_deallocate(u16 chunk_idx, xbnode* bnode)
		{
			// which bit are we dealing with?
			u32 const word_idx = (chunk_idx>>5) & 0x02;
			u32 const bit_idx = chunk_idx & 0x1F;
			u32& bitmap = bnode->mBitmap[word_idx];
			setbit(bitmap, bit_idx);
		}

		static void		sb_deallocate(u16 level_idx, u32 chunk_idx, xsnode* snode)
		{
			u32 const idx = (chunk_idx>>((level_idx*2)+5)) & 0x03;
			if (level_idx > 1)
			{
				xsnode** node = &snode->mNodes[idx];
				sb_deallocate(--level_idx, chunk_idx, *node);
				*node = (xsnode*)((uptr)(*node) & ~(uptr)1);		// Sure that it now has a free chunk
			}
			else
			{
				xbnode** node = (xbnode**)&snode->mNodes[idx];
				sb_deallocate(chunk_idx, *node);
				*node = (xbnode*)((uptr)(*node) & ~(uptr)1);		// Sure that it now has a free chunk
			}
		}

		static u16		sb_compute_num_levels(u32 bin_size, u16 chunk_size);

		void		xsmallbin::init(void* base_address, u32 bin_size, u16 chunk_size)
		{
			mBaseAddress = base_address;
			mNode        = NULL;
			mBinSize     = bin_size;
			mChunkSize   = chunk_size;
			mLevels      = sb_compute_num_levels(bin_size, chunk_size);
		}

		struct release_info
		{
			xsnode*		node;
			u32					level;
			x_iallocator*		node_allocator;
		};

		static void	release_r				(release_info& info)
		{
			ASSERT(info.level > 0);
			if (info.level == 1)
			{
				xsnode* node = info.node;
				for (s32 i=0; i<4; ++i)
				{
					xbnode* bnode = (xbnode*)((uptr)node->mNodes[i] & ~(uptr)1);
					if (bnode != NULL)
						info.node_allocator->deallocate(bnode);
				}
			}
			else
			{
				info.level -= 1;
				xsnode* node = info.node;
				for (s32 i=0; i<4; ++i)
				{
					info.node = node->mNodes[i];
					if (info.node != NULL)
					{
						release_r(info);
						info.node_allocator->deallocate(info.node);
					}
				}
				info.node = node;
				info.level += 1;
			}
		}

		void		xsmallbin::release		(x_iallocator* node_allocator)
		{
			if (mLevels > 0)
			{
				release_info info;
				info.node = mNode;
				info.level = mLevels;
				info.node_allocator = node_allocator;
				release_r(info);
			}
			node_allocator->deallocate(mNode);
			mNode = NULL;
		}

		void*		xsmallbin::allocate(u32 size, u32 alignment, x_iallocator* node_allocator)
		{
			if (((uptr)mNode & 1) == 1)
			{
				// Full
				return NULL;
			}
			else if (mNode==NULL)
			{
				if (mLevels == 0)
				{
					xbnode* bnode = (xbnode*)node_allocator->allocate(sizeof(xsnode), 4);
					clear(bnode);
					mNode = (xsnode*)bnode;
				}
				else
				{
					xsnode* snode = (xsnode*)node_allocator->allocate(sizeof(xsnode), 4);
					clear(snode);
					mNode = snode;
				}
			}

			// This allows us to compute the maximum index per level
			u32 const level_idxs = x_intu::ceilPower2(mBinSize / mChunkSize) - 1;

			u32 chunk_idx;
			if (sb_allocate(mLevels,level_idxs, mNode, node_allocator, chunk_idx) == 0)
				mNode = (xsnode*)((uptr)mNode | 0x00000001);

			void* ptr = (void*)((uptr)mBaseAddress + (chunk_idx*mChunkSize));
			return ptr;
		}

		void		xsmallbin::deallocate(void* ptr)
		{
			ptr = ptr_relative(mBaseAddress, ptr);
			u32 const chunk_idx = (u32)ptr / (u32)mChunkSize;
			if (mLevels == 0)
			{
				sb_deallocate(chunk_idx, (xbnode*)mNode);
			}
			else
			{
				sb_deallocate(mLevels, chunk_idx, mNode);
			}
			mNode = (xsnode*)((uptr)mNode & ~(uptr)1);
		}


		/*
			bin8 (4 levels) = 32 * 4 * 8 = 1024 (8192 bits / 32 == 256 u32 = 1024 bytes
				L1 65336 (1)
					L2 16384 (4)
						L3 4096 (16)
							L4 1024 - bitmap

			bin16 (4 levels) = 32 * 4 * 16 = 2048 (4096 bits / 32 = 128 u32 = 512 bytes)
				L1 65336
					L2 16384
						L3 4096
							L4 1024 - bitmap

			bin32 (3 levels) = 32 * 4 * 32 = 4096 (2048 bits / 32 = 64 u32 = 256 bytes)
				L1 65336
					L2 16384
						L3 4096 - bitmap

			bin64 (3 levels) = 32 * 4 * 64 = 8192 (1024 bits / 32 = 32 u32 = 128 bytes)
				L1 65336
					L2 16384
						L3 4096 - bitmap

			bin128 (2 levels) = 32 * 4 * 128 = 16384 (512 bits / 32 = 16 u32 = 64 bytes)
				L1 65336
					L2 16384 - bitmap
		*/
		static u16		sb_compute_num_levels(u32 bin_size, u16 chunk_size)
		{
			// bnode will contain 4*32 bits, so it divides the binsize by 128
			u32 level_size = (bin_size/128);

			// snode at every level will split the size by 4
			u32 level = 0;
			while (chunk_size < level_size)
			{
				++level;
				level_size = level_size / 4;
			}
			return level;
		}
	}
}