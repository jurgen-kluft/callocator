#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_integer.h"
#include "xbase/x_memory.h"

#include "xbase/x_bitlist.h"

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

    void xhibitset::init(u32* bitlist, u32 maxbits, EFind find)
    {
		m_numbits   = maxbits;
        m_find      = (find == FIND_0) ? 0 : 1;

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
    }

    void xhibitset::init(xalloc* alloc, u32 maxbits, EFind find)
    {
        u32  ndwords = size_in_dwords(maxbits);
        u32* bitlist = (u32*)alloc->allocate(ndwords * 4, sizeof(u32));
        init(bitlist, maxbits, find);
    }

    void xhibitset::release(xalloc* alloc)
    {
        alloc->deallocate(m_levels[0]);
        m_maxlevel = 0;
        m_numbits  = 0;
        m_find     = 0;
    }

    // 5000 bits = 628 bytes = 157 u32 = (32768 bits level 0)
    // 157  bits =  20 bytes =   5 u32 = ( 1024 bits level 1)
    //   5  bits =   4 byte  =   1 u32 = (   32 bits level 2)
    // level 0, bits= 5000, dwords= 157, bytes= 628
    // level 1, bits= 157, dwords= 5, bytes= 20
    // level 2, bits= 5, dwords= 1, bytes= 4
    // total = 628 + 20 + 4 = 652 bytes

    void xhibitset::reset()
    {
        s32 i = 0;
        u32 numbits = m_numbits;
        while (numbits > 1)
        {
            u32* level = m_levels[i];
            u32 numdwords = ((numbits + 31) / 32);
            if (m_find == 0)
            {
	            x_memset(level, 0, numdwords * 4);
				if (numbits < (numdwords * 32))
				{
					s32 shl = ((numdwords * 32) - numbits);
					level[numdwords - 1] = (AllBitsSet << shl);
				}
            }
            else
            {
	            x_memset(level, AllBitsSet, numdwords * 4);
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
            u32  dwordIndex = bit / 32;
            u32  dwordBit   = 1 << (bit & 31);
            u32  dword0     = level[dwordIndex];
            u32  dword1;
            bool avalanche;
            if (m_find == 0)
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
            u32  dwordIndex = bit / 32;
            u32  dwordBit   = 1 << (bit & 31);
            u32  dword0     = level[dwordIndex];
            u32  dword1;
            bool avalanche;
            if (m_find == 0)
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
        ASSERT(bit < m_numbits);
        u32 const* level = m_levels[0];
        u32 dwordIndex = bit / 32;
        u32 dwordBit = bit & 31;
        return ((level[dwordIndex]>>dwordBit) & 1) != m_find;
    }

	bool xhibitset::is_full() const
    {
		u32 const* level = m_levels[m_maxlevel - 1];
        return level[0] == ((1 - m_find) * 0xfffffffff);
    }

    bool xhibitset::find(u32& bit) const
    {
        // Start at top level and find a '0' bit and move down
        u32 dwordIndex = 0;
		u32 dwordBit = 0;
        s32 i = m_maxlevel - 1; 
        while (i >= 0)
        {
            u32 const* level = m_levels[i];
            dwordIndex = (dwordIndex * 32) + dwordBit;
            u32 dword0   = level[dwordIndex];
            dwordBit = xfindFirstBit(~dword0);
            if (dwordBit == 32)
                return false;
            i -= 1;
        }
		bit = (dwordIndex * 32) + dwordBit;
        return true;
    }

}; // namespace xcore
