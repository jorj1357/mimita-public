# Plan: Fix Profiler Allocation Counter Overflow Bug

## Root Cause Analysis

**`gPerfAllocCount` is a 32-bit signed `int`** (`perf-spike.h:80`, `perf-spike.cpp:23`). It counts every `operator new` call since launch. After ~2.1 billion allocations (achievable in 1–2+ hours of active gameplay), it overflows past `INT_MAX`, goes negative, and breaks all per-frame delta logic.

**The cascade:**
1. `gPerfAllocCount` wraps to negative (signed int overflow is UB, but practically wraps)
2. `PerfScopeGuard` constructor captures `mAllocBefore = gPerfAllocCount` (a negative or wrapped value)
3. `PerfScopeGuard` destructor computes `int allocDelta = gPerfAllocCount - mAllocBefore` — this can produce negative values even when allocations occurred
4. The clamp `(uint32_t)(allocDelta > 0 ? allocDelta : 0)` hides the negative delta, producing `allocCountDelta=0`
5. Meanwhile, `gPerfAllocBytes` is `size_t` (64-bit on x64 Windows), so it never wraps, and `bytesDelta` remains correct → **`allocBytesDelta > 0` but `allocCountDelta == 0`**

**Secondary bug:** `PerfState::allocationsThisFrame` and `PerfState::totalAllocations` (`perf.h:279-280`) are declared as `int` and are **never incremented anywhere in the codebase**. The frame-level `allocCount` in `PerfFrame` is always 0. The overlay always shows "Allocs: 0 (total: 0)".

**Thread safety:** `gPerfAllocCount` and `gPerfAllocBytes` are non-atomic globals modified by `operator new`, which can be called from any thread (main, audio, asset loading, etc.). This is a data race (UB).

## Files to Change

### 1. `perf/perf-spike.h` — Global counter types + member types
- Line 80: `extern int gPerfAllocCount` → `extern std::atomic<uint64_t> gPerfAllocCount`
- Line 81: `extern size_t gPerfAllocBytes` → `extern std::atomic<uint64_t> gPerfAllocBytes`
- Line 82: `extern size_t gPerfLargestAlloc` → `extern std::atomic<uint64_t> gPerfLargestAlloc`
- Line 29: `PerfScopeCapture::allocCount` from `uint32_t` → `uint64_t`
- Line 30: `PerfScopeCapture::allocBytes` from `size_t` → `uint64_t`
- Line 101: `mAllocBefore` from `int` → `uint64_t`
- Line 102: `mBytesBefore` from `size_t` → `uint64_t`
- Add `#include <atomic>` to the includes

### 2. `perf/perf-spike.cpp` — Definitions + delta computation + validation
- Line 23: `int gPerfAllocCount = 0` → `std::atomic<uint64_t> gPerfAllocCount{0}`
- Line 24: `size_t gPerfAllocBytes = 0` → `std::atomic<uint64_t> gPerfAllocBytes{0}`
- Line 25: `size_t gPerfLargestAlloc = 0` → `std::atomic<uint64_t> gPerfLargestAlloc{0}`
- Line 80: `mAllocBefore = gPerfAllocCount` → `mAllocBefore = gPerfAllocCount.load(std::memory_order_relaxed)`
- Line 81: `mBytesBefore = gPerfAllocBytes` → `mBytesBefore = gPerfAllocBytes.load(std::memory_order_relaxed)`
- Lines 117-120: Rewrite delta computation:
  ```cpp
  uint64_t currentCount = gPerfAllocCount.load(std::memory_order_relaxed);
  uint64_t currentBytes = gPerfAllocBytes.load(std::memory_order_relaxed);
  uint64_t allocDelta = currentCount - mAllocBefore;     // unsigned subtraction, always correct
  uint64_t bytesDelta = currentBytes - mBytesBefore;
  cap.allocCount += allocDelta;
  cap.allocBytes += bytesDelta;
  // Invariant: bytes > 0 implies count > 0
  if (bytesDelta > 0 && allocDelta == 0) {
      Debug::warn(Debug::Category::General,
          "[PERF][ALLOC][WARN] bytes increased without count increase; "
          "investigate allocation hook mismatch");
  }
  ```
- Line 432: Change format from `%u` to `%llu` (with cast to `unsigned long long`) for `allocCount`
- Line 455: `bool allocInstrumented = (gPerfAllocCount > 0)` → `bool allocInstrumented = (gPerfAllocCount.load(std::memory_order_relaxed) > 0)`

### 3. `perf/perf-allocator.cpp` — Atomic increments in operator new
- Lines 12-13: Replace `gPerfAllocCount++; gPerfAllocBytes += size;` with:
  ```cpp
  gPerfAllocCount.fetch_add(1, std::memory_order_relaxed);
  gPerfAllocBytes.fetch_add(static_cast<uint64_t>(size), std::memory_order_relaxed);
  ```
- Lines 14-16: Replace `gPerfLargestAlloc` check-then-set with CAS loop:
  ```cpp
  uint64_t oldLargest = gPerfLargestAlloc.load(std::memory_order_relaxed);
  if (static_cast<uint64_t>(size) > oldLargest) {
      gPerfLargestAlloc.compare_exchange_weak(oldLargest, static_cast<uint64_t>(size),
          std::memory_order_relaxed);
  }
  ```
- Lines 39-41: Same changes for the nothrow `operator new` overload

### 4. `perf/perf-frame.h` — PerfFrame and EntitySnapshot types
- Line 36: `PerfFrame::allocCount` from `int` → `uint64_t`
- Line 37: `PerfFrame::allocBytes` from `size_t` → `uint64_t`
- Line 61: `EntitySnapshot::allocCount` from `int` → `uint64_t`
- Line 62: `EntitySnapshot::allocBytes` from `size_t` → `uint64_t`

### 5. `perf/perf-frame.cpp` — Frame delta computation + format strings
- Lines 60-72: Replace the broken `allocationsThisFrame` / cumulative `gPerfAllocBytes` reads with snapshot-based frame deltas:
  ```cpp
  static uint64_t sPrevAllocCount = 0;
  static uint64_t sPrevAllocBytes = 0;
  uint64_t curCount = gPerfAllocCount.load(std::memory_order_relaxed);
  uint64_t curBytes = gPerfAllocBytes.load(std::memory_order_relaxed);
  frame.allocCount = curCount - sPrevAllocCount;
  frame.allocBytes = curBytes - sPrevAllocBytes;
  sPrevAllocCount = curCount;
  sPrevAllocBytes = curBytes;
  ps.allocationsThisFrame = static_cast<int>(frame.allocCount <= UINT32_MAX ? frame.allocCount : UINT32_MAX);
  ```
  (Keep `allocationsThisFrame` as `int` for overlay compatibility, clamped to avoid overflow there.)
- Add periodic 60-second total logging:
  ```cpp
  static auto sLastAllocLogTime = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration<double>(now - sLastAllocLogTime).count() >= 60.0) {
      Debug::log(Debug::Category::General,
          "[PERF][ALLOC] totals count=%llu bytes=%llu\n",
          (unsigned long long)curCount, (unsigned long long)curBytes);
      sLastAllocLogTime = now;
  }
  ```
- Line 134: `fprintf(f, "    allocs:     %d\n", frame.allocCount)` → `%llu` with cast
- Line 135: `fprintf(f, "    alloc_bytes: %zu\n", frame.allocBytes)` → `%llu` with cast
- Line 201: Format string `allocs=%d` → `allocs=%llu` with cast
- Line 231: `delta("allocs", ...)` — the `delta` lambda takes `int` args; change to `uint64_t`

### 6. `perf/perf-overlay.cpp` — Format strings for display (minimal)
- Lines 89-91, 259-261: Change `snprintf` format from `"Allocs: %d (total: %d)"` to `"Allocs: %d (total: %d)"` — keep as-is since `allocationsThisFrame` and `totalAllocations` remain `int` in PerfState. The value from `allocationsThisFrame` is now correctly populated from `perfCaptureFrame`. No type changes needed here.

### 7. `perf/perf.h` — PerfState counter types
- Line 279: `int allocationsThisFrame = 0` → keep as `int` (display-only, capped at frame-level deltas which are small)
- Line 280: `int totalAllocations = 0` → `uint64_t totalAllocations = 0` (monotonically increasing)

### 8. `perf/perf.cpp` — beginFrame reset + toggleAllocAudit
- Line 35: `s.allocationsThisFrame = 0` → stays as-is (reset each frame)
- Line 1150: `gState.totalAllocations = 0` → stays as-is (reset on toggle, just the type changes)

## Changes NOT Made (Out of Scope)
- No changes to replay, networking, UI, physics, gameplay, renderer, VSync, or frame pacing code
- No changes to audio, effects, or animation systems
- No new profiling subsystems added — only fixing the existing allocation profiler

## Verification
1. `python build_agent.py` — must succeed
2. `python overseer.py` — must pass (or note it's still broken per temporary notice)
3. Manual check: startup logs should show allocation profiler output with correct deltas
4. The `allocCount=0 but allocBytes>0` pattern should no longer occur
