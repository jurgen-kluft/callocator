#include "ccore/c_target.h"
#include "ccore/c_debug.h"
#include "cbase/c_memory.h"
#include "cbase/c_integer.h"
#include "cbase/c_allocator.h"

#include "callocator/private/c_forwardbin.h"

namespace ncore
{
    namespace forwardbin
    {
        //==============================================================================
        //==============================================================================
        // This is a chunk used in the forward allocator
        //==============================================================================
        //==============================================================================
        struct chunk // 32 bytes
        {
        public:
            enum EMagic
            {
                CHUNK_MAGIC_BEGIN = 0xF00DBEEF,
                CHUNK_MAGIC_HEAD  = 0xFEEDFEED,
                CHUNK_MAGIC_USED  = 0xC0DEC0DE,
                CHUNK_MAGIC_END   = 0xDEADBEEF
            };

            inline chunk() : magic(CHUNK_MAGIC_USED), size(0) { link[0] = link[1] = 0; }

            inline bool inRange(chunk const* begin, chunk const* end) const { return this >= begin && this <= end; }
            inline bool isValid() const { return magic == CHUNK_MAGIC_BEGIN || magic == CHUNK_MAGIC_HEAD || magic == CHUNK_MAGIC_USED || magic == CHUNK_MAGIC_END; }

            inline chunk* getNext() const { return link[1]; }
            inline chunk* getPrev() const { return link[0]; }
            inline void   setNext(chunk* c) { link[1] = c; }
            inline void   setPrev(chunk* c) { link[0] = c; }

            inline u32  getSize() const { return size; }
            inline void setSize(u32 _size) { size = _size; }
            inline void addSize(u32 _size) { size += _size; }
            inline void subSize(u32 _size) { size -= _size; }
            inline u32  popSize()
            {
                u32 _size = size;
                size      = 0;
                return _size;
            }

            void initialize(chunk* _prev, chunk* _next, u32 _size, u32 _magic)
            {
                magic   = _magic;
                link[0] = _prev;
                link[1] = _next;
                size    = _size;
            }

            void setMagic(u32 _magic) { magic = _magic; }

            void merge(chunk*& head)
            {
                // Removing chunk 'c' from the double linked list
                chunk* c    = this;
                chunk* prev = c->getPrev();
                chunk* next = c->getNext();
                if (head == next)
                {
                    // The next chunk is our head and is thus pointing to free memory
                    // Let's move back the head to this chunk this giving it more memory
                    // to allocate from.
                    c->addSize(head->getSize() + sizeof(chunk));
                    next = head->getNext();
                    c->setNext(next);
                    c->setMagic(CHUNK_MAGIC_HEAD);
                    next->setPrev(c);
                    head = c;
                }
                else
                {
                    // Our previous block is a used block, merge with that one.
                    prev->addSize(c->getSize() + sizeof(chunk));
                    next->setPrev(prev);
                    prev->setNext(next);
                }
            }

        private:
            u32    magic;
            u32    size;
            chunk* link[2];
        };

        //==============================================================================
        //==============================================================================
        // The forward allocator
        //==============================================================================
        //==============================================================================
        //==============================================================================

        allocator::allocator() : mMemBegin(nullptr), mMemEnd(nullptr), mHead(nullptr), mNumAllocations(0) {}

        inline static void gIsValidChunk(chunk const* begin, chunk const* end, chunk const* c) { ASSERT(c->isValid() && c->inRange(begin, end)); }

        void allocator::reset()
        {
            // We hold a 'Begin' and 'End' chunk so that we only merge like a list and not like a ring.
            // Coalescing a block at the end with one at the beginning (wrapping around) is something
            // that we are trying to avoid by doing this.

            mBegin = (chunk*)mMemBegin;
            mEnd   = (chunk*)((u8*)mMemEnd - sizeof(chunk));
            mHead  = (chunk*)((u8*)mMemBegin + sizeof(chunk));

            u32 const free_mem_size = (u32)((u8*)mEnd - ((u8*)mHead + sizeof(chunk)));

            mBegin->initialize(mEnd, mHead, 0, chunk::CHUNK_MAGIC_BEGIN);
            mHead->initialize(mBegin, mEnd, free_mem_size, chunk::CHUNK_MAGIC_HEAD);
            mEnd->initialize(mHead, mBegin, 0, chunk::CHUNK_MAGIC_END);

            gIsValidChunk(mBegin, mEnd, mBegin);
            gIsValidChunk(mBegin, mEnd, mHead);
            gIsValidChunk(mBegin, mEnd, mEnd);

            mNumAllocations = 0;
        }

        void allocator::init(u8* mem_begin, u8* mem_end)
        {
            mMemBegin = mem_begin;
            mMemEnd   = mem_end;
            reset();
        }

        enum EMove
        {
            FORWARD,
            BACKWARD
        };
        chunk* move_chunk(chunk* begin, chunk* end, chunk* c, u32 num_bytes_to_move, EMove move)
        {
            gIsValidChunk(begin, end, c);

            if (num_bytes_to_move == 0)
                return c;

            chunk* next = c->getNext();
            chunk* prev = c->getPrev();

            u8* cc = (u8*)c;
            c         = (chunk*)(cc + num_bytes_to_move);
            if (move == FORWARD)
            {
                u8* dst = cc + num_bytes_to_move + sizeof(chunk);
                u8* src = cc + sizeof(chunk);
                while (src != cc)
                    *--dst = *--src;
                c->subSize(num_bytes_to_move);
                prev->addSize(num_bytes_to_move);
            }
            else if (move == BACKWARD)
            {
                u8* dst = cc - num_bytes_to_move;
                u8* src = cc;
                while (dst != cc)
                    *dst++ = *src++;
                c->addSize(num_bytes_to_move);
                prev->subSize(num_bytes_to_move);
            }

            // Now fix the linking
            next->setPrev(c);
            prev->setNext(c);

            gIsValidChunk(begin, end, next);
            gIsValidChunk(begin, end, prev);
            gIsValidChunk(begin, end, c);

            return c;
        }

        inline static u32 gAlignPtr(u8* ptr, u32 alignment, u8*& outPtr)
        {
            u32 const diff = (alignment - ((ptr_t)ptr & (alignment - 1))) & (alignment - 1);
            outPtr         = ptr + diff;
            return diff;
        }

        u8* allocator::allocate(u32 size, u32 alignment)
        {
            // do we have to align up ?
            size = math::alignUp(size, (u32)4);

            u8* alloc_address = nullptr;
            while (true)
            {
                if (size < mHead->getSize())
                {
                    u32 const diff = gAlignPtr((u8*)mHead + sizeof(chunk), alignment, alloc_address);

                    // Check if we still can fulfill this request after alignment
                    if (((diff + sizeof(chunk)) < mHead->getSize()) && size <= (mHead->getSize() - (diff + sizeof(chunk))))
                    {
                        // Move chunk structure up with @diff bytes. This will also fix the chunk
                        // data so that the linking and sizes are correct.
                        chunk* c = move_chunk(mBegin, mEnd, mHead, diff, FORWARD);
                        mHead    = nullptr;

                        // Construct the new head after the allocated chunk
                        chunk* head = (chunk*)((u8*)alloc_address + size);
                        head->initialize(c, c->getNext(), c->getSize() - size - sizeof(chunk), chunk::CHUNK_MAGIC_HEAD);
                        gIsValidChunk(mBegin, mEnd, head);

                        c->setNext(head);
                        c->setSize(size);
                        c->setMagic(chunk::CHUNK_MAGIC_USED);
                        gIsValidChunk(mBegin, mEnd, c);

                        mHead = head;
                        ++mNumAllocations;
                    }
                    else
                    {
                        alloc_address = nullptr;
                    }
                    break;
                }
                else if (mHead->getNext() == mEnd)
                {
                    // It can happen (infrequently) that this chunk is limited by the end of our managed memory.
                    // So here we can check if that happens and if so we need to wrap around and skip the begin chunk
                    // and see if we can fulfill the allocation request with the free size that the begin chunk holds.
                    // If we can can allocate then we must build a new head and merge the old one into a used chunk.

                    // see if we can fulfill the allocation request by taking the next free chunk
                    ASSERT(mHead->getNext() == mEnd && mHead->getNext()->getNext() == mBegin);

                    if (mBegin->getSize() > sizeof(chunk) && size <= (mBegin->getSize() - sizeof(chunk)))
                    {
                        // Alignment might still pose a risk in fulfilling this request
                        chunk*    head = mBegin + 1;
                        u32 const diff = gAlignPtr((u8*)head + sizeof(chunk), alignment, alloc_address);
                        if ((diff + sizeof(chunk)) < mBegin->getSize() && size <= (mBegin->getSize() - (diff + sizeof(chunk))))
                        {
                            // Move head by 'diff' number of bytes
                            head = (chunk*)((u8*)head + diff);

                            // Configure 'head' and set size of mBegin to '0'
                            head->initialize(mBegin, mBegin->getNext(), mBegin->getSize() - (diff + sizeof(chunk)), chunk::CHUNK_MAGIC_HEAD);
                            gIsValidChunk(mBegin, mEnd, head);
                            mBegin->setSize(0);

                            // Merge Head into it's previous 'used' chunk
                            chunk* dummy = mHead;
                            mHead->merge(dummy);
                            // Reset head to the new chunk after Begin
                            mHead = head;

                            // --> Stay in the while loop
                            alloc_address = nullptr;
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

        u32 allocator::get_size(void* p) const
        {
            chunk const* c = (chunk const*)((u8*)p - sizeof(chunk));
            ASSERT(c->isValid());
            return c->getSize();
        }

        u32 allocator::deallocate(void* p)
        {
            if (p == nullptr)
                return 0;

            // Deallocate will mark a chunk as is_used = 0
            // Then we will follow prev and next to see if we can coalesce.
            // When coalescing we also need to deal with mHead since we might
            // be able to move it back (although this is a Forward allocator)
            ASSERT(mNumAllocations > 0);

            --mNumAllocations;
            // if (mNumAllocations == 0)
            //{
            //	reset();
            //}
            // else
            {
                chunk* c = (chunk*)((ptr_t)p - sizeof(chunk));
                gIsValidChunk(mBegin, mEnd, c);

                // Coalesce (unite) chunks can only happen when:
                // - neighbor chunk differs in index by 1
                // - neighbor chunk is a free chunk
                u32 const size = c->getSize();

                // First mark this chunk as free
                c->merge(mHead); // After this call 'c' is invalid!
                c = nullptr;

                // Do check if the previous chunk of Head is Begin.
                // If so check if Begin has some size(), if so merge
                // that size with Head.
                if (mHead->getPrev() == mBegin)
                {
                    ASSERT(mBegin->getNext() == mHead);
                    mBegin->addSize(mHead->popSize());
                    mBegin->setNext(mHead->getNext());

                    mHead = mBegin + 1;
                    mHead->initialize(mBegin, mBegin->getNext(), mBegin->popSize(), chunk::CHUNK_MAGIC_HEAD);

                    mBegin->setNext(mHead);
                }

				return size;
            }
        }

    } // namespace xforwardbin
};    // namespace ncore
