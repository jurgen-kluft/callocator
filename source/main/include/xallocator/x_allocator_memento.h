//==============================================================================
//  x_allocator_memento.h
//==============================================================================
#ifndef __X_ALLOCATOR_MEMENTO_H__
#define __X_ALLOCATOR_MEMENTO_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

#include "xbase/x_allocator.h"

// ----------------------------------------------------------------------------
// Memento: A class to aid debugging of memory leaks/heap corruption.
// 
// Note: This class is not thread-safe by itself.
// 
// Q&A: More than one memory allocator? 
//
//         Yes, you can create multiple instances of this allocator.
//
//
// ----------------------------------------------------------------------------
// Usage:
//    First include this header file wherever you use malloc, realloc or free.
//    Implement your own class derived from x_iallocator that either use 
//    malloc/realloc/free or your own allocator.
//    Also you need to give memento an allocator that doesn't use this memento
//    instance so that this instance can allocate book-keeping data.
// 
//    Run your program with your allocations redirected through memento. 
//    When the program exits, you will get a list of all the leaked blocks, 
//    together with some helpful statistics. You can get the same list of
//    allocated blocks at any point during program execution by calling
//    list_blocks() (note: currently not exposed);
// 
//    Every call to 'malloc/free/realloc' counts as an 'allocation event'.
//    On each event Memento increments a counter. Every block is tagged with
//    the current counter on allocation. Every so often during program
//    execution, the heap is checked for consistency. By default this happens
//    every 1024 events. This could be changed at runtime by using 'paranoia'.
//    0 turns off such checking, 1 sets checking to happen on every event, 
//    any other number n sets checking to happen once every n events.
//    (note: currently not exposed)
// 
//    Memento keeps blocks around for a while after they have been freed, and
//    checks them as part of these heap checks to see if they have been
//    written to (or are freed twice etc).
// 
//    A given heap block can be checked for consistency (it's 'head' and
//    'tail' guard blocks are checked to see if they have been written to)
//    by calling check_block(void *ptr);
// 
//    A check of all the memory can be triggered by calling check();
//    (or check_all_memory(); if you'd like it to be quieter).
// 
//    A good place to breakpoint is breakpoint(), as this will then
//    trigger your debugger if an error is detected. This is done
//    automatically for debug windows builds.
//    (note: currently not exposed)
// 
//    If a block is found to be corrupt, information will be printed to the
//    console, including the address of the block, the size of the block,
//    the type of corruption, the number of the block and the event on which
//    it last passed a check for correctness.
// 
//    If you rerun, and initialize paranoidAt = event; with this number
//    the code will wait until it reaches that event and then start
//    checking the heap after every allocation event. Assuming it is a
//    deterministic failure, you should then find out where in your program
//    the error is occurring (between event x-1 and event x).
// 
//    Then you can rerun the program again, and set breakAt = event; 
//    and the program will call breakpoint() when event x is reached, 
//    enabling you to step through.
// 
//    find(address) will tell you what block (if any) the given address is in.
// 
// ----------------------------------------------------------------------------
// An example:
//    Suppose we have an invocation that crashes with memory corruption.
//     * With memento integrated.
//     * In your debugger put breakpoints on memento::initialized and
//       memento::breakpoint.
//     * Run the program. It will stop in memento::initialized.
//     * Execute paranoia = 1; (In VS use Ctrl-Alt-Q). (Note #1)
//     * Continue execution.
//     * It will detect the memory corruption on the next allocation event
//       after it happens, and stop in memento::breakpoint. The console should
//       show something like:
// 
//       Freed blocks:
//         0x172e610(size=288,num=1415) index 256 (0x172e710) onwards corrupted
//           Block last checked OK at sequence 1457. Now at sequence 1458.
// 
//     * This means that the block became corrupted between allocation 1457
//       and 1458 - so if we rerun and stop the program at 1457, we can then
//       step through, possibly with a data breakpoint at 0x172e710 and see
//       when it occurs.
//     * So restart the program from the beginning. When we hit memento::initialized
//       execute memento::breakAt = 1457; (and maybe memento::paranoia=1, or
//       memento::paranoidAt = 1457)
//     * Continue execution until we hit memento::breakpoint.
//     * Now you can step through and watch the memory corruption happen.
// 
//    Note #1: Using memento::paranoia=1 can cause your program to run
//    very slowly. You may instead choose to use memento::paranoia=100
//    (or some other figure). This will only exhaustively check memory on
//    every 100th allocation event. This trades speed for the size of the
//    average allocation event range in which detection of memory corruption
//    occurs. You may (for example) choose to run once checking every 100
//    allocations and discover that the corruption happens between events
//    X and X+100. You can then rerun using memento::paranoidAt=X, and
//    it'll only start exhaustively checking when it reaches X.
// 


//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	class x_memento_reporter
	{
	public:
		virtual void			print(const char* format, const char* str) = 0;
		virtual void			print(const char* format, void* ptr) = 0;
		virtual void			print(const char* format, int n, int value1, int value2 = 0, int value3 = 0, int value4 = 0) = 0;
	};


	/// Forward declare
	class x_memento;


	class x_memento_handler
	{
	public:
		virtual void			breakpoint(x_memento*) = 0;
	};


	class x_memento : public x_iallocator
	{
	public:
		virtual void*			allocate(xsize_t size, u32 alignment) = 0;
		virtual void*			reallocate(void* ptr, xsize_t size, u32 alignment) = 0;
		virtual void			deallocate(void* ptr) = 0;
		virtual void			release() = 0;

		virtual void*			allocate(xsize_t size, u32 alignment, const char* label, int var) = 0;

		virtual void			set_reporter(x_memento_reporter*) = 0;
		virtual void			set_handler(x_memento_handler*) = 0;

		virtual void			paranoia(int) = 0;
		virtual void			paranoidAt(int) = 0;
		virtual void			breakAt(int) = 0;

		virtual void			set_freelist(int maxkeep, int skipmin, int skipmax) = 0;

		virtual void			list_blocks() = 0;

		virtual void			check() = 0;
		virtual bool			check_block(void*) = 0;
		virtual void			check_all_memory() = 0;

		virtual void			break_on_free(void*) = 0;
	};

	x_memento*				gCreateMementoAllocator(x_iallocator* internal_mem_allocator);

};

#endif	/// __X_ALLOCATOR_MEMENTO_H__

