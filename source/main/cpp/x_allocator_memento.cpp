#include "xallocator/x_allocator_memento.h"

#include <memory.h>
#include <alg.h>

/// ! Inspired by Fortify - Simon P Bullen.

namespace xcore
{
	namespace memento
	{
		enum
		{
			Flag_OldBlock = 1,
			Flag_HasParent = 2,
			Flag_BreakOnFree = 4,
			Flag_BreakOnRealloc = 8
		};

		/// When we list leaked blocks at the end of execution, we search for pointers
		/// between blocks in order to be able to give a nice nested view.
		/// Unfortunately, if you have are running your own allocator (such as
		/// custom chunk allocator) you can often find that the header of the
		/// block always contains pointers to next or previous blocks. This tends to
		/// mean the nesting displayed is "uninteresting" at best :)
		/// 
		/// As a hack to get around this, we have a define 'skipsearch' that
		/// indicates how many bytes to skip over at the start of the chunk.
		/// This may cause us to miss true nestings, but such is life... :-)

		struct BlockHeader
		{
			unsigned int		allocsize;		/// This does limit allocations to < 4 GB
			unsigned int		allocalign;
			unsigned int		blksize;

			int					sequence;		/// = global sequence number at the moment of allocation
			int					lastOk;
			int					flags;

			BlockHeader*		next;
			BlockHeader*		parent;			/// Only used while printing out nested list

			const char*			label;

			/// Entries for nesting display calculations
			BlockHeader*		child;
			BlockHeader*		sibling;

			enum
			{
				HeadGuardSize = 16,
				TailGuardSize = 16
			};

			inline u8*			headguard() const { return ((u8*)this + sizeof(BlockHeader)); }
			inline u8*			tailguard() const { return ((u8*)this + sizeof(BlockHeader) + HeadGuardSize + blksize); }
			inline void*		toblock() const { return ((u8*)this + sizeof(BlockHeader)) + HeadGuardSize; }
			inline void*		toalloc() const
			{
				u32 const header_size = (((sizeof(BlockHeader) + HeadGuardSize) + (allocalign - 1)) / allocalign) * allocalign;
				return ((u8*)this + sizeof(BlockHeader) + HeadGuardSize - header_size);
			}

			inline int			headguard_size() const { return HeadGuardSize; }
			inline int			tailguard_size() const
			{
				u32 const ablksize = ((blksize + (TailGuardSize - 1)) & ~(TailGuardSize - 1));
				return (ablksize - blksize) + TailGuardSize;
			}

			static inline u32	s_allocsize(u32 size, u32 alignment)
			{
				/// How many times 'alignment' our block header is, remember we need to keep the block at the requested alignment.
				u32 const header_size = (((sizeof(BlockHeader) + HeadGuardSize) + (alignment - 1)) / alignment) * alignment;
				u32 const footer_size = TailGuardSize;
				u32 const asize = ((size + (TailGuardSize - 1)) & ~(TailGuardSize - 1));
				u32 total_size = header_size + asize + footer_size;
				return total_size;
			}

			static inline void*	s_alloc2block(void* alloc, u32 size, u32 alignment)
			{
				u32 const header_size = (((sizeof(BlockHeader) + HeadGuardSize) + (alignment - 1)) / alignment) * alignment;
				void* blk = (u8*)alloc + header_size;
				return blk;
			}

			static inline BlockHeader* s_blocktoheader(void* blk)
			{
				BlockHeader* blkheader = (BlockHeader*)((u8*)blk - HeadGuardSize - sizeof(BlockHeader));
				return blkheader;
			}

		};

		/// In future this could (should) be a smarter data structure, like, say,
		/// splay trees or hash-table. For now, we use a list.
		struct BlockHeaderList
		{
			BlockHeader  *head;
			BlockHeader **tail;
		};

		class Functor
		{
		public:
			virtual int dofunc(BlockHeader *, void *) = 0;
		};

		/* And our global structure */
		struct Globals
		{
			int				inited;
			BlockHeaderList	used;
			BlockHeaderList	free;
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

		class NullPrint : public x_memento_report
		{
		public:
			virtual void			print(const char* format, const char* str) { }
			virtual void			print(const char* format, void* ptr) { }
			virtual void			print(const char* format, int n, int value1, int value2 = 0, int value3 = 0, int value4 = 0) { }
		};

		struct Instance
		{
			Globals					m_globals;
			NullPrint				m_stdout_null;

			x_iallocator*			m_internal_mem_allocator;
			x_memento_config		m_config;

			void					initialize(x_memento_config const& config);

			void					start_failing(void);
			void					signal(void);
			void					event(void);

			int						check_block(void *);
			int						check_all_memory(void);
			int						check(void);

			int						set_paranoia(int);
			int						paranoid_at(int);
			int						break_at(int);
			void					break_on_free(void *a);
			void					break_on_realloc(void *a);
			int						get_blocknum(void *);
			int						find(void *a);
			void					breakpoint(void);
			int						fail_at(int);
			int						fail_this_event(void);
			void					list_blocks(void);
			void					list_newblocks(void);
			size_t					set_maxmemory(size_t);
			void					stats(void);
			void					end_stats(void);
			void *					label(void *, const char *);

			void *					mmalloc(unsigned int s, unsigned int align);
			void *					mrealloc(void *, unsigned int s);
			void					mfree(void *);

			void					add_blockhead(BlockHeaderList *blks, BlockHeader *b, int type);
			void					add_blocktail(BlockHeaderList *blks, BlockHeader *b, int type);
			int						internal_check_allocedblock(BlockHeader *b, void *arg);
			int						internal_check_freedblock(BlockHeader *b, void *arg);
			void					remove_block(BlockHeaderList *blks, BlockHeader *b);
			int						check_block_action(BlockHeader *memblk, const char *action);

			int						internal_makespace(size_t space);
			int						app_blocks(BlockHeaderList *blks, Functor& func, void *arg);
			int						app_block(BlockHeaderList *blks, Functor& func, void *arg, BlockHeader *b);

			int						list_blocks_nested(void);

			void					finalize(void);

			int						internal_check_allfreed(BlockHeader *memblk, void *arg);
			int						internal_check_allalloced(BlockHeader *memblk, void *arg);
		};

		void Instance::breakpoint(void)
		{
			/// A handy externally visible function for breakpointing
		}

		void Instance::add_blockhead(BlockHeaderList *blks, BlockHeader *b, int type)
		{
			if (blks->tail == &blks->head)
			{
				/// Adding into an empty list, means the tail changes too
				blks->tail = &b->next;
			}
			b->next = blks->head;
			blks->head = b;

			if (!m_globals.leaksonly)
			{
				memset(b->headguard(), m_config.m_headguardfillpattern, b->headguard_size());
				memset(b->tailguard(), m_config.m_tailguardfillpattern, b->tailguard_size());
			}
		}

		void Instance::add_blocktail(BlockHeaderList *blks, BlockHeader *b, int type)
		{
			*blks->tail = b;
			blks->tail = &b->next;
			b->next = NULL;
			if (!m_globals.leaksonly)
			{
				memset(b->headguard(), m_config.m_headguardfillpattern, b->headguard_size());
				memset(b->tailguard(), m_config.m_tailguardfillpattern, b->tailguard_size());
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

		int Instance::internal_check_allocedblock(BlockHeader *b, void *arg)
		{
			if (!m_globals.leaksonly)
			{
				int	i;
				u8	*p;
				unsigned int corrupt = 0;
				BlkCheckData *data = (BlkCheckData *)arg;

				p = b->headguard();
				i = b->headguard_size();
				u8 g = (u8)m_config.m_headguardfillpattern;
				do
				{
					corrupt |= (*p++ ^ g);
				} while (--i);

				if (corrupt)
				{
					data->preCorrupt = 1;
					corrupt = 0;
				}

				p = b->tailguard();
				i = b->tailguard_size();
				g = (u8)m_config.m_tailguardfillpattern;
				do
				{
					corrupt |= (*p++ ^ g);
				} while (--i);

				if (corrupt)
				{
					data->postCorrupt = 1;
					corrupt = 0;
				}

				if ((data->freeCorrupt | data->preCorrupt | data->postCorrupt) == 0)
				{
					b->lastOk = m_globals.sequence;
				}
				data->found |= 1;
			}
			return 0;
		}

		int Instance::internal_check_freedblock(BlockHeader *b, void *arg)
		{
			if (!m_globals.leaksonly)
			{
				int           i;
				char         *p;
				BlkCheckData *data = (BlkCheckData *)arg;

				p = (char*)b->toblock();
				i = b->blksize;
				/// Attempt to speed this up by checking an (aligned) int at a time
				do
				{
					if (((size_t)p) & 1)
					{
						if (*p++ != (char)m_config.m_freefillpattern)
							break;
						i--;
						if (i == 0)
							break;
					}

					if ((i >= 2) && (((size_t)p) & 2))
					{
						short const b = *(short *)p;
						if (b != (short)m_config.m_freefillpattern)
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
						if (b != (int)m_config.m_freefillpattern)
							goto mismatch;
						p += 4;
						i -= 4;
					}
					i += 4;
					if ((i >= 2) && (((size_t)p) & 2))
					{
						short const b = *(short *)p;
						if (b != (short)m_config.m_freefillpattern)
							goto mismatch;
						p += 2;
						i -= 2;
					}
				mismatch:
					while (i)
					{
						if (*p++ != (char)m_config.m_freefillpattern)
							break;
						i--;
					}
				} while (0);

				if (i)
				{
					data->freeCorrupt = 1;
					data->index = b->blksize - i;
				}

				return internal_check_allocedblock(b, arg);
			}
			else
			{
				return 0;
			}
		}

		void Instance::remove_block(BlockHeaderList *blks, BlockHeader *b)
		{
			BlockHeader *head = blks->head;
			BlockHeader *prev = NULL;

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

		int Instance::internal_makespace(size_t space)
		{
			/// Do we store it in the free-list ?
			if (space > m_config.m_freeskipsizemax || space < m_config.m_freeskipsizemin)
				return 0;

			/// Pretend we added it on
			m_globals.freeListSize += space;

			/// Ditch blocks until it fits within our limit or our list is empty
			while (m_globals.free.head != NULL && m_globals.freeListSize > m_config.m_freemaxsizekeep)
			{
				BlockHeader *head = m_globals.free.head;
				m_globals.free.head = head->next;
				m_globals.freeListSize -= head->blksize;
			}

			/// Make sure we haven't just completely emptied the free list
			if (m_globals.free.head == NULL)
				m_globals.free.tail = &m_globals.free.head;

			/// Make sure the configuration is such that it allows us to keep this 
			/// allocation in our free list.
			if (m_globals.freeListSize > m_config.m_freemaxsizekeep)
			{
				/// We cannot add this allocation, rollback the size increment
				m_globals.freeListSize -= space;
				return 0;
			}

			return 1;
		}

		int Instance::app_blocks(BlockHeaderList *blks, Functor& func, void *arg)
		{
			BlockHeader *head = blks->head;
			BlockHeader *next;
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

		int Instance::app_block(BlockHeaderList *blks, Functor& func, void *arg, BlockHeader *b)
		{
			BlockHeader *head = blks->head;
			BlockHeader *next;
			int result = 0;
			while (head && head != b)
			{
				next = head->next;
				head = next;
			}
			if (head == b)
				result = func.dofunc(head, arg);
			return result;
		}

		static void showBlock(BlockHeader *b, int space, x_memento_report* printer)
		{
			printer->print("0x%p:", b->toblock());
			printer->print("(size=%d,num=%d)", 2, (int)b->blksize, b->sequence);
			if (b->label)
			{
				char cstr[2];
				cstr[0] = (char)space;
				cstr[1] = '\0';
				printer->print("%s", cstr);
				printer->print("(%s)", b->label);
			}
		}

		static void blockDisplay(BlockHeader *b, int n, x_memento_report* printer)
		{
			n++;
			while (n > 40)
			{
				printer->print("%s", "*");
				n -= 40;
			}
			while (n > 0)
			{
				int i = n;
				if (i > 32)
					i = 32;
				n -= i;
				printer->print("%s", &"                                "[32 - i]);
			}
			showBlock(b, '\t', printer);
			printer->print("%s", "\n");
		}

		static int Memento_listBlock(BlockHeader *b, void *arg, x_memento_report* printer)
		{
			int *counts = (int *)arg;
			blockDisplay(b, 0, printer);
			counts[0]++;
			counts[1] += b->blksize;
			return 0;
		}

		static int Memento_listNewBlock(BlockHeader *b, void *arg, x_memento_report* printer)
		{
			if (b->flags & Flag_OldBlock)
				return 0;
			b->flags |= Flag_OldBlock;
			return Memento_listBlock(b, arg, printer);
		}

		class memento_listnewblock_functor : public Functor
		{
			x_memento_report* m_printer;
		public:
			inline		memento_listnewblock_functor(x_memento_report* printer) : m_printer(printer) {}

			virtual int dofunc(BlockHeader * blkh, void * arg)
			{
				return Memento_listNewBlock(blkh, arg, m_printer);
			}
		};

		static void doNestedDisplay(BlockHeader *b, int depth, x_memento_report* printer)
		{
			/* Try and avoid recursion if we can help it */
			do
			{
				blockDisplay(b, depth, printer);
				if (b->sibling)
				{
					if (b->child)
						doNestedDisplay(b->child, depth + 1, printer);
					b = b->sibling;
				}
				else
				{
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

#define MEMENTO_UNDERLYING_MALLOC(size, align)	m_internal_mem_allocator->allocate(size, align)
#define MEMENTO_UNDERLYING_FREE(ptr) m_internal_mem_allocator->deallocate(ptr)
#define MEMENTO_UNDERLYING_REALLOC(ptr, size) m_internal_mem_allocator->reallocate(ptr, size, 4)

		int Instance::list_blocks_nested(void)
		{
			int count, size, i;
			BlockHeader *b;
			void **blocks, *minptr, *maxptr;
			long mask;

			/* Count the blocks */
			count = 0;
			size = 0;
			for (b = m_globals.used.head; b; b = b->next)
			{
				size += b->blksize;
				count++;
			}

			/* Make our block list */
			blocks = (void**)MEMENTO_UNDERLYING_MALLOC(sizeof(void *) * count, sizeof(void*));
			if (blocks == NULL)
				return 1;

			/* Populate our block list */
			b = m_globals.used.head;
			minptr = maxptr = b->toblock();
			mask = (long)minptr;
			for (i = 0; b; b = b->next, i++)
			{
				void *p = b->toblock();
				mask &= (long)p;
				if (p < minptr)
					minptr = p;
				if (p > maxptr)
					maxptr = p;
				blocks[i] = p;
				b->flags &= ~Flag_HasParent;
				b->child = NULL;
				b->sibling = NULL;
				b->parent = NULL;
			}
			qsort(blocks, count, sizeof(void *), ptrcmp);

			/* Now, calculate tree */
			for (b = m_globals.used.head; b; b = b->next)
			{
				char *p = (char*)b->toblock();
				int end = (b->blksize < m_config.m_ptrsearch ? b->blksize : m_config.m_ptrsearch);
				for (i = m_globals.searchskip; i < end; i += sizeof(void *))
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
						BlockHeader *child = BlockHeader::s_blocktoheader(*r);
						BlockHeader *parent;

						/* We're assuming tree structure, not graph - ignore second
						 * and subsequent pointers. */
						if (child->parent != NULL)
							continue;
						if (child->flags & Flag_HasParent)
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
						child->flags |= Flag_HasParent;
					}
				}
			}

			/* Now display with nesting */
			for (b = m_globals.used.head; b; b = b->next)
			{
				if ((b->flags & Flag_HasParent) == 0)
					doNestedDisplay(b, 0, m_config.m_report);
			}
			m_config.m_report->print(" Total number of blocks = %d\n", 1, count);
			m_config.m_report->print(" Total size of blocks = %d\n", 1, size);

			MEMENTO_UNDERLYING_FREE(blocks);

			/* Now put the blocks back for valgrind */
			for (b = m_globals.used.head; b;)
			{
				BlockHeader *next = b->next;
				VALGRIND_MAKE_MEM_NOACCESS(b, sizeof(*b));
				b = next;
			}

			return 0;
		}


		void Instance::list_blocks(void)
		{
			m_config.m_report->print("%s", "Allocated blocks:\n");
			if (list_blocks_nested())
			{
				int counts[2];
				counts[0] = 0;
				counts[1] = 0;
				memento_listnewblock_functor f(m_config.m_report);
				app_blocks(&m_globals.used, f, &counts[0]);
				m_config.m_report->print(" Total number of blocks = %d\n", 1, counts[0]);
				m_config.m_report->print(" Total size of blocks = %d\n", 1, counts[1]);
			}
		}

		void Instance::list_newblocks(void)
		{
			int counts[2];
			counts[0] = 0;
			counts[1] = 0;
			m_config.m_report->print("%s", "Blocks allocated and still extant since last list:\n");
			memento_listnewblock_functor f(m_config.m_report);
			app_blocks(&m_globals.used, f, &counts[0]);
			m_config.m_report->print("  Total number of blocks = %d\n", 1, counts[0]);
			m_config.m_report->print("  Total size of blocks = %d\n", 1, counts[1]);
		}

		void Instance::end_stats(void)
		{
			m_config.m_report->print("Total memory malloced = %u bytes\n", 1, (unsigned int)m_globals.totalAlloc);
			m_config.m_report->print("Peak memory malloced = %u bytes\n", 1, (unsigned int)m_globals.peakAlloc);
			m_config.m_report->print("%u mallocs, %u frees, %u reallocs\n", 3, (unsigned int)m_globals.numMallocs, (unsigned int)m_globals.numFrees, (unsigned int)m_globals.numReallocs);
			m_config.m_report->print("Average allocation size %u bytes\n", 1, (unsigned int)(m_globals.numMallocs != 0 ? m_globals.totalAlloc / m_globals.numMallocs : 0));
		}

		void Instance::stats(void)
		{
			m_config.m_report->print("Current memory malloced = %u bytes\n", 1, (unsigned int)m_globals.alloc);
			end_stats();
		}

		void Instance::finalize(void)
		{
			check_all_memory();
			end_stats();

			if (m_globals.used.head != NULL)
			{
				list_blocks();
				breakpoint();
			}

			if (m_globals.segv)
			{
				m_config.m_report->print("Memory dumped on SEGV while squeezing @ %d\n", 1, m_globals.failAt);
			}
			else if (m_globals.squeezing)
			{
				if (m_globals.pattern == 0)
					m_config.m_report->print("Memory squeezing @ %d complete\n", 1, m_globals.squeezeAt);
				else
					m_config.m_report->print("Memory squeezing @ %d (%d) complete\n", 2, m_globals.squeezeAt, m_globals.pattern);
			}

			if (m_globals.failing)
			{
				m_config.m_report->print("fail_at=%d\n", 1, m_globals.failAt);
				m_config.m_report->print("MEMENTO_PATTERN=%d\n", 1, m_globals.pattern);
			}

			if (m_globals.nextFailAt != 0)
			{
				m_config.m_report->print("MEMENTO_NEXTFAILAT=%d\n", 1, m_globals.nextFailAt);
				m_config.m_report->print("MEMENTO_NEXTPATTERN=%d\n", 1, m_globals.nextPattern);
			}
		}

		static void Memento_inited(void)
		{
			/* A good place for a breakpoint */
		}

		void Instance::initialize(x_memento_config const& config)
		{
			memset(&m_globals, 0, sizeof(m_globals));

			m_config = config;

			m_globals.inited = 1;
			m_globals.used.head = NULL;
			m_globals.used.tail = &m_globals.used.head;
			m_globals.free.head = NULL;
			m_globals.free.tail = &m_globals.free.head;
			m_globals.sequence = 0;
			m_globals.countdown = 1024;

			//@JURGEN
			//atexit(finalize);

			Memento_inited();
		}

		void Instance::signal(void)
		{
			m_globals.segv = 1;

			/* If we just return from this function the SEGV will be unhandled, and
			 * we'll launch into whatever JIT debugging system the OS provides. At
			 * least m_config.m_report->print(something useful first. If MEMENTO_NOJIT is set, then
			 * just exit to avoid the JIT (and get the usual atexit handling). */

			finalize();
		}

		void Instance::start_failing(void)
		{
			if (!m_globals.failing)
			{
				m_config.m_report->print("%s", "Starting to fail...\n");

				m_globals.failing = 1;
				m_globals.failAt = m_globals.sequence;
				m_globals.nextFailAt = m_globals.sequence + 1;
				m_globals.pattern = 0;
				m_globals.patternBit = 0;
				//signal(SIGSEGV, &xcore::Instance::Memento_signal);
				//signal(SIGABRT, Memento_signal);
				breakpoint();
			}
		}

		void Instance::event(void)
		{
			m_globals.sequence++;
			if ((m_globals.sequence >= m_globals.paranoidAt) && (m_globals.paranoidAt != 0))
			{
				m_globals.paranoia = 1;
				m_globals.countdown = 1;
			}
			if (--m_globals.countdown == 0)
			{
				check_all_memory();
				m_globals.countdown = m_globals.paranoia;
			}

			if (m_globals.sequence == m_globals.breakAt)
			{
				m_config.m_report->print("Breaking at event %d\n", 1, m_globals.breakAt);
				breakpoint();
			}
		}

		int Instance::break_at(int event)
		{
			m_globals.breakAt = event;
			return event;
		}

		void * Instance::label(void *ptr, const char *label)
		{
			BlockHeader *block;
			if (ptr == NULL)
				return NULL;
			block = BlockHeader::s_blocktoheader(ptr);
			VALGRIND_MAKE_MEM_DEFINED(&block->label, sizeof(block->label));
			block->label = label;
			VALGRIND_MAKE_MEM_UNDEFINED(&block->label, sizeof(block->label));
			return ptr;
		}

		int Instance::fail_this_event(void)
		{
			int failThisOne;

			if (!m_globals.inited)
			{
				x_memento_config config;
				config.init(&m_stdout_null);
				initialize(config);
			}

			event();

			if ((m_globals.sequence >= m_globals.failAt) && (m_globals.failAt != 0))
				start_failing();

			if (!m_globals.failing)
				return 0;

			failThisOne = ((m_globals.patternBit & m_globals.pattern) == 0);

			/* If we are failing, and we've reached the end of the pattern and we've
			 * still got bits available in the pattern word, and we haven't already
			 * set a nextPattern, then extend the pattern. */
			if (m_globals.failing && ((~(m_globals.patternBit - 1) & m_globals.pattern) == 0) && (m_globals.patternBit != 0) && m_globals.nextPattern == 0)
			{
				/* We'll fail this one, and set the 'next' one to pass it. */
				m_globals.nextFailAt = m_globals.failAt;
				m_globals.nextPattern = m_globals.pattern | m_globals.patternBit;
			}

			m_globals.patternBit = (m_globals.patternBit ? m_globals.patternBit << 1 : 1);

			return failThisOne;
		}

		void * Instance::mmalloc(unsigned int s, unsigned int a)
		{
			u32 const smem = BlockHeader::s_allocsize(s, a);

			if (fail_this_event())
				return NULL;

			if (s == 0)
				return NULL;

			m_globals.numMallocs++;

			if (m_globals.maxMemory != 0 && m_globals.alloc + s > m_globals.maxMemory)
				return NULL;

			void * alloc = MEMENTO_UNDERLYING_MALLOC(smem, a);
			if (alloc == NULL)
				return NULL;

			m_globals.alloc += s;
			m_globals.totalAlloc += s;
			if (m_globals.peakAlloc < m_globals.alloc)
				m_globals.peakAlloc = m_globals.alloc;

			void * block = BlockHeader::s_alloc2block(alloc, s, a);
			if (!m_globals.leaksonly)
			{
				memset(block, m_config.m_allocfillpattern, s);
			}

			BlockHeader *memblk = BlockHeader::s_blocktoheader(block);

			memblk->allocsize = smem;
			memblk->allocalign = a;
			memblk->blksize = s;
			memblk->sequence = m_globals.sequence;
			memblk->lastOk = memblk->sequence;
			memblk->flags = 0;
			memblk->label = 0;
			memblk->child = NULL;
			memblk->sibling = NULL;
			add_blockhead(&m_globals.used, memblk, 0);

			return block;
		}

		class checkallocedblock_functor : public Functor
		{
			Instance* m_instance;
		public:
			checkallocedblock_functor(Instance* Instance) : m_instance(Instance) { }
			virtual int dofunc(BlockHeader * blkh, void * arg)
			{
				return m_instance->internal_check_allocedblock(blkh, arg);
			}
		};

		int Instance::check_block_action(BlockHeader *memblk, const char *action)
		{
			if (!m_globals.leaksonly)
			{
				BlkCheckData data;
				checkallocedblock_functor f(this);

				memset(&data, 0, sizeof(data));
				app_block(&m_globals.used, f, &data, memblk);
				if (!data.found)
				{
					/* Failure! */
					m_config.m_report->print("Attempt to %s block ", action);
					showBlock(memblk, 32, m_config.m_report);
					m_config.m_report->print("%s", "\n");
					breakpoint();
					return 1;
				}
				else if (data.preCorrupt || data.postCorrupt)
				{
					m_config.m_report->print("%s", "Block ");
					showBlock(memblk, ' ', m_config.m_report);
					m_config.m_report->print(" found to be corrupted on %s!\n", action);
					if (data.preCorrupt)
					{
						m_config.m_report->print("%s", "Preguard corrupted\n");
					}
					if (data.postCorrupt)
					{
						m_config.m_report->print("%s", "Postguard corrupted\n");
					}
					m_config.m_report->print("Block last checked OK at allocation %d. Now %d.\n", 2, memblk->lastOk, m_globals.sequence);
					breakpoint();
					return 1;
				}
			}
			return 0;
		}

		void Instance::mfree(void *blk)
		{
			BlockHeader *memblk;

			if (!m_globals.inited)
			{
				x_memento_config config;
				config.init(&m_stdout_null);
				initialize(config);
			}

			event();

			if (blk == NULL)
				return;

			memblk = BlockHeader::s_blocktoheader(blk);
			if (check_block_action(memblk, "free"))
				return;

			if (memblk->flags & Flag_BreakOnFree)
				breakpoint();

			m_globals.alloc -= memblk->blksize;
			m_globals.numFrees++;

			remove_block(&m_globals.used, memblk);

			if (internal_makespace(memblk->blksize))
			{
				if (!m_globals.leaksonly)
				{
					memset(memblk->toblock(), m_config.m_freefillpattern, memblk->blksize);
				}
				add_blocktail(&m_globals.free, memblk, 1);
			}
			else
			{
				MEMENTO_UNDERLYING_FREE(memblk->toalloc());
			}
		}

		void * Instance::mrealloc(void *blk, unsigned int newsize)
		{
			if (blk == NULL)
				return mmalloc(newsize, 16);

			if (newsize == 0)
			{
				mfree(blk);
				return NULL;
			}

			if (fail_this_event())
				return NULL;

			BlockHeader *memblk = BlockHeader::s_blocktoheader(blk);
			unsigned int align = memblk->allocalign;
			if (check_block_action(memblk, "realloc"))
				return NULL;

			VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
			if (memblk->flags & Flag_BreakOnRealloc)
				breakpoint();

			VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
			if (m_globals.maxMemory != 0 && m_globals.alloc - memblk->blksize + newsize > m_globals.maxMemory)
				return NULL;

			u32 newsizemem = BlockHeader::s_allocsize(newsize, align);
			remove_block(&m_globals.used, memblk);
			VALGRIND_MAKE_MEM_DEFINED(memblk, sizeof(*memblk));
			int flags = memblk->flags;
			BlockHeader *newmemblk = (BlockHeader*)MEMENTO_UNDERLYING_REALLOC(memblk->toalloc(), newsizemem);
			if (newmemblk == NULL)
			{
				add_blockhead(&m_globals.used, memblk, 2);
				return NULL;
			}

			m_globals.numReallocs++;
			m_globals.totalAlloc += newsize;
			m_globals.alloc -= newmemblk->blksize;
			m_globals.alloc += newsize;
			if (m_globals.peakAlloc < m_globals.alloc)
				m_globals.peakAlloc = m_globals.alloc;

			newmemblk->flags = flags;
			if (newmemblk->blksize < newsize)
			{
				char *newbytes = ((char *)newmemblk->toblock()) + newmemblk->blksize;
				if (!m_globals.leaksonly)
				{
					memset(newbytes, m_config.m_allocfillpattern, newsize - newmemblk->blksize);
				}
			}
			newmemblk->allocsize = newsizemem;
			newmemblk->blksize = newsize;

			if (!m_globals.leaksonly)
			{
				memset(newmemblk->headguard(), m_config.m_headguardfillpattern, newmemblk->headguard_size());
				memset(newmemblk->tailguard(), m_config.m_tailguardfillpattern, newmemblk->tailguard_size());
			}
			add_blockhead(&m_globals.used, newmemblk, 2);
			return newmemblk->toblock();
		}

		int Instance::check_block(void *blk)
		{
			BlockHeader *memblk;

			if (blk == NULL)
				return 0;
			memblk = BlockHeader::s_blocktoheader(blk);
			return check_block_action(memblk, "check");
		}

		int Instance::internal_check_allalloced(BlockHeader *memblk, void *arg)
		{
			BlkCheckData *data = (BlkCheckData *)arg;

			internal_check_allocedblock(memblk, data);
			if (data->preCorrupt || data->postCorrupt)
			{
				if ((data->found & 2) == 0)
				{
					m_config.m_report->print("%s", "Allocated blocks:\n");
					data->found |= 2;
				}
				m_config.m_report->print("%s", "  Block ");
				showBlock(memblk, ' ', m_config.m_report);
				if (data->preCorrupt)
				{
					m_config.m_report->print("%s", " Preguard ");
				}
				if (data->postCorrupt)
				{
					m_config.m_report->print("%s Postguard ", (data->preCorrupt ? "&" : ""));
				}
				m_config.m_report->print("%s", "corrupted.\n");
				m_config.m_report->print("    Block last checked OK at allocation %d. Now %d.\n", 2, memblk->lastOk, m_globals.sequence);
				data->preCorrupt = 0;
				data->postCorrupt = 0;
				data->freeCorrupt = 0;
			}
			else
				memblk->lastOk = m_globals.sequence;
			return 0;
		}

		int Instance::internal_check_allfreed(BlockHeader *memblk, void *arg)
		{
			BlkCheckData *data = (BlkCheckData *)arg;

			internal_check_freedblock(memblk, data);
			if (data->preCorrupt || data->postCorrupt || data->freeCorrupt)
			{
				if ((data->found & 4) == 0)
				{
					m_config.m_report->print("%s", "Freed blocks:\n");
					data->found |= 4;
				}
				m_config.m_report->print("%s", "  ");
				showBlock(memblk, ' ', m_config.m_report);

				if (data->freeCorrupt)
				{
					m_config.m_report->print(" index %d", 1, data->index);
					m_config.m_report->print(" (address 0x%p) onwards", &((char *)memblk->toblock())[data->index]);
					if (data->preCorrupt)
					{
						m_config.m_report->print("%s", "+ preguard");
					}
					if (data->postCorrupt)
					{
						m_config.m_report->print("%s", "+ postguard");
					}
				}
				else
				{
					if (data->preCorrupt)
					{
						m_config.m_report->print("%s", " preguard");
					}
					if (data->postCorrupt)
					{
						m_config.m_report->print("%s Postguard", (data->preCorrupt ? "+" : ""));
					}
				}
				m_config.m_report->print("%s", " corrupted.\n");
				m_config.m_report->print("    Block last checked OK at allocation %d. Now %d.\n", 2, memblk->lastOk, m_globals.sequence);
				data->preCorrupt = 0;
				data->postCorrupt = 0;
				data->freeCorrupt = 0;
			}
			else
				memblk->lastOk = m_globals.sequence;
			return 0;
		}

		class checkallalloced_functor : public Functor
		{
			Instance* m_instance;
		public:
			checkallalloced_functor(Instance* Instance) : m_instance(Instance) { }
			virtual int dofunc(BlockHeader * blkh, void * arg)
			{
				return m_instance->internal_check_allalloced(blkh, arg);
			}
		};

		class checkallfreed_functor : public Functor
		{
			Instance* m_instance;
		public:
			checkallfreed_functor(Instance* Instance) : m_instance(Instance) { }
			virtual int dofunc(BlockHeader * blkh, void * arg)
			{
				return m_instance->internal_check_allfreed(blkh, arg);
			}
		};

		int Instance::check_all_memory(void)
		{
			if (!m_globals.leaksonly)
			{
				BlkCheckData data;

				memset(&data, 0, sizeof(data));
				checkallalloced_functor checkAllAlloced(this);
				app_blocks(&m_globals.used, checkAllAlloced, &data);
				checkallfreed_functor checkAllFreed(this);
				app_blocks(&m_globals.free, checkAllFreed, &data);
				if (data.found & 6) {
					breakpoint();
					return 1;
				}
			}
			return 0;
		}

		int Instance::set_paranoia(int i)
		{
			m_globals.paranoia = i;
			m_globals.countdown = m_globals.paranoia;
			return i;
		}

		int Instance::paranoid_at(int i)
		{
			m_globals.paranoidAt = i;
			return i;
		}

		int Instance::get_blocknum(void *b)
		{
			BlockHeader *memblk;
			if (b == NULL)
				return 0;
			memblk = BlockHeader::s_blocktoheader(b);
			return (memblk->sequence);
		}

		int Instance::check(void)
		{
			int result;

			m_config.m_report->print("%s", "Checking memory\n");
			result = check_all_memory();
			m_config.m_report->print("%s", "Memory checked!\n");
			return result;
		}

		struct findBlkData
		{
			void              *addr;
			BlockHeader *blk;
			int                flags;
		};

		static int Memento_containsAddr(BlockHeader *b, void *arg)
		{
			findBlkData *data = (findBlkData *)arg;
			char *blkend = (char*)b->tailguard();
			if ((b->toblock() <= data->addr) && ((void *)blkend > data->addr))
			{
				data->blk = b;
				data->flags = 1;
				return 1;
			}
			if (((void *)b <= data->addr) && (b->toblock() > data->addr))
			{
				data->blk = b;
				data->flags = 2;
				return 1;
			}
			if (((void *)blkend <= data->addr) && ((void *)(blkend + b->tailguard_size()) > data->addr))
			{
				data->blk = b;
				data->flags = 3;
				return 1;
			}
			return 0;
		}

		class containsaddr_functor : public Functor
		{
		public:
			inline		containsaddr_functor() { }
			virtual int dofunc(BlockHeader * blkh, void * arg)
			{
				return Memento_containsAddr(blkh, arg);
			}
		};


		int Instance::find(void *a)
		{
			findBlkData data;

			data.addr = a;
			data.blk = NULL;
			data.flags = 0;
			containsaddr_functor containsAddr;
			app_blocks(&m_globals.used, containsAddr, &data);
			if (data.blk != NULL)
			{
				m_config.m_report->print("Address 0x%p ", data.addr);
				m_config.m_report->print("is in %s allocated block ", (data.flags == 1 ? "" : (data.flags == 2 ? "preguard of " : "postguard of ")));
				showBlock(data.blk, ' ', m_config.m_report);
				m_config.m_report->print("%s", "\n");
				return data.blk->sequence;
			}
			data.blk = NULL;
			data.flags = 0;
			app_blocks(&m_globals.free, containsAddr, &data);
			if (data.blk != NULL)
			{
				m_config.m_report->print("Address 0x%p ", data.addr);
				m_config.m_report->print("is in %s freed block ", (data.flags == 1 ? "" : (data.flags == 2 ? "preguard of " : "postguard of ")));
				showBlock(data.blk, ' ', m_config.m_report);
				m_config.m_report->print("%s", "\n");
				return data.blk->sequence;
			}
			return 0;
		}

		void Instance::break_on_free(void *a)
		{
			findBlkData data;
			containsaddr_functor containsAddr;

			data.addr = a;
			data.blk = NULL;
			data.flags = 0;
			app_blocks(&m_globals.used, containsAddr, &data);
			if (data.blk != NULL)
			{
				m_config.m_report->print("Will stop when address 0x%p ", data.addr);
				m_config.m_report->print("(in %sallocated block ", (data.flags == 1 ? "" : (data.flags == 2 ? "preguard of " : "postguard of ")));
				showBlock(data.blk, ' ', m_config.m_report);
				m_config.m_report->print("%s", ") is freed\n");
				data.blk->flags |= Flag_BreakOnFree;
				return;
			}
			data.blk = NULL;
			data.flags = 0;
			app_blocks(&m_globals.free, containsAddr, &data);
			if (data.blk != NULL)
			{
				m_config.m_report->print("Can't stop on free; address 0x%p ", data.addr);
				m_config.m_report->print("is in %sfreed block ", (data.flags == 1 ? "" : (data.flags == 2 ? "preguard of " : "postguard of ")));
				showBlock(data.blk, ' ', m_config.m_report);
				m_config.m_report->print("%s", "\n");
				return;
			}
			m_config.m_report->print("Can't stop on free; address 0x%p is not in a known block.\n", a);
		}

		void Instance::break_on_realloc(void *a)
		{
			findBlkData data;
			containsaddr_functor containsAddr;

			data.addr = a;
			data.blk = NULL;
			data.flags = 0;
			app_blocks(&m_globals.used, containsAddr, &data);
			if (data.blk != NULL)
			{
				m_config.m_report->print("Will stop when address 0x%p ", data.addr);
				m_config.m_report->print("(in %sallocated block ", (data.flags == 1 ? "" : (data.flags == 2 ? "preguard of " : "postguard of ")));
				showBlock(data.blk, ' ', m_config.m_report);
				m_config.m_report->print("%s", ") is freed (or realloced)\n");
				data.blk->flags |= Flag_BreakOnFree | Flag_BreakOnRealloc;
				return;
			}

			data.blk = NULL;
			data.flags = 0;
			app_blocks(&m_globals.free, containsAddr, &data);
			if (data.blk != NULL)
			{
				m_config.m_report->print("Can't stop on free/realloc; address 0x%p ", (data.flags == 1 ? "" : (data.flags == 2 ? "preguard of " : "postguard of ")));
				m_config.m_report->print("is in %sfreed block ", (data.flags == 1 ? "" : (data.flags == 2 ? "preguard of " : "postguard of ")));
				showBlock(data.blk, ' ', m_config.m_report);
				m_config.m_report->print("%s", "\n");
				return;
			}
			m_config.m_report->print("Can't stop on free/realloc; address 0x%p is not in a known block.\n", a);
		}

		int Instance::fail_at(int i)
		{
			m_globals.failAt = i;
			if ((m_globals.sequence > m_globals.failAt) && (m_globals.failing != 0))
				start_failing();
			return i;
		}

		size_t Instance::set_maxmemory(size_t max)
		{
			m_globals.maxMemory = max;
			return max;
		}

	}	/// end-of namespace memento


	class x_memento_allocator : public x_iallocator
	{
	public:
		virtual const char*		name() const									{ return TARGET_FULL_DESCR_STR " memento allocator"; }

		virtual void*			allocate(xsize_t size, u32 alignment)
		{
			return m_memento.mmalloc((u32)size, alignment);
		}

		virtual void*			reallocate(void* ptr, xsize_t size, u32 alignment)
		{
			return m_memento.mrealloc(ptr, (u32)size);
		}

		virtual void			deallocate(void* ptr)
		{
			return m_memento.mfree(ptr);
		}

		virtual void			release()
		{
			m_memento.finalize();
			m_allocator->deallocate(this);
		}

		void*					operator new(xsize_t num_bytes)					{ return NULL; }
		void*					operator new(xsize_t num_bytes, void* mem)		{ return mem; }
		void					operator delete(void* pMem)						{ }
		void					operator delete(void* pMem, void*)				{ }

		x_iallocator*			m_allocator;
		memento::Instance		m_memento;

	private:
		virtual					~x_memento_allocator()							{ }

	};

	x_iallocator*				gCreateMementoAllocator(x_memento_config const& config, x_iallocator* allocator)
	{
		void* mem = allocator->allocate(sizeof(x_memento_allocator), sizeof(void*));
		x_memento_allocator* memento_allocator = new (mem)x_memento_allocator();
		memento_allocator->m_allocator = allocator;
		memento_allocator->m_memento.m_internal_mem_allocator = allocator;
		memento_allocator->m_memento.initialize(config);
		return memento_allocator;
	}

}
