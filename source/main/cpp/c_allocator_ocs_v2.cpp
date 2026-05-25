#include "ccore/c_target.h"

struct struct_a_t
{
    float m;
};

struct struct_b_t
{
    float m;
};

struct struct_c_t
{
    float m;
};

namespace ncore
{
#define D_STRUCT_TYPE static u16 const __nst_index__
#define D_STRUCT_TYPE_SET(i) static u16 const __nst_index__ = i

    struct entity_t
    {
        u64 m_global_mask; // A system can hold a maximum of 64 struct types
        u8  m_bin[16];     // Maximum amount of bins per struct type is 256
        u16 m_index[16];   // A bin can only hold a maximum of 32768 items
    };

    // Every bin is limited to a maximum of 32768 entries, so if the smallest
    // struct size is 16 and the largest is 256, then the address space a bin
    // should cover is largest size * 32768 = 8 MiB
    struct bin_t
    {
        u16    m_struct_size;   // the size of the struct in this bin
        u16    m_free_count;    // number of free entries in this bin
        u16    m_free_index;    // index of the first free entry in this bin
        u32    m_free_l0;       // binmap level 0
        u32    m_free_l1[32];   // binmap level 1
        u32    m_free_l2[1024]; // binmap level 2
        u32*   m_entity_index;  // entity index for each entry in this bin
    };

    struct system_t
    {
        u64    m_total_address_space; // total virtual address space
        u32    m_max_global;          // maximum global struct types
        u8*    m_global_to_local;     // global struct index to local
        u8*    m_data;                // m_bin_size * 256 * 64 = 8MiB * 16384 =
        bin_t* m_bins[256 * 64];      // Each struct type has a maximum 256 bins

        // Maximum entities = 8M
        u64       m_entities_l0;
        u64       m_entities_l1[64];
        u64*      m_entities_l2;
        u64*      m_entities_l3;
        u32       m_entity_count;
        entity_t* m_entities; // virtual memory
    };

    static system_t* create_system() { return nullptr; }
    
    template<typename T>
    static T* create_struct(system_t* system) { return nullptr; }

    template<typename A, typename B>
    static void attach_struct(system_t* system, A* a, B* b) { }

    template<typename B, typename A>
    static B* get_struct(system_t* system, A* a) { return nullptr; }

    template<typename B, typename A>
    static bool has_struct(system_t* system, A* a) { return false; }

    template<typename T>
    static void detach_struct(system_t* system, T* t) { }

    template<typename T>
    static void destroy_struct(system_t* system, T* t) { }

    static void test_func()
    {
        // User API is like this:
        system_t* system = create_system();

        struct_a_t* a = create_struct<struct_a_t>(system);
        struct_b_t* b = create_struct<struct_b_t>(system);

        // attach a and b (this will create an entity if it didn't exist yet)
        attach_struct(system, a, b);

        // we have a, now get b
        struct_b_t* bb = get_struct<struct_b_t>(system, a);

        bool a_has_b = has_struct<struct_b_t>(system, a);
        bool b_has_a = has_struct<struct_a_t>(system, b);

        // detach b
        detach_struct(system, b);

        // create c, and attach b and c
        struct_c_t* c = create_struct<struct_c_t>(system);
        attach_struct(system, c, b);

        destroy_struct(system, b); // this will also detach b!
    }
} // namespace ncore