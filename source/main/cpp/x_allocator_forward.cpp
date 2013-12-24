#include "xbase\x_target.h"
#include "xbase\x_memory_std.h"
#include "xbase\x_limits.h"
#include "xbase\x_integer.h"
#include "xbase\x_allocator.h"
#include "xbase\x_integer.h"

#include "xallocator\x_allocator_forward.h"
#include "xallocator\private\x_forwardbin.h"

namespace xcore
{

	class x_allocator_forward : public x_iallocator
	{
	public:
							x_allocator_forward();
							x_allocator_forward(xbyte* beginAddress, u32 size, x_iallocator* allocator);
		virtual				~x_allocator_forward();

		virtual const char*	name() const									{ return TARGET_FULL_DESCR_STR "[Allocator, Type=Forward]"; }

		void				initialize(void* beginAddress, u32 size);
		virtual void		release();
		
		virtual void*		allocate(xsize_t size, u32 alignment);
		virtual void*		reallocate(void* ptr, xsize_t size, u32 alignment);
		virtual void		deallocate(void* ptr);
	
		void*				operator new(xsize_t num_bytes)					{ return NULL; }
		void*				operator new(xsize_t num_bytes, void* mem)		{ return mem; }
		void				operator delete(void* pMem)						{ }
		void				operator delete(void* pMem, void* )				{ }

	private:
		x_iallocator*		mAllocator;
		u32					mTotalSize;
		xbyte*				mMemBegin;
		xforwardbin::xallocator mForwardAllocator;

							x_allocator_forward(const x_allocator_forward&);
							x_allocator_forward& operator= (const x_allocator_forward&);
	};


	x_allocator_forward::x_allocator_forward()
		: mAllocator(NULL)
		, mTotalSize(0)
		, mMemBegin(NULL)
	{

	}

	x_allocator_forward::x_allocator_forward(xbyte* beginAddress, u32 size, x_iallocator* allocator)
		: mAllocator(allocator)
		, mTotalSize(size)
		, mMemBegin(beginAddress)
	{ 
		mForwardAllocator.init(mMemBegin, mMemBegin + size);
	}

	x_allocator_forward::~x_allocator_forward()
	{
		release();
	}

	void x_allocator_forward::release()
	{
		mAllocator->deallocate(mMemBegin);
		mAllocator->deallocate(this);
	}

	void*	x_allocator_forward::allocate(xsize_t size, u32 alignment)
	{
		ASSERT(size < X_U32_MAX);
		return mForwardAllocator.allocate((u32)size, alignment);
	}

	void* x_allocator_forward::reallocate(void* ptr, xsize_t size, u32 alignment)
	{
		ASSERT(size < X_U32_MAX);
		u32 const old_size = mForwardAllocator.get_size(ptr);
		void* new_ptr = mForwardAllocator.allocate((u32)size, alignment);
		x_memcpy(new_ptr, ptr, old_size);
		mForwardAllocator.deallocate(ptr);
		return new_ptr;
	}

	void x_allocator_forward::deallocate(void* ptr)
	{
		return mForwardAllocator.deallocate(ptr);
	}

	x_iallocator*		gCreateForwardAllocator(x_iallocator* allocator, u32 memsize)
	{
		void* memForAllocator = allocator->allocate(sizeof(x_allocator_forward), 4);
		void* mem = allocator->allocate(memsize + 32, 4);
		x_allocator_forward* forwardRingAllocator = new (memForAllocator) x_allocator_forward((xbyte*)mem, memsize, allocator);
		return forwardRingAllocator;
	}

};

