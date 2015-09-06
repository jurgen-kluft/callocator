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
			BlockHeader*		prev;			/// A double linked list improves 'remove_block' performance

			const char*			label;
			int					id;

			enum
			{
				HeadGuardSize = 12,
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

		struct BlockHeaderList
		{
			inline			BlockHeaderList() : head(NULL) {}
			BlockHeader *	head;
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

			int				sequence;
			int				countdown;
			int				lastChecked;
			int				failAt;
			int				breakAt;
			int				paranoia;
			int				paranoidAt;
			int				failing;
			int				nextFailAt;
			int				squeezeAt;
			int				squeezing;
			int				segv;
			int				pattern;
			int				nextPattern;
			int				patternBit;

			size_t			alloc;
			size_t			peakAlloc;
			size_t			totalAlloc;
			size_t			numMallocs;
			size_t			numFrees;
			size_t			numReallocs;

			size_t			freelistsize;
			int				freemaxsizekeep;		/// The maximum size of memory to keep in the free list
			int				freeskipsizemin;		/// Do not add allocations that are smaller than this size in the free list
			int				freeskipsizemax;		/// Do not add allocations that are larger than this size in the free list

			size_t			maxmemory;
			unsigned int	ptrsearch;

			int				headguardfillpattern;
			int				tailguardfillpattern;
			int				allocfillpattern;
			int				freefillpattern;

			void			initialize()
			{
				inited = 1;

				sequence = 0;
				paranoia = 1024;
				countdown = paranoia;
				paranoidAt = 0;

				breakAt = 0;
				pattern = 0;

				freemaxsizekeep = 32 * 1024 * 1024;	/// 32 MB 
				freeskipsizemin = 0;
				freeskipsizemax = 1 * 1024 * 1024;	/// 1 MB

				maxmemory = (1024 * 1024 * 1024);
				maxmemory *= 1024;
				ptrsearch = 65536;
				headguardfillpattern = 0xAFAFAFAF;
				tailguardfillpattern = 0xDBDBDBDB;
				allocfillpattern = 0xCDCDCDCD;
				freefillpattern = 0xFEFEFEFE;
				leaksonly = false;
				searchskip = 0;
			}

			/// options - config

			// Set the following if you're only looking for leaks, not
			// memory overwrites to speed up the operation of memento.
			bool			leaksonly;
			int				searchskip;
		};

		class NullPrint : public x_memento_reporter
		{
		public:
			virtual void			print(const char* format, const char* str) { }
			virtual void			print(const char* format, void* ptr) { }
			virtual void			print(const char* format, int n, int value1, int value2 = 0, int value3 = 0, int value4 = 0) { }

			virtual void			breakpoint() 
			{
			}
		};

		struct Instance
		{
			Globals					m_globals;
			NullPrint				m_null_reporter;

			x_memento*				m_memento;
			x_memento_reporter*		m_report;
			x_iallocator*			m_allocator;
			x_memento_handler*		m_event_handler;

			void					initialize();

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
			void *					label(void *, const char *, int);

			void *					mmalloc(unsigned int s, unsigned int align);
			void *					mrealloc(void *, unsigned int s);
			void					mfree(void *);

			void					add_blockhead(BlockHeaderList *blks, BlockHeader *b);
			void					add_blocktail(BlockHeaderList *blks, BlockHeader *b);
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
			if (m_event_handler != NULL)
				m_event_handler->breakpoint(m_memento);
		}

		void Instance::add_blockhead(BlockHeaderList *blks, BlockHeader *b)
		{
			if (blks->head == NULL)
				blks->head = b;

			b->next = blks->head;
			b->prev = blks->head->prev;

			blks->head->prev->next = b;
			blks->head->prev = b;

			blks->head = b;

			if (!m_globals.leaksonly)
			{
				memset(b->headguard(), m_globals.headguardfillpattern, b->headguard_size());
				memset(b->tailguard(), m_globals.tailguardfillpattern, b->tailguard_size());
			}
		}

		void Instance::add_blocktail(BlockHeaderList *blks, BlockHeader *b)
		{
			if (blks->head == NULL)
				blks->head = b;

			b->next = blks->head;
			b->prev = blks->head->prev;

			blks->head->prev = b;
			blks->head->prev->next = b;


			if (!m_globals.leaksonly)
			{
				memset(b->headguard(), m_globals.headguardfillpattern, b->headguard_size());
				memset(b->tailguard(), m_globals.tailguardfillpattern, b->tailguard_size());
			}
		}

		struct BlkCheckData
		{
			int found;
			int corrupted_headguard;
			int corrupted_tailguard;
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
				u8 g = (u8)m_globals.headguardfillpattern;
				do
				{
					corrupt |= (*p++ ^ g);
				} while (--i);

				if (corrupt)
				{
					data->corrupted_headguard = 1;
					corrupt = 0;
				}

				p = b->tailguard();
				i = b->tailguard_size();
				g = (u8)m_globals.tailguardfillpattern;
				do
				{
					corrupt |= (*p++ ^ g);
				} while (--i);

				if (corrupt)
				{
					data->corrupted_tailguard = 1;
					corrupt = 0;
				}

				if ((data->freeCorrupt | data->corrupted_headguard | data->corrupted_tailguard) == 0)
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
						if (*p++ != (char)m_globals.freefillpattern)
							break;
						i--;
						if (i == 0)
							break;
					}

					if ((i >= 2) && (((size_t)p) & 2))
					{
						short const b = *(short *)p;
						if (b != (short)m_globals.freefillpattern)
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
						if (b != (int)m_globals.freefillpattern)
							goto mismatch;
						p += 4;
						i -= 4;
					}
					i += 4;
					if ((i >= 2) && (((size_t)p) & 2))
					{
						short const b = *(short *)p;
						if (b != (short)m_globals.freefillpattern)
							goto mismatch;
						p += 2;
						i -= 2;
					}
				mismatch:
					while (i)
					{
						if (*p++ != (char)m_globals.freefillpattern)
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
			BlockHeader *& head = blks->head;
			if (b == head)
			{
				if (b == b->next)
				{
					head = NULL;
				}
				else
				{
					head = b->next;
				}
			}
			
			b->prev->next = b->next;
			b->next->prev = b->prev;

			b->next = b;
			b->prev = b;
		}

		int Instance::internal_makespace(size_t space)
		{
			/// Do we store it in the free-list ?
			if (space > m_globals.freeskipsizemax || space < m_globals.freeskipsizemin)
				return 0;

			/// Pretend we added it on
			m_globals.freelistsize += space;

			/// Ditch blocks until it fits within our limit or our list is empty
			while (m_globals.free.head != NULL && m_globals.freelistsize > m_globals.freemaxsizekeep)
			{
				BlockHeader * head = m_globals.free.head;
				m_globals.freelistsize -= head->blksize;
				if (head != head->next)
					m_globals.free.head = head->next;
				else
					m_globals.free.head = NULL;
			}

			/// Make sure the configuration is such that it allows us to keep this 
			/// allocation in our free list.
			if (m_globals.freelistsize > m_globals.freemaxsizekeep)
			{
				/// We cannot add this allocation, rollback the size increment
				m_globals.freelistsize -= space;
				return 0;
			}

			return 1;
		}

		int Instance::app_blocks(BlockHeaderList *blks, Functor& func, void *arg)
		{
			int result = 0;
			BlockHeader * head = blks->head;
			if (head != NULL)
			{
				do
				{
					result = func.dofunc(head, arg);
					if (result)
						break;
					head = head->next;
				} while (head != blks->head);
			}
			return result;
		}

		int Instance::app_block(BlockHeaderList *blks, Functor& func, void *arg, BlockHeader *b)
		{
			int result = 0;
			BlockHeader * head = blks->head;
			if (head != NULL)
			{
				do
				{
					if (head == b)
					{
						result = func.dofunc(head, arg);
						break;
					}
					head = head->next;
				} while (head != blks->head);
			}
			return result;
		}

		static void show_block(BlockHeader *b, int space, x_memento_reporter* printer)
		{
			printer->print("0x%016x:", b->toblock());
			printer->print("(size=%d,num=%d)", 2, (int)b->blksize, b->sequence);
			if (b->label)
			{
				char cstr[2];
				cstr[0] = (char)space;
				cstr[1] = '\0';
				printer->print("%s", cstr);
				printer->print("(%s:", b->label);
				printer->print("%d)", 1, b->id);
			}
		}

		static void blockDisplay(BlockHeader *b, int n, x_memento_reporter* printer)
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
			show_block(b, '\t', printer);
			printer->print("%s", "\n");
		}

		static int s_list_block(BlockHeader *b, void *arg, x_memento_reporter* printer)
		{
			int *counts = (int *)arg;
			blockDisplay(b, 0, printer);
			counts[0]++;
			counts[1] += b->blksize;
			return 0;
		}

		static int s_list_newblock(BlockHeader *b, void *arg, x_memento_reporter* printer)
		{
			if (b->flags & Flag_OldBlock)
				return 0;
			b->flags |= Flag_OldBlock;
			return s_list_block(b, arg, printer);
		}

		class listnewblock_functor : public Functor
		{
			x_memento_reporter* m_printer;
		public:
			inline		listnewblock_functor(x_memento_reporter* printer) : m_printer(printer) {}

			virtual int dofunc(BlockHeader * blkh, void * arg)
			{
				return s_list_newblock(blkh, arg, m_printer);
			}
		};

		struct NestedBlock
		{
			BlockHeader*		current;
			NestedBlock*		parent;
			NestedBlock*		child;
			NestedBlock*		sibling;
		};

		static void doNestedDisplay(NestedBlock *b, int depth, x_memento_reporter* printer)
		{
			/// Try and avoid recursion if we can help it
			do
			{
				blockDisplay(b->current, depth, printer);
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
			NestedBlock const* nba = (NestedBlock const*)a_;
			NestedBlock const* nbb = (NestedBlock const*)b_;

			void const* a = (void const*)nba->current;
			void const* b = (void const*)nbb->current;

			if (a == b)
				return 0;
			else if (a < b)
				return -1;
			return 1;
		}

		int Instance::list_blocks_nested(void)
		{
			int count, size, i;
			BlockHeader *b;
			void *minptr, *maxptr;
			uptr mask;

			/// Count the blocks
			count = 0;
			size = 0;
			for (b = m_globals.used.head; b; b = b->next)
			{
				size += b->blksize;
				count++;
			}

			/// Make our auxilary block list 
			NestedBlock* blocks = (NestedBlock*)m_allocator->allocate(sizeof(NestedBlock) * count, sizeof(void*));
			if (blocks == NULL)
				return 1;

			/// Populate our block list
			b = m_globals.used.head;
			minptr = maxptr = b->toblock();
			mask = (uptr)minptr;
			for (i = 0; b; b = b->next, i++)
			{
				void *p = b->toblock();
				mask &= (uptr)p;
				if (p < minptr)
					minptr = p;
				if (p > maxptr)
					maxptr = p;
				
				b->flags &= ~Flag_HasParent;

				NestedBlock& nb = blocks[i];
				nb.current = b;
				nb.child = NULL;
				nb.sibling = NULL;
				nb.parent = NULL;
			}
			qsort(blocks, count, sizeof(NestedBlock), ptrcmp);

			/// Now, calculate tree
			for (int i = 0; i < count; i++)
			{
				NestedBlock* b = &blocks[i];

				char *p = (char*)b->current->toblock();
				int end = (b->current->blksize < m_globals.ptrsearch ? b->current->blksize : m_globals.ptrsearch);
				for (int j = m_globals.searchskip; j < end; j += sizeof(void *))
				{
					void *q = *(void **)(&p[j]);

					/// Do trivial checks on pointer
					if ((mask & (uptr)q) != mask || q < minptr || q > maxptr)
						continue;

					/// Search for pointer
					NestedBlock* r = (NestedBlock*)bsearch(&q, blocks, count, sizeof(void *), ptrcmp);
					if (r)
					{
						/// Found child
						NestedBlock*	child = r;
						NestedBlock*	parent;

						/// We're assuming tree structure, not graph - ignore second
						/// and subsequent pointers.
						if (child->parent != NULL)
							continue;
						if (child->current->flags & Flag_HasParent)
							continue;

						/// Not interested in pointers to ourself!
						if (child == b)
							continue;

						/// We're also assuming acyclicness here. If this is one of
						/// our parents, ignore it.
						parent = b->parent;
						while (parent != NULL && parent != child)
							parent = parent->parent;
						if (parent == child)
							continue;

						child->sibling = b->child;
						b->child = child;
						child->parent = b;
						child->current->flags |= Flag_HasParent;
					}
				}
			}

			/// Now display with nesting
			for (int i = 0; i < count; i++)
			{
				NestedBlock* b = &blocks[i];

				if ((b->current->flags & Flag_HasParent) == 0)
					doNestedDisplay(b, 0, m_report);
			}

			m_report->print(" Total number of blocks = %d\n", 1, count);
			m_report->print(" Total size of blocks = %d\n", 1, size);

			/// Free our auxilary buffer used for sorting our blocks
			m_allocator->deallocate(blocks);

			return 0;
		}


		void Instance::list_blocks(void)
		{
			m_report->print("%s", "Allocated blocks:\n");
			if (list_blocks_nested())
			{
				int counts[2];
				counts[0] = 0;
				counts[1] = 0;
				listnewblock_functor f(m_report);
				app_blocks(&m_globals.used, f, &counts[0]);
				m_report->print(" Total number of blocks = %d\n", 1, counts[0]);
				m_report->print(" Total size of blocks = %d\n", 1, counts[1]);
			}
		}

		void Instance::list_newblocks(void)
		{
			int counts[2];
			counts[0] = 0;
			counts[1] = 0;
			m_report->print("%s", "Blocks allocated and still extant since last list:\n");
			listnewblock_functor f(m_report);
			app_blocks(&m_globals.used, f, &counts[0]);
			m_report->print("  Total number of blocks = %d\n", 1, counts[0]);
			m_report->print("  Total size of blocks = %d\n", 1, counts[1]);
		}

		void Instance::end_stats(void)
		{
			m_report->print("Total memory malloced = %u bytes\n", 1, (unsigned int)m_globals.totalAlloc);
			m_report->print("Peak memory malloced = %u bytes\n", 1, (unsigned int)m_globals.peakAlloc);
			m_report->print("%u mallocs, %u frees, %u reallocs\n", 3, (unsigned int)m_globals.numMallocs, (unsigned int)m_globals.numFrees, (unsigned int)m_globals.numReallocs);
			m_report->print("Average allocation size %u bytes\n", 1, (unsigned int)(m_globals.numMallocs != 0 ? m_globals.totalAlloc / m_globals.numMallocs : 0));
		}

		void Instance::stats(void)
		{
			m_report->print("Current memory malloced = %u bytes\n", 1, (unsigned int)m_globals.alloc);
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
				m_report->print("Memory dumped on SEGV while squeezing @ %d\n", 1, m_globals.failAt);
			}
			else if (m_globals.squeezing)
			{
				if (m_globals.pattern == 0)
					m_report->print("Memory squeezing @ %d complete\n", 1, m_globals.squeezeAt);
				else
					m_report->print("Memory squeezing @ %d (%d) complete\n", 2, m_globals.squeezeAt, m_globals.pattern);
			}

			if (m_globals.failing)
			{
				m_report->print("fail_at=%d\n", 1, m_globals.failAt);
				m_report->print("MEMENTO_PATTERN=%d\n", 1, m_globals.pattern);
			}

			if (m_globals.nextFailAt != 0)
			{
				m_report->print("MEMENTO_NEXTFAILAT=%d\n", 1, m_globals.nextFailAt);
				m_report->print("MEMENTO_NEXTPATTERN=%d\n", 1, m_globals.nextPattern);
			}
		}

		static void initialized(void)
		{
			/// -> A good place for a breakpoint
		}

		void Instance::initialize()
		{
			memset(&m_globals, 0, sizeof(m_globals));

			m_globals.initialize();

			initialized();
		}

		void Instance::signal(void)
		{
			m_globals.segv = 1;

			/// If we just return from this function the SEGV will be unhandled, and
			/// we'll launch into whatever JIT debugging system the OS provides. At
			/// least m_report->print(something useful first. If MEMENTO_NOJIT is set, then
			/// just exit to avoid the JIT (and get the usual atexit handling).

			finalize();
		}

		void Instance::start_failing(void)
		{
			if (!m_globals.failing)
			{
				m_report->print("%s", "Starting to fail...\n");

				m_globals.failing = 1;
				m_globals.failAt = m_globals.sequence;
				m_globals.nextFailAt = m_globals.sequence + 1;
				m_globals.pattern = 0;
				m_globals.patternBit = 0;

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
				m_report->print("Breaking at event %d\n", 1, m_globals.breakAt);
				breakpoint();
			}
		}

		void * Instance::label(void *ptr, const char *label, int id)
		{
			BlockHeader *block;
			if (ptr == NULL)
				return NULL;
			block = BlockHeader::s_blocktoheader(ptr);
			block->label = label;
			block->id = id;
			return ptr;
		}

		int Instance::fail_this_event(void)
		{
			int failThisOne;

			if (!m_globals.inited)
			{
				initialize();
			}

			event();

			if ((m_globals.sequence >= m_globals.failAt) && (m_globals.failAt != 0))
				start_failing();

			if (!m_globals.failing)
				return 0;

			failThisOne = ((m_globals.patternBit & m_globals.pattern) == 0);

			/// If we are failing, and we've reached the end of the pattern and we've
			/// still got bits available in the pattern word, and we haven't already
			/// set a nextPattern, then extend the pattern.
			if (m_globals.failing && ((~(m_globals.patternBit - 1) & m_globals.pattern) == 0) && (m_globals.patternBit != 0) && m_globals.nextPattern == 0)
			{
				/// We'll fail this one, and set the 'next' one to pass it.
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

			if (m_globals.maxmemory != 0 && m_globals.alloc + s > m_globals.maxmemory)
				return NULL;

			void * alloc = m_allocator->allocate(smem, a);
			if (alloc == NULL)
				return NULL;

			m_globals.alloc += s;
			m_globals.totalAlloc += s;
			if (m_globals.peakAlloc < m_globals.alloc)
				m_globals.peakAlloc = m_globals.alloc;

			void * block = BlockHeader::s_alloc2block(alloc, s, a);
			if (!m_globals.leaksonly)
			{
				memset(block, m_globals.allocfillpattern, s);
			}

			BlockHeader *memblk = BlockHeader::s_blocktoheader(block);

			memblk->allocsize = smem;
			memblk->allocalign = a;
			memblk->blksize = s;
			memblk->sequence = m_globals.sequence;
			memblk->lastOk = memblk->sequence;
			memblk->flags = 0;
			memblk->label = 0;
			memblk->id = 0;
			memblk->next = memblk;
			memblk->prev = memblk;
			add_blockhead(&m_globals.used, memblk);

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
					/// Failure!
					m_report->print("Attempt to %s block ", action);
					show_block(memblk, 32, m_report);
					m_report->print("%s", "\n");
					breakpoint();
					return 1;
				}
				else if (data.corrupted_headguard || data.corrupted_tailguard)
				{
					m_report->print("%s", "Block ");
					show_block(memblk, ' ', m_report);
					m_report->print(" found to be corrupted on %s!\n", action);
					if (data.corrupted_headguard && data.corrupted_tailguard)
					{
						m_report->print("%s", "    Block head & tail-guard corrupted\n");
					}
					else if (data.corrupted_headguard)
					{
						m_report->print("%s", "    Block head-guard corrupted\n");
					}
					else if (data.corrupted_tailguard)
					{
						m_report->print("%s", "    Block tail-guard corrupted\n");
					}
					m_report->print("    Block checked OK at sequence %d. Now at sequence %d.\n", 2, memblk->lastOk, m_globals.sequence);
					breakpoint();
					return 1;
				}
			}
			return 0;
		}

		void Instance::mfree(void *blk)
		{
			if (blk == NULL)
				return;

			if (!m_globals.inited)
			{
				initialize();
			}

			event();

			BlockHeader * memblk = BlockHeader::s_blocktoheader(blk);
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
					memset(memblk->toblock(), m_globals.freefillpattern, memblk->blksize);
				}
				add_blocktail(&m_globals.free, memblk);
			}
			else
			{
				m_allocator->deallocate(memblk->toalloc());
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

			if (memblk->flags & Flag_BreakOnRealloc)
				breakpoint();

			if (m_globals.maxmemory != 0 && m_globals.alloc - memblk->blksize + newsize > m_globals.maxmemory)
				return NULL;

			u32 newsizemem = BlockHeader::s_allocsize(newsize, align);
			remove_block(&m_globals.used, memblk);
			int flags = memblk->flags;
			BlockHeader *newmemblk = (BlockHeader*)m_allocator->reallocate(memblk->toalloc(), newsizemem, align);
			if (newmemblk == NULL)
			{
				add_blockhead(&m_globals.used, memblk);
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
					memset(newbytes, m_globals.allocfillpattern, newsize - newmemblk->blksize);
				}
			}
			newmemblk->allocsize = newsizemem;
			newmemblk->blksize = newsize;

			if (!m_globals.leaksonly)
			{
				memset(newmemblk->headguard(), m_globals.headguardfillpattern, newmemblk->headguard_size());
				memset(newmemblk->tailguard(), m_globals.tailguardfillpattern, newmemblk->tailguard_size());
			}
			add_blockhead(&m_globals.used, newmemblk);
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
			if (data->corrupted_headguard || data->corrupted_tailguard)
			{
				if ((data->found & 2) == 0)
				{
					m_report->print("%s", "Allocated blocks:\n");
					data->found |= 2;
				}
				m_report->print("%s", "  Block ");
				show_block(memblk, ' ', m_report);
				if (data->corrupted_headguard && data->corrupted_tailguard)
				{
					m_report->print("%s", "head & tail");
				}
				else if (data->corrupted_headguard)
				{
					m_report->print("%s", "head");
				}
				else if (data->corrupted_tailguard)
				{
					m_report->print("%s", "tail");
				}
				m_report->print("%s", " guard corrupted.\n");
				m_report->print("    Block last checked OK at sequence %d. Now at sequence %d.\n", 2, memblk->lastOk, m_globals.sequence);
				data->corrupted_headguard = 0;
				data->corrupted_tailguard = 0;
				data->freeCorrupt = 0;
			}
			else
			{
				memblk->lastOk = m_globals.sequence;
			}
			return 0;
		}

		int Instance::internal_check_allfreed(BlockHeader *memblk, void *arg)
		{
			BlkCheckData *data = (BlkCheckData *)arg;

			internal_check_freedblock(memblk, data);
			if (data->corrupted_headguard || data->corrupted_tailguard || data->freeCorrupt)
			{
				if ((data->found & 4) == 0)
				{
					m_report->print("%s", "Freed blocks:\n");
					data->found |= 4;
				}
				m_report->print("%s", "  ");
				show_block(memblk, ' ', m_report);

				if (data->freeCorrupt)
				{
					m_report->print(" index %d", 1, data->index);
					m_report->print(" (address 0x%p) onwards", &((char *)memblk->toblock())[data->index]);
					if (data->corrupted_headguard)
					{
						m_report->print("%s", "+ head-guard");
					}
					if (data->corrupted_tailguard)
					{
						m_report->print("%s", "+ tail-guard");
					}
				}
				else
				{
					if (data->corrupted_headguard && data->corrupted_tailguard)
					{
						m_report->print("%s", " head & tail guard");
					}
					else if (data->corrupted_headguard)
					{
						m_report->print("%s", " head guard");
					}
					else if (data->corrupted_tailguard)
					{
						m_report->print("%s", " tail guard");
					}
				}
				m_report->print("%s", " corrupted.\n");
				m_report->print("    Block last checked OK at sequence %d. Now at sequence %d.\n", 2, memblk->lastOk, m_globals.sequence);
				data->corrupted_headguard = 0;
				data->corrupted_tailguard = 0;
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
				if (data.found & 6) 
				{
					breakpoint();
					return 1;
				}
			}
			return 0;
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

			m_report->print("%s", "MEMENTO: Begin; checking memory\n");
			result = check_all_memory();
			m_report->print("%s", "MEMENTO: End; checking memory\n");
			return result;
		}

		int Instance::set_paranoia(int i)
		{
			m_globals.paranoia = i;
			m_globals.countdown = i;
			return i;
		}

		int Instance::paranoid_at(int i)
		{
			m_globals.paranoidAt = i;
			return i;
		}

		int Instance::break_at(int event)
		{
			m_globals.breakAt = event;
			return event;
		}

		struct findBlkData
		{
			void*			addr;
			BlockHeader*	blk;
			int				flags;
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
				m_report->print("Address 0x%p ", data.addr);
				m_report->print("is in %s allocated block ", (data.flags == 1 ? "" : (data.flags == 2 ? "head-guard of " : "tail-guard of ")));
				show_block(data.blk, ' ', m_report);
				m_report->print("%s", "\n");
				return data.blk->sequence;
			}
			data.blk = NULL;
			data.flags = 0;
			app_blocks(&m_globals.free, containsAddr, &data);
			if (data.blk != NULL)
			{
				m_report->print("Address 0x%p ", data.addr);
				m_report->print("is in %s freed block ", (data.flags == 1 ? "" : (data.flags == 2 ? "head-guard of " : "tail-guard of ")));
				show_block(data.blk, ' ', m_report);
				m_report->print("%s", "\n");
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
				m_report->print("Will stop when address 0x%p ", data.addr);
				m_report->print("(in %sallocated block ", (data.flags == 1 ? "" : (data.flags == 2 ? "head-guard of " : "tail-guard of ")));
				show_block(data.blk, ' ', m_report);
				m_report->print("%s", ") is freed\n");
				data.blk->flags |= Flag_BreakOnFree;
				return;
			}
			data.blk = NULL;
			data.flags = 0;
			app_blocks(&m_globals.free, containsAddr, &data);
			if (data.blk != NULL)
			{
				m_report->print("Can't stop on free; address 0x%p ", data.addr);
				m_report->print("is in %sfreed block ", (data.flags == 1 ? "" : (data.flags == 2 ? "head-guard of " : "tail-guard of ")));
				show_block(data.blk, ' ', m_report);
				m_report->print("%s", "\n");
				return;
			}
			m_report->print("Can't stop on free; address 0x%p is not in a known block.\n", a);
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
				m_report->print("Will stop when address 0x%p ", data.addr);
				m_report->print("(in %sallocated block ", (data.flags == 1 ? "" : (data.flags == 2 ? "head-guard of " : "tail-guard of ")));
				show_block(data.blk, ' ', m_report);
				m_report->print("%s", ") is freed (or realloced)\n");
				data.blk->flags |= Flag_BreakOnFree | Flag_BreakOnRealloc;
				return;
			}

			data.blk = NULL;
			data.flags = 0;
			app_blocks(&m_globals.free, containsAddr, &data);
			if (data.blk != NULL)
			{
				m_report->print("Can't stop on free/realloc; address 0x%p ", (data.flags == 1 ? "" : (data.flags == 2 ? "head-guard of " : "tail-guard of ")));
				m_report->print("is in %sfreed block ", (data.flags == 1 ? "" : (data.flags == 2 ? "head-guard of " : "tail-guard of ")));
				show_block(data.blk, ' ', m_report);
				m_report->print("%s", "\n");
				return;
			}
			m_report->print("Can't stop on free/realloc; address 0x%p is not in a known block.\n", a);
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
			m_globals.maxmemory = max;
			return max;
		}

	}	/// end-of namespace memento


	class x_memento_allocator : public x_memento
	{
	public:
		virtual const char*		name() const									{ return TARGET_FULL_DESCR_STR " memento debug allocator"; }

		virtual void*			allocate(xsize_t size, u32 alignment, const char* label, int var)
		{
			void * p = m_memento.mmalloc((u32)size, alignment);
			m_memento.label(p, label, var);
			return p;
		}

		virtual void			set_reporter(x_memento_reporter* reporter)
		{
			m_memento.m_report = reporter;
		}

		virtual void			set_handler(x_memento_handler* handler)
		{
			m_memento.m_event_handler = handler;
		}

		virtual void			paranoia(int p)
		{
			m_memento.set_paranoia(p);
		}

		virtual void			paranoidAt(int sequence)
		{
			m_memento.paranoid_at(sequence);
		}

		virtual void			breakAt(int sequence)
		{
			m_memento.break_at(sequence);
		}

		virtual void			set_freelist(int maxkeep, int skipmin, int skipmax)
		{
			m_memento.m_globals.freemaxsizekeep = maxkeep;
			m_memento.m_globals.freeskipsizemin = skipmin;
			m_memento.m_globals.freeskipsizemax = skipmax;
		}

		virtual void			list_blocks()
		{
			m_memento.list_blocks();
		}

		virtual void			check()
		{
			m_memento.check();
		}

		virtual bool			check_block(void* p)
		{
			return m_memento.check_block(p) == 0;
		}

		virtual void			check_all_memory()
		{
			m_memento.check_all_memory();
		}

		virtual void			break_on_free(void* p)
		{
			m_memento.break_on_free(p);
		}

		/// 
		/// x_iallocator interface
		/// 
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

	x_memento*				gCreateMementoAllocator(x_iallocator* allocator)
	{
		void* mem = allocator->allocate(sizeof(x_memento_allocator), sizeof(void*));
		x_memento_allocator* memento_allocator = new (mem)x_memento_allocator();
		memento_allocator->m_allocator = allocator;
		memento_allocator->m_memento.m_memento = memento_allocator;
		memento_allocator->m_memento.m_event_handler = NULL;
		memento_allocator->m_memento.m_allocator = allocator;
		memento_allocator->m_memento.initialize();
		return memento_allocator;
	}

}
