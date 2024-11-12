#include "ccore/c_memory.h"
#include "ccore/c_allocator.h"
#include "ccore/c_math.h"
#include "cbase/c_hash.h"
#include "cbase/c_runes.h"
#include "cbase/c_tree32.h"

#include "callocator/c_allocator_string.h"

namespace ncore
{
    namespace nstring
    {
        namespace nutf8
        {
            struct members_t
            {
                utf8::prune     m_str_memory;
                utf8::prune     m_str_cursor;
                utf8::prune     m_str_end;
                str_t*          m_items;
                ntree32::tree_t m_tree;
                ntree32::node_t m_root;
                u32             m_size;
                u32             m_max;
            };

            class utf8_storage_t : public storage_t
            {
            public:
                utf8_storage_t() : storage_t() {}
                virtual ~utf8_storage_t() {}
                DCORE_CLASS_PLACEMENT_NEW_DELETE

                members_t m_data;

                static s8 s_compare_str(str_t const* strA, str_t const* strB)
                {
                    if (strA->m_hash != strB->m_hash)
                        return strA->m_hash < strB->m_hash ? -1 : 1;

                    if (strA->m_len != strB->m_len)
                        return strA->m_len < strB->m_len ? -1 : 1;

                    utf8::pcrune strA8 = strA->m_str;
                    utf8::pcrune strB8 = strB->m_str;
                    utf8::pcrune endA8 = strA8 + strA->m_len;
                    utf8::pcrune endB8 = strB8 + strB->m_len;
                    while (strA8 < endA8)
                    {
                        utf8::rune rA = *strA8++;
                        utf8::rune rB = *strB8++;
                        if (rA.r != rB.r)
                            return rA.r < rB.r ? -1 : 1;
                    }
                    return 0;
                }

                static s8 s_compare_str_to_node(u32 const _str, u32 const _node, void const* user_data)
                {
                    members_t const* members  = (members_t const*)user_data;
                    str_t const*     str      = members->m_items + _str;
                    str_t const*     node_str = members->m_items + _node;
                    return s_compare_str(str, node_str);
                }

                void setup(void* mem, size_t mem_size, u32 max_items)
                {
                    m_data.m_items = (str_t*)mem;
                    m_data.m_root  = ntree32::c_invalid_node;
                    m_data.m_size  = 0;
                    m_data.m_max   = max_items;

                    ntree32::setup_tree(m_data.m_tree, (ntree32::nnode_t*)(m_data.m_items + max_items));

                    m_data.m_str_memory = (utf8::prune)(m_data.m_tree.m_nodes + max_items);
                    m_data.m_str_cursor = m_data.m_str_memory;
                    m_data.m_str_end    = m_data.m_str_cursor + mem_size - 1;
                    m_data.m_str_end->r = utf8::TERMINATOR;
                }

                cpstr_t v_put(crunes_t const& str) override
                {
                    // The incoming string can be of any type, we will convert it to UTF-8.
                    utf8::prune dst8    = m_data.m_str_cursor;
                    utf8::prune end8    = m_data.m_str_end;
                    u32         cursor8 = 0;
                    u32         cursor  = str.m_str;
                    switch (str.m_type)
                    {
                        case ascii::TYPE: utf::convert(str.m_ascii, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                        case ucs2::TYPE: utf::convert(str.m_ucs2, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                        case utf8::TYPE: utf::convert(str.m_utf8, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                        case utf16::TYPE: utf::convert(str.m_utf16, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                        case utf32::TYPE: utf::convert(str.m_utf32, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                    }
                    end8 = dst8 + cursor8;

                    str_t* const item = m_data.m_items + m_data.m_size;
                    item->m_str       = dst8;
                    item->m_hash      = (u32)nhash::strhash((const char*)dst8, (const char*)end8);
                    item->m_len       = (u32)(end8 - dst8);

                    ntree32::node_t key_node = m_data.m_size;
                    ntree32::node_t found_node;
                    if (!ntree32::find(m_data.m_tree, m_data.m_root, key_node, s_compare_str_to_node, this, found_node))
                    {
                        ntree32::node_t tmp_node = m_data.m_size + 1;
                        ntree32::insert(m_data.m_tree, m_data.m_root, key_node, tmp_node, s_compare_str_to_node, this, found_node);
                        str_t* insert_item = m_data.m_items + found_node;
                        *insert_item       = *item;
                        m_data.m_size++;
                        m_data.m_str_cursor = dst8 + 1; // +1 to include the terminator.
                    }
                    return m_data.m_items + found_node;
                }

                cpstr_t v_put(const char* str) override
                {
                    // The incoming string is ASCII, we will convert it to UTF-8.
                    utf8::prune dst8    = m_data.m_str_cursor;
                    utf8::prune end8    = m_data.m_str_end;
                    u32         cursor8 = 0;
                    u32         cursor  = 0;
                    utf::convert(str, cursor, dst8, cursor8, (u32)(end8 - dst8));
                    end8 = dst8 + cursor8;

                    str_t* const item = m_data.m_items + m_data.m_size;
                    item->m_str       = dst8;
                    item->m_hash      = (u32)nhash::strhash((const char*)dst8, (const char*)end8);
                    item->m_len       = (u32)(end8 - dst8);

                    ntree32::node_t key_node = m_data.m_size;
                    ntree32::node_t found_node;
                    if (!ntree32::find(m_data.m_tree, m_data.m_root, key_node, s_compare_str_to_node, this, found_node))
                    {
                        ntree32::node_t tmp_node = m_data.m_size + 1;
                        ntree32::insert(m_data.m_tree, m_data.m_root, key_node, tmp_node, s_compare_str_to_node, this, found_node);
                        str_t* insert_item = m_data.m_items + found_node;
                        *insert_item       = *item;
                        m_data.m_size++;
                        m_data.m_str_cursor = dst8 + 1; // +1 to include the terminator.
                    }
                    return m_data.m_items + found_node;
                }
            };

            static u32 compute_max_items(size_t memory_size)
            {
                // average string length is 32 bytes
                size_t const average_string_len = 32;
                return (u32)((memory_size - sizeof(utf8_storage_t)) / (sizeof(str_t) + sizeof(ntree32::nnode_t) + average_string_len));
            }

        } // namespace nutf8

        storage_t* g_create_storage_utf8(void* mem, size_t mem_size)
        {
            nutf8::utf8_storage_t* storage   = new (mem) nutf8::utf8_storage_t();
            u32 const              max_items = math::floorpo2(nutf8::compute_max_items(mem_size));
            storage->setup((u8*)mem + sizeof(nutf8::utf8_storage_t), mem_size - sizeof(nutf8::utf8_storage_t), max_items);
            return storage;
        }

        namespace nascii
        {
            struct members_t
            {
                char*           m_str_memory;
                char*           m_str_cursor;
                char*           m_str_end;
                str_t*          m_items;
                ntree32::tree_t m_tree;
                ntree32::node_t m_root;
                u32             m_size;
                u32             m_max;
            };

            class ascii_storage_t : public storage_t
            {
            public:
                ascii_storage_t() : storage_t() {}
                virtual ~ascii_storage_t() {}
                DCORE_CLASS_PLACEMENT_NEW_DELETE

                members_t m_data;

                static s8 s_compare_str(str_t const* strA, str_t const* strB)
                {
                    if (strA->m_hash != strB->m_hash)
                        return strA->m_hash < strB->m_hash ? -1 : 1;

                    if (strA->m_len != strB->m_len)
                        return strA->m_len < strB->m_len ? -1 : 1;

                    utf8::pcrune strA8 = strA->m_str;
                    utf8::pcrune strB8 = strB->m_str;
                    utf8::pcrune endA8 = strA8 + strA->m_len;
                    utf8::pcrune endB8 = strB8 + strB->m_len;
                    while (strA8 < endA8)
                    {
                        utf8::rune rA = *strA8++;
                        utf8::rune rB = *strB8++;
                        if (rA.r != rB.r)
                            return rA.r < rB.r ? -1 : 1;
                    }
                    return 0;
                }

                static s8 s_compare_str_to_node(u32 const _str, u32 const _node, void const* user_data)
                {
                    members_t const* members  = (members_t const*)user_data;
                    str_t const*     str      = members->m_items + _str;
                    str_t const*     node_str = members->m_items + _node;
                    return s_compare_str(str, node_str);
                }

                void setup(void* mem, size_t mem_size, u32 max_items)
                {
                    m_data.m_items = (str_t*)mem;
                    m_data.m_root  = ntree32::c_invalid_node;
                    m_data.m_size  = 0;
                    m_data.m_max   = max_items;

                    ntree32::setup_tree(m_data.m_tree, (ntree32::nnode_t*)(m_data.m_items + max_items));

                    m_data.m_str_memory = (char*)(m_data.m_tree.m_nodes + max_items);
                    m_data.m_str_cursor = m_data.m_str_memory;
                    m_data.m_str_end    = m_data.m_str_cursor + mem_size - 1;
                    *m_data.m_str_end   = ascii::TERMINATOR;
                }

                cpstr_t v_put(crunes_t const& str) override
                {
                    // The incoming string can be of any type, we will convert it to UTF-8.
                    char* dst8    = m_data.m_str_cursor;
                    char* end8    = m_data.m_str_end;
                    u32   cursor8 = 0;
                    u32   cursor  = str.m_str;
                    switch (str.m_type)
                    {
                        case ascii::TYPE: utf::convert(str.m_ascii, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                        case ucs2::TYPE: utf::convert(str.m_ucs2, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                        case utf8::TYPE: utf::convert(str.m_utf8, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                        case utf16::TYPE: utf::convert(str.m_utf16, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                        case utf32::TYPE: utf::convert(str.m_utf32, cursor, str.m_end, dst8, cursor8, (u32)(end8 - dst8)); break;
                    }
                    end8 = dst8 + cursor8;

                    str_t* const item = m_data.m_items + m_data.m_size;
                    item->m_str       = (utf8::prune)dst8;
                    item->m_hash      = (u32)nhash::strhash((const char*)dst8, (const char*)end8);
                    item->m_len       = (u32)(end8 - dst8);

                    ntree32::node_t key_node = m_data.m_size;
                    ntree32::node_t found_node;
                    if (!ntree32::find(m_data.m_tree, m_data.m_root, key_node, s_compare_str_to_node, this, found_node))
                    {
                        ntree32::node_t tmp_node = m_data.m_size + 1;
                        ntree32::insert(m_data.m_tree, m_data.m_root, key_node, tmp_node, s_compare_str_to_node, this, found_node);
                        str_t* insert_item = m_data.m_items + found_node;
                        *insert_item       = *item;
                        m_data.m_size++;
                        m_data.m_str_cursor = dst8 + 1; // +1 to include the terminator.
                    }
                    return m_data.m_items + found_node;
                }

                cpstr_t v_put(const char* str) override
                {
                    // The incoming string is ASCII, we will convert it to UTF-8.
                    char* dst8    = m_data.m_str_cursor;
                    char* end8    = m_data.m_str_end;
                    u32   cursor8 = 0;
                    u32   cursor  = 0;
                    utf::convert(str, cursor, dst8, cursor8, (u32)(end8 - dst8));
                    end8 = dst8 + cursor8;

                    str_t* const item = m_data.m_items + m_data.m_size;
                    item->m_str       = (utf8::prune)dst8;
                    item->m_hash      = (u32)nhash::strhash((const char*)dst8, (const char*)end8);
                    item->m_len       = (u32)(end8 - dst8);

                    ntree32::node_t key_node = m_data.m_size;
                    ntree32::node_t found_node;
                    if (!ntree32::find(m_data.m_tree, m_data.m_root, key_node, s_compare_str_to_node, this, found_node))
                    {
                        ntree32::node_t tmp_node = m_data.m_size + 1;
                        ntree32::insert(m_data.m_tree, m_data.m_root, key_node, tmp_node, s_compare_str_to_node, this, found_node);
                        str_t* insert_item = m_data.m_items + found_node;
                        *insert_item       = *item;
                        m_data.m_size++;
                        m_data.m_str_cursor = dst8 + 1; // +1 to include the terminator.
                    }
                    return m_data.m_items + found_node;
                }
            };

            static u32 compute_max_items(size_t memory_size)
            {
                // average string length is 32 bytes
                size_t const average_string_len = 32;
                return (u32)((memory_size - sizeof(ascii_storage_t)) / (sizeof(str_t) + sizeof(ntree32::nnode_t) + average_string_len));
            }

        } // namespace nascii

        storage_t* g_create_storage_ascii(void* mem, size_t mem_size)
        {
            nascii::ascii_storage_t* storage   = new (mem) nascii::ascii_storage_t();
            u32 const                max_items = math::floorpo2(nascii::compute_max_items(mem_size));
            storage->setup((u8*)mem + sizeof(nascii::ascii_storage_t), mem_size - sizeof(nascii::ascii_storage_t), max_items);
            return storage;
        }

        void g_destroy_storage(storage_t* storage) {}

    } // namespace nstring

} // namespace ncore
