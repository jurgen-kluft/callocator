#ifndef __TEST_ALLOCATOR_H__
#define __TEST_ALLOCATOR_H__
#include "cbase/c_target.h"
#ifdef USE_PRAGMA_ONCE
#pragma once
#endif

#include "cbase/c_allocator.h"
#include "cunittest/private/ut_Config.h"

class test_alloc_t : public ncore::alloc_t
{
    UnitTest::TestAllocator** mAllocator;

public:
    test_alloc_t(UnitTest::TestAllocator** allocator)
        : mAllocator(allocator)
    {
    }
    virtual void*      v_allocate(ncore::u32 size, ncore::u32 align) { return (*mAllocator)->Allocate(size, align); }
    virtual ncore::u32 v_deallocate(void* p) { return (*mAllocator)->Deallocate(p); }
    virtual void       v_release() {}
};

#define UNITTEST_ALLOCATOR                         \
    static test_alloc_t TestAlloc(&TestAllocator); \
    static alloc_t*     Allocator = &TestAlloc

#endif // __TEST_ALLOCATOR_H__