#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_integer.h"
#include "xbase/x_memory.h"

#include "xallocator/private/x_bitlist.h"

namespace xcore
{
    u32 xhibitset::size_in_dwords(u32 numbits)
    {
        u32 numdwords = 0;
        while (numbits > 1)
        {
            numdwords += ((numbits + 31) / 32);
            numbits = (numbits + 31) >> 5;
        }
        return numdwords * 4;
    }

    void xhibitset::init(u32* bitlist, u32 maxbits, bool setall, bool invert)
    {
		m_numbits   = maxbits;
        m_invert    = invert ? AllBitsSet : 0;

        // Figure out the pointer to every level
        u32  numbits = maxbits;
        u32  offset  = 0;
        u32* level   = bitlist;
        s32  i       = 0;
        while (numbits > 1)
        {
            m_levels[i++] = level;
            level        += ((numbits + 31) / 32);
            numbits       = (numbits + 31) >> 5;
        }
        m_maxlevel = i;
        reset(setall);
    }

    void xhibitset::init(xalloc* alloc, u32 maxbits, bool setall, bool invert)
    {
        u32  ndwords = size_in_dwords(maxbits);
        u32* bitlist = (u32*)alloc->allocate(ndwords * 4, sizeof(u32));
        init(bitlist, maxbits, setall, invert);
    }

    void xhibitset::release(xalloc* alloc)
    {
        alloc->deallocate(m_levels[0]);
        m_maxlevel = 0;
        m_numbits  = 0;
        m_invert   = 0;
    }

    // 5000 bits = 628 bytes = 157 u32 = (32768 bits level 0)
    // 157  bits =  20 bytes =   5 u32 = ( 1024 bits level 1)
    //   5  bits =   4 byte  =   1 u32 = (   32 bits level 2)
    // level 0, bits= 5000, dwords= 157, bytes= 628
    // level 1, bits= 157, dwords= 5, bytes= 20
    // level 2, bits= 5, dwords= 1, bytes= 4
    // total = 628 + 20 + 4 = 652 bytes

    void xhibitset::reset(bool setall)
    {
        s32 const invert = setall ? ~m_invert : m_invert;

        s32 i = 0;
        u32 numbits = m_numbits;
        while (numbits > 1)
        {
            u32* level = m_levels[i];
            u32 numdwords = ((numbits + 31) / 32);
            x_memset(level, m_invert, numdwords * 4);
            if (m_invert == 0)
            {
                u32 lastmask = (0xffffffff << ((numdwords * 32) - numbits));
                level[numdwords - 1] = level[numdwords - 1] | lastmask;
            }
            else
            {
                u32 lastmask = (0xffffffff << ((numdwords * 32) - numbits));
                level[numdwords - 1] = level[numdwords - 1] & ~lastmask;
            }
            numbits = (numbits + 31) >> 5;
            i += 1;
        }
    }

    void xhibitset::set(u32 bit)
    {
        ASSERT(bit < m_numbits);

        // set bit in level 0, then avalanche up if necessary
        s32 i = 0;
        while (i < m_maxlevel)
        {
            u32* level = m_levels[i];
            u32  dwordIndex = (bit + 31) / 32;
            u32  dwordBit   = 1 << (bit & 31);
            u32  dword0     = level[dwordIndex];
            u32  dword1;
            bool avalanche;
            if (m_invert == 0)
            {
                dword1    = dword0 | dwordBit;
                avalanche = (dword0 != dword1 && dword1 == AllBitsSet);
            }
            else
            {
                dword1    = dword0 & ~dwordBit;
                avalanche = (dword0 != dword1 && dword0 == AllBitsSet);
            }

            level[dwordIndex] = dword1;

            if (!avalanche)
                break;

            i   += 1;
            bit  = bit >> 5;
        }
    }

    void xhibitset::clr(u32 bit)
    {
        ASSERT(bit < m_numbits);

        // clear bit in level 0, then avalanche up if necessary
        s32 i = 0;
        while (i < m_maxlevel)
        {
            u32* level = m_levels[i];
            u32  dwordIndex = (bit + 31) / 32;
            u32  dwordBit   = 1 << (bit & 31);
            u32  dword0     = level[dwordIndex];
            u32  dword1;
            bool avalanche;
            if (m_invert == 0)
            {
                dword1    = dword0 & ~dwordBit;
                avalanche = (dword0 != dword1 && dword0 == AllBitsSet);
            }
            else
            {
                dword1    = dword0 | dwordBit;
                avalanche = (dword0 != dword1 && dword1 == AllBitsSet);
            }

            level[dwordIndex] = dword1;

            if (!avalanche)
                break;

            i   += 1;
            bit  = bit >> 5;
        }
    }

	bool xhibitset::is_set(u32 bit) const
    {
        u32 const* level = m_levels[0];
        u32 dwordIndex = bit / 32;
        u32 dwordBit = bit & 31;
        ASSERT(&level[dwordIndex] < m_levels[1]);
        return level[dwordIndex] & (1 << dwordBit);
    }

	bool xhibitset::is_full() const
    {
        if (m_invert == 0)
        {
            return m_levels[m_maxlevel - 1] == 0xfffffffff;
        }
        return m_levels[m_maxlevel - 1] == 0;
    }

    bool xhibitset::find(u32& bit) const
    {
        // Start at top level and find a '0' bit and move down
        u32 dwordIndex = 0;
        s32 i = m_maxlevel - 1; 
        while (i >= 0)
        {
            u32 const* level = m_levels[i];
            u32 dword0   = level[dwordIndex];
            u32 dwordBit = xfindFirstBit(~dword0);
            if (dwordBit == 32)
                return false;
            dwordIndex = (dwordIndex * 32) + dwordBit;
            i -= 1;
        }
        return dwordIndex;
    }

}; // namespace xcore
