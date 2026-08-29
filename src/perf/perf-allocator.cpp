// ── Global operator new/delete overrides for allocation tracking ──
// These intercept every heap allocation and update gPerfAllocCount,
// gPerfAllocBytes, and gPerfLargestAlloc so the profiler can report
// exact per-frame allocation counts and the largest single allocation.

#include <cstdlib>
#include <new>
#include "perf/perf-spike.h"

void* operator new(std::size_t size)
{
    gPerfAllocCount.fetch_add(1, std::memory_order_relaxed);
    gPerfAllocBytes.fetch_add(static_cast<uint64_t>(size), std::memory_order_relaxed);

    uint64_t oldLargest = gPerfLargestAlloc.load(std::memory_order_relaxed);
    const uint64_t size64 = static_cast<uint64_t>(size);
    while (size64 > oldLargest &&
           !gPerfLargestAlloc.compare_exchange_weak(
               oldLargest, size64,
               std::memory_order_relaxed, std::memory_order_relaxed)) {}

    void* ptr = std::malloc(size);
    if (!ptr) throw std::bad_alloc();
    return ptr;
}

void operator delete(void* ptr) noexcept
{
    std::free(ptr);
}

void* operator new[](std::size_t size)
{
    return ::operator new(size);
}

void operator delete[](void* ptr) noexcept
{
    ::operator delete(ptr);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
    gPerfAllocCount.fetch_add(1, std::memory_order_relaxed);
    gPerfAllocBytes.fetch_add(static_cast<uint64_t>(size), std::memory_order_relaxed);

    uint64_t oldLargest = gPerfLargestAlloc.load(std::memory_order_relaxed);
    const uint64_t size64 = static_cast<uint64_t>(size);
    while (size64 > oldLargest &&
           !gPerfLargestAlloc.compare_exchange_weak(
               oldLargest, size64,
               std::memory_order_relaxed, std::memory_order_relaxed)) {}

    return std::malloc(size);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept
{
    std::free(ptr);
}
