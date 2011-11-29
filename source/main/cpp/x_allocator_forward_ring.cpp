#include "xbase\x_target.h"
#include "xbase\x_types.h"
#include "xbase\x_memory_std.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xbase\x_integer.h"

#include "xallocator\x_allocator.h"

namespace xcore
{
	#define	CHUNK_MAGIC		0xCAFEBABE

	class x_allocator_fr : public x_iallocator
	{
	public:
							x_allocator_fr();
							x_allocator_fr(xbyte* beginAddress, u32 size, x_iallocator* allocator);
		virtual				~x_allocator_fr();

		virtual const char*	name() const									{ return "Forward Ring Allocator"; }

		void				initialize(void* beginAddress, u32 size);
		virtual void		release();
		
		virtual void*		allocate(u32 size, u32 alignment);
		virtual void*		reallocate(void* ptr, u32 size, u32 alignment);
		virtual void		deallocate(void* ptr);
	
		void*				operator new(xsize_t num_bytes)					{ return NULL; }
		void*				operator new(xsize_t num_bytes, void* mem)		{ return mem; }
		void				operator delete(void* pMem)						{ }
		void				operator delete(void* pMem, void* )				{ }

	private:
		struct chunk	// 32 bytes
		{
		public:
			chunk(chunk* _prev, chunk* _next, u32 _index, u32 _size) { link[0] = _prev; link[1] = _next;  index[0] = _index; index[1] = _index; size = _size; }
			u32					begin_magic;
			chunk*				link[2];
			u32					index[2];		///< [1..MAX_UINT] Sequential chunks are coalesced, but we need to keep track of the index before and after us.
			u32					size;
			u16					is_used;
			u8					is_begin;
			u8					is_end;
			u32					end_magic;
		};

		///< Behavior:
		///< This allocator only allocates memory from what is in front of head, when it is
		///< detected that there is not enough free space between head and end it finds out
		///< if wrapping around (head=begin) and then verifying if we have enough free space
		///< (tail - head)

		struct xforwardring
		{
			xbyte*			mMemBegin;
			xbyte*			mMemEnd;
			
			chunk*			mBegin;
			chunk*			mEnd;

			chunk*			mHead;

			u32				mNumAllocations;

			xforwardring()
				: mMemBegin(NULL)
				, mMemEnd(NULL)
				, mHead(NULL)
				, mNumAllocations(0)
			{

			}

			void				reset()
			{
				// We hold a 'Begin' and 'End' chunk so that we only merge like a list and not like a ring.
				// Coalescing a block at the end with one at the beginning (wrapping around) is something
				// that we are trying to avoid by doing this.

				mBegin = (chunk*)mMemBegin;
				mEnd   = (chunk*)((u32)mMemEnd - sizeof(chunk));
				mHead  = (chunk*)((u32)mMemBegin + sizeof(chunk));

				mBegin->begin_magic = CHUNK_MAGIC;
				mBegin->link[0]     = mEnd;
				mBegin->link[1]     = mHead;
				mBegin->index[0]    = 0xffffffff;
				mBegin->index[1]    = 0xffffffff;
				mBegin->size        = 0;
				mBegin->is_used     = 1;
				mBegin->is_begin    = 1;
				mBegin->is_end      = 0;
				mBegin->end_magic   = CHUNK_MAGIC;

				mHead->begin_magic = CHUNK_MAGIC;
				mHead->link[0]     = mBegin;
				mHead->link[1]     = mEnd;
				mHead->index[0]    = 1;
				mHead->index[1]    = 1;
				mHead->size        = mMemEnd - mMemBegin - 3*sizeof(chunk);
				mHead->is_used     = 0;
				mHead->is_begin    = 0;
				mHead->is_end      = 0;
				mHead->end_magic   = CHUNK_MAGIC;

				mEnd->begin_magic = CHUNK_MAGIC;
				mEnd->link[0]     = mHead;
				mEnd->link[1]     = mHead;
				mEnd->index[0]    = 0xffffffff;
				mEnd->index[1]    = 0xffffffff;
				mEnd->size        = 0;
				mEnd->is_used     = 1;
				mEnd->is_begin    = 0;
				mEnd->is_end      = 1;
				mEnd->end_magic   = CHUNK_MAGIC;

				mNumAllocations = 0;
			}

			void				init(xbyte* mem_begin, xbyte* mem_end)
			{
				mMemBegin = mem_begin;
				mMemEnd = mem_end;
				reset();
			}

			inline chunk*		move_chunk(chunk* c, u32 num_bytes_to_move_forward)
			{
				if (num_bytes_to_move_forward == 0)
					return c;
				chunk temp = *c;
				c = (chunk*)((xbyte*)mHead + num_bytes_to_move_forward);
				*c = temp;
				return c;
			}

			inline xbyte*		allocate(u32 size, u32 alignment)
			{
				// do we have to align up ?
				size = x_intu::alignUp(size, 4);

				xbyte* alloc_address = NULL;
				while (true)
				{
					if (size < mHead->size)
					{
						alloc_address = (xbyte*)((((u32)mHead + sizeof(chunk)) + (alignment-1)) & ~(alignment-1));
						u32 const diff = ((u32)alloc_address - (u32)mHead) - sizeof(chunk);

						// Move chunk structure up with @diff bytes
						mHead = move_chunk(mHead, diff);
						if (mHead->link[0] != NULL)
							mHead->link[0]->link[1] = mHead;

						chunk* next = (chunk*)((u32)alloc_address + size);
						next->begin_magic = CHUNK_MAGIC;
						next->link[0]   = mHead;
						next->link[1]   = mHead->link[1];
						next->index[0]  = mHead->index[1] + 1;
						next->index[1]  = mHead->index[1] + 1;
						next->size      = mHead->size - diff - size - sizeof(chunk);
						next->is_used   = 0;
						next->is_begin  = 0;
						next->is_end    = 0;
						next->end_magic = CHUNK_MAGIC;

						mHead->link[1]  = next;
						mHead->size     = size;
						mHead->is_used  = 1;

						mHead = next;
						mNumAllocations++;
						break;
					}
					else if (mHead->link[1] == mEnd)
					{
						// It can happen (infrequently) that this chunk is limited by the end of our managed memory.
						// So here we can check if that happens and if so we need to take the next next chunk (wrap 
						// around and skipping the begin chunk) and see if we can fulfill the allocation request by 
						// doing that.

						// see if we can fulfill the allocation request by taking the next free chunk
						ASSERT(mHead->link[1] == mEnd && mHead->link[1]->link[1] == mBegin);

						chunk* next_free = mHead->link[1]->link[1];
						if (next_free->is_used == 0 && size <= next_free->size)
						{
							// Alignment might still pose a risk in fulfilling this request
							alloc_address = (xbyte*)((((u32)mHead + sizeof(chunk)) + (alignment-1)) & ~(alignment-1));
							u32 const diff = ((u32)alloc_address - (u32)mHead) - sizeof(chunk);
							if (size <= (next_free->size - diff))
							{
								// Ok we can fulfill this allocation request with the next chunk
								// Reset our Head to here
								mHead = next_free;
							}
						}
						else
						{
							// failed to fulfill the allocation request
							break;
						}
					}
					else
					{
						// failed to fulfill the allocation request
						break;
					}
				}
				return alloc_address;
			}

			inline void			deallocate(void* p)
			{
				// Deallocate will mark a chunk as is_used = 0
				// Then we will follow prev and next to see if we can coalesce.
				// When coalescing we also need to deal with mHead since we might
				// be able to move it back (although this is a Forward allocator)
				ASSERT(mNumAllocations > 0);

				--mNumAllocations;
				if (mNumAllocations == 0)
				{
					reset();
				}
				else
				{
					chunk* c = (chunk*)((u32)p - sizeof(chunk));
					ASSERT(c->begin_magic == CHUNK_MAGIC && c->end_magic == CHUNK_MAGIC);

					// Coalesce (unite) chunks can only happen when:
					// - neighbor chunk differs in index by 1
					// - neighbor chunk is a free chunk

					// First mark this chunk as free
					c->is_used = 0;

					// Coalesce backwards
					u32 backward_index = c->index[0] - 1;
					chunk* prev = c->link[0];
					chunk* backward = NULL;
					while (prev!=NULL && prev->is_used==0 && prev->index[1]==backward_index)
					{
						backward = prev;
						backward_index = prev->index[0] - 1;
						prev = prev->link[0];
					}
					++backward_index;

					// Coalesce forwards
					u32 forward_index = c->index[1] + 1;
					chunk* next = c->link[1];
					chunk* forward = NULL;
					while (next!=NULL && next->is_used==0 && next->index[0]==forward_index)
					{
						forward = next;
						forward_index = next->index[1] + 1;
						next = next->link[1];
					}
					--forward_index;

					// Did we go back or forward at all ?
					if (backward!=NULL && forward!=NULL)
					{
						// We went backwards and forwards
						backward->link[1]  = forward->link[1];
						backward->index[1] = forward->index[1];
						backward->size     = ((u32)forward->link[1] - (u32)backward) - sizeof(chunk);
						if ((u32)mHead>=(u32)backward && (u32)mHead<=(u32)forward)
							mHead = backward;
					}
					else if (backward!=NULL && forward==NULL)
					{
						// We only went backwards
						backward->link[1]  = c->link[1];
						backward->index[1] = c->index[1];
						backward->size     = ((u32)c->link[1] - (u32)backward) - sizeof(chunk);
						if ((u32)mHead>=(u32)backward && (u32)mHead<=(u32)c)
							mHead = backward;
					}
					else if (backward==NULL && forward!=NULL)
					{
						// We only went forwards
						c->link[1]  = forward->link[1];
						c->index[1] = forward->index[1];
						c->size     = ((u32)forward->link[1] - (u32)c) - sizeof(chunk);
						if ((u32)mHead>=(u32)c && (u32)mHead<=(u32)forward)
							mHead = c;
					}
				}
			}
		};

		x_iallocator*		mAllocator;
		u32					mTotalSize;
		xforwardring		mForwardRing;

							x_allocator_fr(const x_allocator_fr&);
							x_allocator_fr& operator= (const x_allocator_fr&);
	};


	x_allocator_fr::x_allocator_fr()
		: mAllocator(NULL)
		, mTotalSize(0)
	{

	}

	x_allocator_fr::x_allocator_fr(xbyte* beginAddress, u32 size, x_iallocator* allocator)
		: mAllocator(allocator)
		, mTotalSize(size)
	{ 
		mForwardRing.init(beginAddress, beginAddress + size);
	}

	x_allocator_fr::~x_allocator_fr()
	{
		release();
	}

	void x_allocator_fr::release()
	{
		mAllocator->deallocate(mForwardRing.mMemBegin);
		mAllocator->deallocate(this);
	}

	void*	x_allocator_fr::allocate(u32 size, u32 alignment)
	{
		return mForwardRing.allocate(size, alignment);
	}

	void* x_allocator_fr::reallocate(void* ptr, u32 size, u32 alignment)
	{
		chunk* c = (chunk*)((u32)ptr - sizeof(chunk));
		ASSERT(c->is_used==1 && c->begin_magic == CHUNK_MAGIC && c->end_magic == CHUNK_MAGIC);
		u32 old_size = c->size;
		void* new_ptr = mForwardRing.allocate(size, alignment);
		x_memcpy(new_ptr, ptr, old_size);
		mForwardRing.deallocate(ptr);
		return new_ptr;
	}

	void x_allocator_fr::deallocate(void* ptr)
	{
		return mForwardRing.deallocate(ptr);
	}

	x_iallocator*		gCreateForwardRingAllocator(x_iallocator* allocator, u32 memsize)
	{
		void* memForAllocator = allocator->allocate(sizeof(x_allocator_fr), 4);
		void* mem = allocator->allocate(memsize, 4);
		x_allocator_fr* forwardRingAllocator = new (memForAllocator) x_allocator_fr((xbyte*)mem, memsize, allocator);
		return forwardRingAllocator;
	}

};

