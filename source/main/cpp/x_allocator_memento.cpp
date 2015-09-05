#include "xallocator/x_allocator_memento.h"


#include <stdio.h>
#include <memory.h>
#include <alg.h>

/* Inspired by Fortify by Simon P Bullen. */

/* Don't keep blocks around if they'd mean losing more than a quarter of
 * the freelist. */
#define MEMENTO_FREELIST_MAX_SINGLE_BLOCK (MEMENTO_FREELIST_MAX/4)


namespace xcore
{
	/* How far to search for pointers in each block when calculating nestings */
#define MEMENTO_PTRSEARCH 65536

#ifndef MEMENTO_MAXPATTERN
#define MEMENTO_MAXPATTERN 0
#endif

	enum
	{
		Memento_PreSize = 16,
		Memento_PostSize = 16
	};

	enum
	{
		Memento_Flag_OldBlock = 1,
		Memento_Flag_HasParent = 2,
		Memento_Flag_BreakOnFree = 4,
		Memento_Flag_BreakOnRealloc = 8
	};


	/// When we list leaked blocks at the end of execution, we search for pointers
	/// between blocks in order to be able to give a nice nested view.
	/// Unfortunately, if you have are running your own allocator (such as
	/// ghostscripts chunk allocator) you can often find that the header of the
	/// block always contains pointers to next or previous blocks. This tends to
	/// mean the nesting displayed is "uninteresting" at best :)
	/// 
	/// As a hack to get around this, we have a define MEMENTO_SKIP_SEARCH that
	/// indicates how many bytes to skip over at the start of the chunk.
	/// This may cause us to miss true nestings, but such is life...s

	struct Memento_BlkHeader
	{
		unsigned int		rawsize;		/// This does limit allocations to < 4 GB
		int					sequence;
		int					lastCheckedOK;
		int					flags;
		Memento_BlkHeader*	next;
		Memento_BlkHeader*	parent;			/// Only used while printing out nested list

		const char*			label;

		/// Entries for nesting display calculations
		Memento_BlkHeader*	child;
		Memento_BlkHeader*	sibling;

		char				preblk[Memento_PreSize];
	};

	/// In future this could (should) be a smarter data structure, like, say,
	/// splay trees. For now, we use a list.
	struct Memento_Blocks
	{
		Memento_BlkHeader  *head;
		Memento_BlkHeader **tail;
	};

	class memento_functor
	{
	public:
		virtual int dofunc(Memento_BlkHeader *, void *) = 0;
	};

	/* And our global structure */
	struct memento_globals
	{
		int				inited;
		Memento_Blocks	used;
		Memento_Blocks	free;
		size_t			freeListSize;
		int				sequence;
		int				paranoia;
		int				paranoidAt;
		int				countdown;
		int				lastChecked;
		int				breakAt;
		int				failAt;
		int				failing;
		int				nextFailAt;
		int				squeezeAt;
		int				squeezing;
		int				segv;
		int				pattern;
		int				nextPattern;
		int				patternBit;
		size_t			maxMemory;
		size_t			alloc;
		size_t			peakAlloc;
		size_t			totalAlloc;
		size_t			numMallocs;
		size_t			numFrees;
		size_t			numReallocs;

		/// options - config

		// Set the following if you're only looking for leaks, not
		// memory overwrites to speed up the operation of memento.
		bool			leaksonly;
		int				searchskip;
	};

	enum econfig
	{
		MEMENTO_MAXALIGN = sizeof(u64),
		MEMENTO_PREFILL = 0xa6,
		MEMENTO_POSTFILL = 0xa7,
		MEMENTO_ALLOCFILL = 0xa8,
		MEMENTO_FREEFILL = 0xa9,
		MEMENTO_FREEFILL16 = 0xa9a9,
		MEMENTO_FREEFILL32 = 0xa9a9a9a9,
		MEMENTO_FREELIST_MAX = 0x2000000
	};

	struct memento_instance
	{
		memento_globals			globals;
		x_iallocator*			m_internal_mem_allocator;

		void					Memento_init(void);
		void					Memento_signal(void);

		void					Memento_startFailing(void);
		void					Memento_event(void);

		int						Memento_checkBlock(void *);
		int						Memento_checkAllMemory(void);
		int						Memento_check(void);

		int						Memento_setParanoia(int);
		int						Memento_paranoidAt(int);
		int						Memento_breakAt(int);
		void					Memento_breakOnFree(void *a);
		void					Memento_breakOnRealloc(void *a);
		int						Memento_getBlockNum(void *);
		int						Memento_find(void *a);
		void					Memento_breakpoint(void);
		int						Memento_failAt(int);
		int						Memento_failThisEvent(void);
		void					Memento_listBlocks(void);
		void					Memento_listNewBlocks(void);
		size_t					Memento_setMax(size_t);
		void					Memento_stats(void);
		void					Memento_endStats(void);
		void *					Memento_label(void *, const char *);

		void *					Memento_malloc(unsigned int s);
		void *					Memento_realloc(void *, unsigned int s);
		void					Memento_free(void *);

		void					Memento_addBlockHead(Memento_Blocks *blks, Memento_BlkHeader *b, int type);
		void					Memento_addBlockTail(Memento_Blocks *blks, Memento_BlkHeader *b, int type);
		int						Memento_Internal_checkAllocedBlock(Memento_BlkHeader *b, void *arg);
		int						Memento_Internal_checkFreedBlock(Memento_BlkHeader *b, void *arg);
		void					Memento_removeBlock(Memento_Blocks *blks, Memento_BlkHeader *b);
		int						checkBlock(Memento_BlkHeader *memblk, const char *action);

		int						Memento_Internal_makeSpace(size_t space);
		int						Memento_appBlocks(Memento_Blocks *blks, memento_functor& func, void *arg);
		int						Memento_appBlock(Memento_Blocks *blks, memento_functor& func, void *arg, Memento_BlkHeader *b);

		int						Memento_listBlocksNested(void);

		void					Memento_fin(void);

		int						Memento_Internal_checkAllFreed(Memento_BlkHeader *memblk, void *arg);
		int						Memento_Internal_checkAllAlloced(Memento_BlkHeader *memblk, void *arg);
	};

	#define MEMENTO_EXTRASIZE		(sizeof(Memento_BlkHeader) + Memento_PostSize)

	/// Round up size S to the next multiple of N (where N is a power of 2)
	#define MEMENTO_ROUNDUP(S,N)	((S + N-1)&~(N-1))

	#define MEMBLK_SIZE(s)			MEMENTO_ROUNDUP(s + MEMENTO_EXTRASIZE, MEMENTO_MAXALIGN)

	#define MEMBLK_FROMBLK(B)		(&((Memento_BlkHeader*)(void *)(B))[-1])
	#define MEMBLK_TOBLK(B)			((void*)(&((Memento_BlkHeader*)(void*)(B))[1]))
	#define MEMBLK_POSTPTR(B)		(&((char *)(void *)(B))[(B)->rawsize + sizeof(Memento_BlkHeader)])

	void Memento_breakpoint(void)
	{
		/// A handy externally visible function for breakpointing
	}

	void memento_instance::Memento_addBlockHead(Memento_Blocks *blks, Memento_BlkHeader *b, int type)
	{
		if (blks->tail == &blks->head)
		{
			/* Adding into an empty list, means the tail changes too */
			blks->tail = &b->next;
		}
		b->next = blks->head;
		blks->head = b;
		
		if (!globals.leaksonly)
		{
			memset(b->preblk, MEMENTO_PREFILL, Memento_PreSize);
			memset(MEMBLK_POSTPTR(b), MEMENTO_POSTFILL, Memento_PostSize);
		}
	}

	void memento_instance::Memento_addBlockTail(Memento_Blocks *blks, Memento_BlkHeader *b, int type)
	{
		*blks->tail = b;
		blks->tail = &b->next;
		b->next = NULL;
		if (!globals.leaksonly)
		{
			memset(b->preblk, MEMENTO_PREFILL, Memento_PreSize);
			memset(MEMBLK_POSTPTR(b), MEMENTO_POSTFILL, Memento_PostSize);
		}
	}

	struct BlkCheckData
	{
		int found;
		int preCorrupt;
		int postCorrupt;
		int freeCorrupt;
		int index;
	};

	int memento_instance::Memento_Internal_checkAllocedBlock(Memento_BlkHeader *b, void *arg)
	{
		if (!globals.leaksonly)
		{
			int           i;
			char         *p;
			int           corrupt = 0;
			BlkCheckData *data = (BlkCheckData *)arg;

			p = b->preblk;
			i = Memento_PreSize;
			do {
				corrupt |= (*p++ ^ (char)MEMENTO_PREFILL);
			} while (--i);
			if (corrupt) {
				data->preCorrupt = 1;
			}
			p = MEMBLK_POSTPTR(b);
			i = Memento_PreSize;
			do {
				corrupt |= (*p++ ^ (char)MEMENTO_POSTFILL);
			} while (--i);
			if (corrupt) {
				data->postCorrupt = 1;
			}
			if ((data->freeCorrupt | data->preCorrupt | data->postCorrupt) == 0) {
				b->lastCheckedOK = globals.sequence;
			}
			data->found |= 1;
		}
		return 0;
	}

	int memento_instance::Memento_Internal_checkFreedBlock(Memento_BlkHeader *b, void *arg)
	{
		if (!globals.leaksonly)
		{
			int           i;
			char         *p;
			BlkCheckData *data = (BlkCheckData *)arg;

			p = (char*)MEMBLK_TOBLK(b);
			i = b->rawsize;
			/* Attempt to speed this up by checking an (aligned) int at a time */
			do
			{
				if (((size_t)p) & 1)
				{
					if (*p++ != (char)MEMENTO_FREEFILL)
						break;
					i--;
					if (i == 0)
						break;
				}

				if ((i >= 2) && (((size_t)p) & 2))
				{
					short const b = *(short *)p;
					if (b != (short)MEMENTO_FREEFILL16)
						goto mismatch;
					p += 2;
					i -= 2;
					if (i == 0)
						break;
				}
				i -= 4;
				while (i >= 0)
				{
					int const b = *(int *)p;
					if (b != (int)MEMENTO_FREEFILL32)
						goto mismatch;
					p += 4;
					i -= 4;
				}
				i += 4;
				if ((i >= 2) && (((size_t)p) & 2))
				{
					short const b = *(short *)p;
					if (b != (short)MEMENTO_FREEFILL16)
						goto mismatch;
					p += 2;
					i -= 2;
				}
			mismatch:
				while (i)
				{
					if (*p++ != (char)MEMENTO_FREEFILL)
						break;
					i--;
				}
			} while (0);

			if (i)
			{
				data->freeCorrupt = 1;
				data->index = b->rawsize - i;
			}

			return Memento_Internal_checkAllocedBlock(b, arg);
		}
		else
		{
			return 0;
		}
	}

	void memento_instance::Memento_removeBlock(Memento_Blocks *blks, Memento_BlkHeader *b)
	{
		Memento_BlkHeader *head = blks->head;
		Memento_BlkHeader *prev = NULL;
		while ((head) && (head != b)) 
		{
			prev = head;
			head = head->next;
		}
		if (head == NULL) 
		{
			/* FAIL! Will have been reported to user earlier, so just exit. */
			return;
		}

		if (*blks->tail == head) 
		{
			/* Removing the tail of the list */
			if (prev == NULL) 
			{
				/* Which is also the head */
				blks->tail = &blks->head;
			}
			else 
			{
				/* Which isn't the head */
				blks->tail = &prev->next;
			}
		}

		if (prev == NULL) 
		{
			/* Removing from the head of the list */
			blks->head = head->next;
		}
		else 
		{
			/* Removing from not-the-head */
			prev->next = head->next;
		}
	}

	int memento_instance::Memento_Internal_makeSpace(size_t space)
	{
		/* If too big, it can never go on the freelist */
		if (space > MEMENTO_FREELIST_MAX_SINGLE_BLOCK)
			return 0;

		/* Pretend we added it on. */
		globals.freeListSize += space;
		/* Ditch blocks until it fits within our limit */
		while (globals.freeListSize > MEMENTO_FREELIST_MAX) 
		{
			Memento_BlkHeader *head = globals.free.head;
			globals.free.head = head->next;
			globals.freeListSize -= MEMBLK_SIZE(head->rawsize);
		}
		/* Make sure we haven't just completely emptied the free list */
		/* (This should never happen, but belt and braces... */
		if (globals.free.head == NULL)
			globals.free.tail = &globals.free.head;
		return 1;
	}

	int memento_instance::Memento_appBlocks(Memento_Blocks *blks, memento_functor& func, void *arg)
	{
		Memento_BlkHeader *head = blks->head;
		Memento_BlkHeader *next;
		int                result;
		while (head) 
		{
			result = func.dofunc(head, arg);
			next = head->next;
			if (result)
				return result;
			head = next;
		}
		return 0;
	}
	
	#define VALGRIND_MAKE_MEM_DEFINED(a,b)
	#define VALGRIND_MAKE_MEM_NOACCESS(a,b)
	#define VALGRIND_MAKE_MEM_UNDEFINED(a,b)

	int memento_instance::Memento_appBlock(Memento_Blocks *blks, memento_functor& func, void *arg, Memento_BlkHeader *b)
	{
		Memento_BlkHeader *head = blks->head;
		Memento_BlkHeader *next;
		int                result;
		while (head && head != b) 
		{
			VALGRIND_MAKE_MEM_DEFINED(head, sizeof(Memento_BlkHeader));
			next = head->next;
			VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_POSTPTR(head), Memento_PostSize);
			head = next;
		}
		if (head == b) 
		{
			VALGRIND_MAKE_MEM_DEFINED(head, sizeof(Memento_BlkHeader));
			VALGRIND_MAKE_MEM_DEFINED(MEMBLK_TOBLK(head), head->rawsize + Memento_PostSize);
			result = func.dofunc(head, arg);
			VALGRIND_MAKE_MEM_NOACCESS(MEMBLK_POSTPTR(head), Memento_PostSize);
			VALGRIND_MAKE_MEM_NOACCESS(head, sizeof(Memento_BlkHeader));
			return result;
		}
		return 0;
	}

	static void showBlock(Memento_BlkHeader *b, int space)
	{
		fprintf(stderr, "0x%p:(size=%d,num=%d)", MEMBLK_TOBLK(b), (int)b->rawsize, b->sequence);
		if (b->label)
			fprintf(stderr, "%c(%s)", space, b->label);
	}

	static void blockDisplay(Memento_BlkHeader *b, int n)
	{
		n++;
		while (n > 40)
		{
			fprintf(stderr, "*");
			n -= 40;
		}
		while (n > 0)
		{
			int i = n;
			if (i > 32)
				i = 32;
			n -= i;
			fprintf(stderr, "%s", &"                                "[32 - i]);
		}
		showBlock(b, '\t');
		fprintf(stderr, "\n");
	}

	static int Memento_listBlock(Memento_BlkHeader *b, void *arg)
	{
		int *counts = (int *)arg;
		blockDisplay(b, 0);
		counts[0]++;
		counts[1] += b->rawsize;
		return 0;
	}

	static int Memento_listNewBlock(Memento_BlkHeader *b, void *arg)
	{
		if (b->flags & Memento_Flag_OldBlock)
			return 0;
		b->flags |= Memento_Flag_OldBlock;
		return Memento_listBlock(b, arg);
	}

	class memento_listnewblock_functor : public memento_functor
	{
	public:
		inline		memento_listnewblock_functor() {}

		virtual int dofunc(Memento_BlkHeader * blkh, void * arg)
		{
			return Memento_listNewBlock(blkh, arg);
		}
	};

	static void doNestedDisplay(Memento_BlkHeader *b,
		int depth)
	{
		/* Try and avoid recursion if we can help it */
		do {
			blockDisplay(b, depth);
			if (b->sibling) {
				if (b->child)
					doNestedDisplay(b->child, depth + 1);
				b = b->sibling;
			}
			else {
				b = b->child;
				depth++;
			}
		} while (b);
	}

	static int ptrcmp(const void *a_, const void *b_)
	{
		const char **a = (const char **)a_;
		const char **b = (const char **)b_;
		return (int)(*a - *b);
	}

	#define MEMENTO_UNDERLYING_MALLOC(size)	m_internal_mem_allocator->allocate(size, 4)
	#define MEMENTO_UNDERLYING_FREE(ptr) m_internal_mem_allocator->deallocate(ptr)
	#define MEMENTO_UNDERLYING_REALLOC(ptr, size) m_internal_mem_allocator->reallocate(ptr, size, 4)

	int memento_instance::Memento_listBlocksNested(void)
	{
		int count, size, i;
		Memento_BlkHeader *b;
		void **blocks, *minptr, *maxptr;
		long mask;

		/* Count the blocks */
		count = 0;
		size = 0;
		for (b = globals.used.head; b; b = b->next) 
		{
			VALGRIND_MAKE_MEM_DEFINED(b, sizeof(*b));
			size += b->rawsize;
			count++;
		}

		/* Make our block list */
		blocks = (void**)MEMENTO_UNDERLYING_MALLOC(sizeof(void *) * count);
		if (blocks == NULL)
			return 1;

		/* Populate our block list */
		b = globals.used.head;
		minptr = maxptr = MEMBLK_TOBLK(b);
		mask = (long)minptr;
		for (i = 0; b; b = b->next, i++) 
		{
			void *p = MEMBLK_TOBLK(b);
			mask &= (long)p;
			if (p < minptr)
				minptr = p;
			if (p > maxptr)
				maxptr = p;
			blocks[i] = p;
			b->flags &= ~Memento_Flag_HasParent;
			b->child = NULL;
			b->sibling = NULL;
			b->parent = NULL;
		}
		qsort(blocks, count, sizeof(void *), ptrcmp);

		/* Now, calculate tree */
		for (b = globals.used.head; b; b = b->next) 
		{
			char *p = (char*)MEMBLK_TOBLK(b);
			int end = (b->rawsize < MEMENTO_PTRSEARCH ? b->rawsize : MEMENTO_PTRSEARCH);
			for (i = globals.searchskip; i < end; i += sizeof(void *)) 
			{
				void *q = *(void **)(&p[i]);
				void **r;

				/* Do trivial checks on pointer */
				if ((mask & (int)q) != mask || q < minptr || q > maxptr)
					continue;

				/* Search for pointer */
				r = (void**)bsearch(&q, blocks, count, sizeof(void *), ptrcmp);
				if (r) 
				{
					/* Found child */
					Memento_BlkHeader *child = MEMBLK_FROMBLK(*r);
					Memento_BlkHeader *parent;

					/* We're assuming tree structure, not graph - ignore second
					 * and subsequent pointers. */
					if (child->parent != NULL)
						continue;
					if (child->flags & Memento_Flag_HasParent)
						continue;

					/* Not interested in pointers to ourself! */
					if (child == b)
						continue;

					/* We're also assuming acyclicness here. If this is one of
					 * our parents, ignore it. */
					parent = b->parent;
					while (parent != NULL && parent != child)
						parent = parent->parent;
					if (parent == child)
						continue;

					child->sibling = b->child;
					b->child = child;
					child->parent = b;
					child->flags |= Memento_Flag_HasParent;
				}
			}
		}

		/* Now display with nesting */
		for (b = globals.used.head; b; b = b->next) 
		{
			if ((b->flags & Memento_Flag_HasParent) == 0)
				doNestedDisplay(b, 0);
		}
		fprintf(stderr, " Total number of blocks = %d\n", count);
		fprintf(stderr, " Total size of blocks = %d\n", size);

		MEMENTO_UNDERLYING_FREE(blocks);

		/* Now put the blocks back for valgrind */
		for (b = globals.used.head; b;) 
		{
			Memento_BlkHeader *next = b->next;
			VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(*b));
			b = next;
		}

		return 0;
	}


	void memento_instance::Memento_listBlocks(void)
	{
		fprintf(stderr, "Allocated blocks:\n");
		if (Memento_listBlocksNested())
		{
			int counts[2];
			counts[0] = 0;
			counts[1] = 0;
			memento_listnewblock_functor f;
			Memento_appBlocks(&globals.used, f, &counts[0]);
			fprintf(stderr, " Total number of blocks = %d\n", counts[0]);
			fprintf(stderr, " Total size of blocks = %d\n", counts[1]);
		}
	}

	void memento_instance::Memento_listNewBlocks(void)
	{
		int counts[2];
		counts[0] = 0;
		counts[1] = 0;
		fprintf(stderr, "Blocks allocated and still extant since last list:\n");
		memento_listnewblock_functor f;
		Memento_appBlocks(&globals.used, f, &counts[0]);
		fprintf(stderr, "  Total number of blocks = %d\n", counts[0]);
		fprintf(stderr, "  Total size of blocks = %d\n", counts[1]);
	}

	void memento_instance::Memento_endStats(void)
	{
		fprintf(stderr, "Total memory malloced = %u bytes\n", (unsigned int)globals.totalAlloc);
		fprintf(stderr, "Peak memory malloced = %u bytes\n", (unsigned int)globals.peakAlloc);
		fprintf(stderr, "%u mallocs, %u frees, %u reallocs\n", (unsigned int)globals.numMallocs, (unsigned int)globals.numFrees, (unsigned int)globals.numReallocs);
		fprintf(stderr, "Average allocation size %u bytes\n", (unsigned int)(globals.numMallocs != 0 ? globals.totalAlloc / globals.numMallocs : 0));
	}

	void memento_instance::Memento_stats(void)
	{
		fprintf(stderr, "Current memory malloced = %u bytes\n", (unsigned int)globals.alloc);
		Memento_endStats();
	}

	void memento_instance::Memento_fin(void)
	{
		Memento_checkAllMemory();
		Memento_endStats();

		if (globals.used.head != NULL) 
		{
			Memento_listBlocks();
			Memento_breakpoint();
		}

		if (globals.segv) 
		{
			fprintf(stderr, "Memory dumped on SEGV while squeezing @ %d\n", globals.failAt);
		}
		else if (globals.squeezing)
		{
			if (globals.pattern == 0)
				fprintf(stderr, "Memory squeezing @ %d complete\n", globals.squeezeAt);
			else
				fprintf(stderr, "Memory squeezing @ %d (%d) complete\n", globals.squeezeAt, globals.pattern);
		}

		if (globals.failing)
		{
			fprintf(stderr, "MEMENTO_FAILAT=%d\n", globals.failAt);
			fprintf(stderr, "MEMENTO_PATTERN=%d\n", globals.pattern);
		}
		
		if (globals.nextFailAt != 0)
		{
			fprintf(stderr, "MEMENTO_NEXTFAILAT=%d\n", globals.nextFailAt);
			fprintf(stderr, "MEMENTO_NEXTPATTERN=%d\n", globals.nextPattern);
		}
	}

	static void Memento_inited(void)
	{
		/* A good place for a breakpoint */
	}

	void memento_instance::Memento_init(void)
	{
		memset(&globals, 0, sizeof(globals));

		globals.inited = 1;
		globals.used.head = NULL;
		globals.used.tail = &globals.used.head;
		globals.free.head = NULL;
		globals.free.tail = &globals.free.head;
		globals.sequence = 0;
		globals.countdown = 1024;

		//@JURGEN
		//atexit(Memento_fin);

		Memento_inited();
	}

	#include <signal.h>

	void memento_instance::Memento_signal(void)
	{
		globals.segv = 1;

		/* If we just return from this function the SEGV will be unhandled, and
		 * we'll launch into whatever JIT debugging system the OS provides. At
		 * least fprintf(stderr, something useful first. If MEMENTO_NOJIT is set, then
		 * just exit to avoid the JIT (and get the usual atexit handling). */

		Memento_fin();
	}

	void memento_instance::Memento_startFailing(void)
	{
		if (!globals.failing) 
		{
			fprintf(stderr, "Starting to fail...\n");
			fflush(stderr);
			globals.failing = 1;
			globals.failAt = globals.sequence;
			globals.nextFailAt = globals.sequence + 1;
			globals.pattern = 0;
			globals.patternBit = 0;
			//signal(SIGSEGV, &xcore::memento_instance::Memento_signal);
			//signal(SIGABRT, Memento_signal);
			Memento_breakpoint();
		}
	}

	void memento_instance::Memento_event(void)
	{
		globals.sequence++;
		if ((globals.sequence >= globals.paranoidAt) && (globals.paranoidAt != 0)) 
		{
			globals.paranoia = 1;
			globals.countdown = 1;
		}
		if (--globals.countdown == 0) 
		{
			Memento_checkAllMemory();
			globals.countdown = globals.paranoia;
		}

		if (globals.sequence == globals.breakAt) 
		{
			fprintf(stderr, "Breaking at event %d\n", globals.breakAt);
			Memento_breakpoint();
		}
	}

	int memento_instance::Memento_breakAt(int event)
	{
		globals.breakAt = event;
		return event;
	}

	void * memento_instance::Memento_label(void *ptr, const char *label)
	{
		Memento_BlkHeader *block;

		if (ptr == NULL)
			return NULL;
		block = MEMBLK_FROMBLK(ptr);
		VALGRIND_MAKE_MEM_DEFINED(&block->label, sizeof(block->label));
		block->label = label;
		VALGRIND_MAKE_MEM_UNDEFINED(&block->label, sizeof(block->label));
		return ptr;
	}

	int memento_instance::Memento_failThisEvent(void)
	{
		int failThisOne;

		if (!globals.inited)
			Memento_init();

		Memento_event();

		if ((globals.sequence >= globals.failAt) && (globals.failAt != 0))
			Memento_startFailing();

		if (!globals.failing)
			return 0;

		failThisOne = ((globals.patternBit & globals.pattern) == 0);

		/* If we are failing, and we've reached the end of the pattern and we've
		 * still got bits available in the pattern word, and we haven't already
		 * set a nextPattern, then extend the pattern. */
		if (globals.failing && ((~(globals.patternBit - 1) & globals.pattern) == 0) && (globals.patternBit != 0) && globals.nextPattern == 0)
		{
			/* We'll fail this one, and set the 'next' one to pass it. */
			globals.nextFailAt = globals.failAt;
			globals.nextPattern = globals.pattern | globals.patternBit;
		}

		globals.patternBit = (globals.patternBit ? globals.patternBit << 1 : 1);

		return failThisOne;
	}

	void * memento_instance::Memento_malloc(unsigned int s)
	{
		Memento_BlkHeader *memblk;
		size_t             smem = MEMBLK_SIZE(s);

		if (Memento_failThisEvent())
			return NULL;

		if (s == 0)
			return NULL;

		globals.numMallocs++;

		if (globals.maxMemory != 0 && globals.alloc + s > globals.maxMemory)
			return NULL;

		memblk = (Memento_BlkHeader*)MEMENTO_UNDERLYING_MALLOC(smem);
		if (memblk == NULL)
			return NULL;

		globals.alloc += s;
		globals.totalAlloc += s;
		if (globals.peakAlloc < globals.alloc)
			globals.peakAlloc = globals.alloc;

		if (!globals.leaksonly)
		{
			memset(MEMBLK_TOBLK(memblk), MEMENTO_ALLOCFILL, s);
		}

		memblk->rawsize = s;
		memblk->sequence = globals.sequence;
		memblk->lastCheckedOK = memblk->sequence;
		memblk->flags = 0;
		memblk->label = 0;
		memblk->child = NULL;
		memblk->sibling = NULL;
		Memento_addBlockHead(&globals.used, memblk, 0);

		return MEMBLK_TOBLK(memblk);
	}

	class memento_checkallocedblock_functor : public memento_functor
	{
		memento_instance* m_instance;
	public:
		memento_checkallocedblock_functor(memento_instance* instance) : m_instance(instance) { }
		virtual int dofunc(Memento_BlkHeader * blkh, void * arg)
		{
			return m_instance->Memento_Internal_checkAllocedBlock(blkh, arg);
		}
	};

	int memento_instance::checkBlock(Memento_BlkHeader *memblk, const char *action)
	{
		if (!globals.leaksonly)
		{
			BlkCheckData data;
			memento_checkallocedblock_functor f(this);

			memset(&data, 0, sizeof(data));
			Memento_appBlock(&globals.used, f, &data, memblk);
			if (!data.found) {
				/* Failure! */
				fprintf(stderr, "Attempt to %s block ", action);
				showBlock(memblk, 32);
				fprintf(stderr, "\n");
				Memento_breakpoint();
				return 1;
			}
			else if (data.preCorrupt || data.postCorrupt) {
				fprintf(stderr, "Block ");
				showBlock(memblk, ' ');
				fprintf(stderr, " found to be corrupted on %s!\n", action);
				if (data.preCorrupt) {
					fprintf(stderr, "Preguard corrupted\n");
				}
				if (data.postCorrupt) {
					fprintf(stderr, "Postguard corrupted\n");
				}
				fprintf(stderr, "Block last checked OK at allocation %d. Now %d.\n",
					memblk->lastCheckedOK, globals.sequence);
				Memento_breakpoint();
				return 1;
			}
		}
		return 0;
	}

	void memento_instance::Memento_free(void *blk)
	{
		Memento_BlkHeader *memblk;

		if (!globals.inited)
			Memento_init();

		Memento_event();

		if (blk == NULL)
			return;

		memblk = MEMBLK_FROMBLK(blk);
		VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
		if (checkBlock(memblk, "free"))
			return;

		VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
		if (memblk->flags & Memento_Flag_BreakOnFree)
			Memento_breakpoint();

		globals.alloc -= memblk->rawsize;
		globals.numFrees++;

		Memento_removeBlock(&globals.used, memblk);

		VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
		if (Memento_Internal_makeSpace(MEMBLK_SIZE(memblk->rawsize))) 
		{
			VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
			VALGRIND_MAKE_MEM_DEFINED(MEMBLK_TOBLK(memblk),	memblk->rawsize + Memento_PostSize);
			if (!globals.leaksonly)
			{
				memset(MEMBLK_TOBLK(memblk), MEMENTO_FREEFILL, memblk->rawsize);
			}
			Memento_addBlockTail(&globals.free, memblk, 1);
		}
		else 
		{
			MEMENTO_UNDERLYING_FREE(memblk);
		}
	}

	void * memento_instance::Memento_realloc(void *blk, unsigned int newsize)
	{
		Memento_BlkHeader *memblk, *newmemblk;
		size_t             newsizemem;
		int                flags;

		if (blk == NULL)
			return Memento_malloc(newsize);
		if (newsize == 0) {
			Memento_free(blk);
			return NULL;
		}

		if (Memento_failThisEvent())
			return NULL;

		memblk = MEMBLK_FROMBLK(blk);
		if (checkBlock(memblk, "realloc"))
			return NULL;

		VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
		if (memblk->flags & Memento_Flag_BreakOnRealloc)
			Memento_breakpoint();

		VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
		if (globals.maxMemory != 0 && globals.alloc - memblk->rawsize + newsize > globals.maxMemory)
			return NULL;

		newsizemem = MEMBLK_SIZE(newsize);
		Memento_removeBlock(&globals.used, memblk);
		VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
		flags = memblk->flags;
		newmemblk = (Memento_BlkHeader*)MEMENTO_UNDERLYING_REALLOC(memblk, newsizemem);
		if (newmemblk == NULL)
		{
			Memento_addBlockHead(&globals.used, memblk, 2);
			return NULL;
		}
		globals.numReallocs++;
		globals.totalAlloc += newsize;
		globals.alloc -= newmemblk->rawsize;
		globals.alloc += newsize;
		if (globals.peakAlloc < globals.alloc)
			globals.peakAlloc = globals.alloc;

		newmemblk->flags = flags;
		if (newmemblk->rawsize < newsize) 
		{
			char *newbytes = ((char *)MEMBLK_TOBLK(newmemblk)) + newmemblk->rawsize;
			VALGRIND_MAKE_MEM_DEFINED(newbytes, newsize - newmemblk->rawsize);
			if (!globals.leaksonly)
			{
				memset(newbytes, MEMENTO_ALLOCFILL, newsize - newmemblk->rawsize);
			}
			VALGRIND_MAKE_MEM_UNDEFINED(newbytes, newsize - newmemblk->rawsize);
		}
		newmemblk->rawsize = newsize;
		if (!globals.leaksonly)
		{
			VALGRIND_MAKE_MEM_DEFINED(newmemblk->preblk, Memento_PreSize);
			memset(newmemblk->preblk, MEMENTO_PREFILL, Memento_PreSize);
			VALGRIND_MAKE_MEM_UNDEFINED(newmemblk->preblk, Memento_PreSize);
			VALGRIND_MAKE_MEM_DEFINED(MEMBLK_POSTPTR(newmemblk), Memento_PostSize);
			memset(MEMBLK_POSTPTR(newmemblk), MEMENTO_POSTFILL, Memento_PostSize);
			VALGRIND_MAKE_MEM_UNDEFINED(MEMBLK_POSTPTR(newmemblk), Memento_PostSize);
		}
		Memento_addBlockHead(&globals.used, newmemblk, 2);
		return MEMBLK_TOBLK(newmemblk);
	}

	int memento_instance::Memento_checkBlock(void *blk)
	{
		Memento_BlkHeader *memblk;

		if (blk == NULL)
			return 0;
		memblk = MEMBLK_FROMBLK(blk);
		return checkBlock(memblk, "check");
	}

	int memento_instance::Memento_Internal_checkAllAlloced(Memento_BlkHeader *memblk, void *arg)
	{
		BlkCheckData *data = (BlkCheckData *)arg;

		Memento_Internal_checkAllocedBlock(memblk, data);
		if (data->preCorrupt || data->postCorrupt) {
			if ((data->found & 2) == 0) {
				fprintf(stderr, "Allocated blocks:\n");
				data->found |= 2;
			}
			fprintf(stderr, "  Block ");
			showBlock(memblk, ' ');
			if (data->preCorrupt) {
				fprintf(stderr, " Preguard ");
			}
			if (data->postCorrupt) {
				fprintf(stderr, "%s Postguard ",
					(data->preCorrupt ? "&" : ""));
			}
			fprintf(stderr, "corrupted.\n    "
				"Block last checked OK at allocation %d. Now %d.\n",
				memblk->lastCheckedOK, globals.sequence);
			data->preCorrupt = 0;
			data->postCorrupt = 0;
			data->freeCorrupt = 0;
		}
		else
			memblk->lastCheckedOK = globals.sequence;
		return 0;
	}

	int memento_instance::Memento_Internal_checkAllFreed(Memento_BlkHeader *memblk, void *arg)
	{
		BlkCheckData *data = (BlkCheckData *)arg;

		Memento_Internal_checkFreedBlock(memblk, data);
		if (data->preCorrupt || data->postCorrupt || data->freeCorrupt) {
			if ((data->found & 4) == 0) {
				fprintf(stderr, "Freed blocks:\n");
				data->found |= 4;
			}
			fprintf(stderr, "  ");
			showBlock(memblk, ' ');
			if (data->freeCorrupt) {
				fprintf(stderr, " index %d (address 0x%p) onwards", data->index,
					&((char *)MEMBLK_TOBLK(memblk))[data->index]);
				if (data->preCorrupt) {
					fprintf(stderr, "+ preguard");
				}
				if (data->postCorrupt) {
					fprintf(stderr, "+ postguard");
				}
			}
			else {
				if (data->preCorrupt) {
					fprintf(stderr, " preguard");
				}
				if (data->postCorrupt) {
					fprintf(stderr, "%s Postguard",
						(data->preCorrupt ? "+" : ""));
				}
			}
			fprintf(stderr, " corrupted.\n"
				"    Block last checked OK at allocation %d. Now %d.\n",
				memblk->lastCheckedOK, globals.sequence);
			data->preCorrupt = 0;
			data->postCorrupt = 0;
			data->freeCorrupt = 0;
		}
		else
			memblk->lastCheckedOK = globals.sequence;
		return 0;
	}

	class memento_checkallalloced_functor : public memento_functor
	{
		memento_instance* m_instance;
	public:
		memento_checkallalloced_functor(memento_instance* instance) : m_instance(instance) { }
		virtual int dofunc(Memento_BlkHeader * blkh, void * arg)
		{
			return m_instance->Memento_Internal_checkAllAlloced(blkh, arg);
		}
	};

	class memento_checkallfreed_functor : public memento_functor
	{
		memento_instance* m_instance;
	public:
		memento_checkallfreed_functor(memento_instance* instance) : m_instance(instance) { }
		virtual int dofunc(Memento_BlkHeader * blkh, void * arg)
		{
			return m_instance->Memento_Internal_checkAllFreed(blkh, arg);
		}
	};

	int memento_instance::Memento_checkAllMemory(void)
	{
		if (!globals.leaksonly)
		{
			BlkCheckData data;

			memset(&data, 0, sizeof(data));
			memento_checkallalloced_functor checkAllAlloced(this);
			Memento_appBlocks(&globals.used, checkAllAlloced, &data);
			memento_checkallfreed_functor checkAllFreed(this);
			Memento_appBlocks(&globals.free, checkAllFreed, &data);
			if (data.found & 6) {
				Memento_breakpoint();
				return 1;
			}
		}
		return 0;
	}

	int memento_instance::Memento_setParanoia(int i)
	{
		globals.paranoia = i;
		globals.countdown = globals.paranoia;
		return i;
	}

	int memento_instance::Memento_paranoidAt(int i)
	{
		globals.paranoidAt = i;
		return i;
	}

	int memento_instance::Memento_getBlockNum(void *b)
	{
		Memento_BlkHeader *memblk;
		if (b == NULL)
			return 0;
		memblk = MEMBLK_FROMBLK(b);
		return (memblk->sequence);
	}

	int memento_instance::Memento_check(void)
	{
		int result;

		fprintf(stderr, "Checking memory\n");
		result = Memento_checkAllMemory();
		fprintf(stderr, "Memory checked!\n");
		return result;
	}

	struct findBlkData
	{
		void              *addr;
		Memento_BlkHeader *blk;
		int                flags;
	};

	static int Memento_containsAddr(Memento_BlkHeader *b, void *arg)
	{
		findBlkData *data = (findBlkData *)arg;
		char *blkend = &((char *)MEMBLK_TOBLK(b))[b->rawsize];
		if ((MEMBLK_TOBLK(b) <= data->addr) && ((void *)blkend > data->addr)) 
		{
			data->blk = b;
			data->flags = 1;
			return 1;
		}
		if (((void *)b <= data->addr) && (MEMBLK_TOBLK(b) > data->addr)) 
		{
			data->blk = b;
			data->flags = 2;
			return 1;
		}
		if (((void *)blkend <= data->addr) && ((void *)(blkend + Memento_PostSize) > data->addr)) 
		{
			data->blk = b;
			data->flags = 3;
			return 1;
		}
		return 0;
	}

	class memento_containsaddr_functor : public memento_functor
	{
	public:
		inline		memento_containsaddr_functor() { }
		virtual int dofunc(Memento_BlkHeader * blkh, void * arg)
		{
			return Memento_containsAddr(blkh, arg);
		}
	};


	int memento_instance::Memento_find(void *a)
	{
		findBlkData data;

		data.addr = a;
		data.blk = NULL;
		data.flags = 0;
		memento_containsaddr_functor containsAddr;
		Memento_appBlocks(&globals.used, containsAddr, &data);
		if (data.blk != NULL) 
		{
			fprintf(stderr, "Address 0x%p is in %sallocated block ",
				data.addr,
				(data.flags == 1 ? "" : (data.flags == 2 ?
				"preguard of " : "postguard of ")));
			showBlock(data.blk, ' ');
			fprintf(stderr, "\n");
			return data.blk->sequence;
		}
		data.blk = NULL;
		data.flags = 0;
		Memento_appBlocks(&globals.free, containsAddr, &data);
		if (data.blk != NULL) 
		{
			fprintf(stderr, "Address 0x%p is in %sfreed block ",
				data.addr,
				(data.flags == 1 ? "" : (data.flags == 2 ?
				"preguard of " : "postguard of ")));
			showBlock(data.blk, ' ');
			fprintf(stderr, "\n");
			return data.blk->sequence;
		}
		return 0;
	}

	void memento_instance::Memento_breakOnFree(void *a)
	{
		findBlkData data;
		memento_containsaddr_functor containsAddr;

		data.addr = a;
		data.blk = NULL;
		data.flags = 0;
		Memento_appBlocks(&globals.used, containsAddr, &data);
		if (data.blk != NULL) 
		{
			fprintf(stderr, "Will stop when address 0x%p (in %sallocated block ",
				data.addr,
				(data.flags == 1 ? "" : (data.flags == 2 ?
				"preguard of " : "postguard of ")));
			showBlock(data.blk, ' ');
			fprintf(stderr, ") is freed\n");
			data.blk->flags |= Memento_Flag_BreakOnFree;
			return;
		}
		data.blk = NULL;
		data.flags = 0;
		Memento_appBlocks(&globals.free, containsAddr, &data);
		if (data.blk != NULL) 
		{
			fprintf(stderr, "Can't stop on free; address 0x%p is in %sfreed block ",
				data.addr,
				(data.flags == 1 ? "" : (data.flags == 2 ?
				"preguard of " : "postguard of ")));
			showBlock(data.blk, ' ');
			fprintf(stderr, "\n");
			return;
		}
		fprintf(stderr, "Can't stop on free; address 0x%p is not in a known block.\n", a);
	}

	void memento_instance::Memento_breakOnRealloc(void *a)
	{
		findBlkData data;
		memento_containsaddr_functor containsAddr;

		data.addr = a;
		data.blk = NULL;
		data.flags = 0;
		Memento_appBlocks(&globals.used, containsAddr, &data);
		if (data.blk != NULL) {
			fprintf(stderr, "Will stop when address 0x%p (in %sallocated block ",
				data.addr,
				(data.flags == 1 ? "" : (data.flags == 2 ?
				"preguard of " : "postguard of ")));
			showBlock(data.blk, ' ');
			fprintf(stderr, ") is freed (or realloced)\n");
			data.blk->flags |= Memento_Flag_BreakOnFree | Memento_Flag_BreakOnRealloc;
			return;
		}
		data.blk = NULL;
		data.flags = 0;
		Memento_appBlocks(&globals.free, containsAddr, &data);
		if (data.blk != NULL) {
			fprintf(stderr, "Can't stop on free/realloc; address 0x%p is in %sfreed block ",
				data.addr,
				(data.flags == 1 ? "" : (data.flags == 2 ?
				"preguard of " : "postguard of ")));
			showBlock(data.blk, ' ');
			fprintf(stderr, "\n");
			return;
		}
		fprintf(stderr, "Can't stop on free/realloc; address 0x%p is not in a known block.\n", a);
	}

	int memento_instance::Memento_failAt(int i)
	{
		globals.failAt = i;
		if ((globals.sequence > globals.failAt) && (globals.failing != 0))
			Memento_startFailing();
		return i;
	}

	size_t memento_instance::Memento_setMax(size_t max)
	{
		globals.maxMemory = max;
		return max;
	}

}
