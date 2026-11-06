# Destruction Uncaught Exception

Time created: 2026-10-01T16:55:00Z
Time last updated: 2026-10-01T17:07:33Z

Status: UNRESOLVED

Related specification:
`docs/specs/destructible-world/destructible-world.md`

Related changelog:
`docs/changelog/2026-10-01/20261001_170733-crash-safe-shared-collision.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-10-01T11:36:15-04:00` (local; artifact `crash-2026-10-01_11-36-15-20384.txt`)

The game terminated while the human was testing destructible crates. The report
was written to `%LOCALAPPDATA%\MiMITA\crashes` and read:

```text
Exception: UNKNOWN (0x20474343)
Location: KERNELBASE.dll+0xc1ada
Operation: unknown
Module: KERNELBASE.dll
Offset: 0xc1ada
```

The report did not name any game function.

### Expected Behavior

A crash report must identify the failing subsystem (and, where possible, the
function and line) so the cause can be found, and destruction work must not
terminate the process while a valid mesh is still held.

### Actual Behavior

The process aborted with an uncaught exception marker that named only
`KERNELBASE.dll`. No game subsystem, function, or stack was recorded.

### Why This Is Bad

The central destructible-world feature is unusable if it can hard-crash the
client during normal shooting, and an unidentifiable crash cannot be fixed or
even triaged.

### Specification

`docs/specs/destructible-world/destructible-world.md`

Relevant requirements: sections 17/19 (penetration and subtraction), 20/21
(fracture), 44 (explicit work budgets, split heavy work). The engine must keep
running and report failures.

### Confirmed Cause

Not confirmed. What is confirmed:

- `0x20474343` is `STATUS_GCC_THROW`, the MinGW/GCC C++ exception code raised by
  `_Unwind_RaiseException` through `KERNELBASE!RaiseException`. The process died
  from an uncaught C++ exception (for example `std::bad_alloc`,
  `std::length_error`, or a library/Manifold throw), **not** a raw access
  violation.
- The old `src/debug/crash-handler.cpp` only recorded the `RaiseException`
  frame. It had no `std::terminate` handler, never read
  `abi::__cxa_current_exception_type()`, and never symbolized a stack
  (`grep` found no `set_terminate`/`CaptureStackBackTrace`/`SymFromAddr`).
- Candidate throw sites exist in the destruction path (see below), but none is
  proven to be this crash. The exact function is still unknown.

Candidate sites found by inspection (unproven):

- `src/impact/boolean-mesh.cpp`: `std::string` builds, container insertions,
  `gSessions` insert/eviction, and every `manifold::` call run without any
  try/catch; exceptions unwind through the fixed 60 Hz tick.
- `src/impact/destructible-geometry.cpp` `rebuild`: the cut history arithmetic
  and `fillSurface` index the boolean output without validation.
- `addCut` did a redundant second rollback (`pop_back`) after `rebuild` had
  already rolled back, dropping a previously-applied valid cut and desyncing
  the authoritative history (confirmed code defect, not proven crash).

### Attempted Fix 1

Time:
`2026-10-01T13:00:00Z`

Change (crash diagnostics, the prerequisite for any fix):

- `src/debug/crash-handler.{h,cpp}`: added an allocation-free breadcrumb ring
  (`recordCrashBreadcrumb`) dumped into every report; a `std::terminate` handler
  that records `abi::__cxa_current_exception_type()` + `current_exception`
  `what()`; best-effort symbolized stacks via dbghelp plus module+link-address
  fallback so MinGW DWARF frames can be resolved offline with
  `addr2line -e mimita.exe 0x<link address>`.
- Recognized `STATUS_GCC_THROW` (0x20474343) as `CXX_EXCEPTION_GCC` and added an
  explanatory note, instead of the previous `UNKNOWN`.
- Added breadcrumbs at: impact submit/reject, flush, boolean rebuild (start,
  success, rejection, exception), fracture decompose, entity add/remove,
  render-buffer upload, projectile entity hit, and replicated cut apply.
- Added `--crash-exception-selftest`, which throws an uncaught exception and
  verifies the report contains the code label, breadcrumbs, and stack.

Result:

- The report now reads `Exception: CXX_EXCEPTION_GCC (0x20474343)`, prints the
  recorded breadcrumbs, and prints a stack whose frames resolve with
  `addr2line`. Example run is in the changelog.
- A real C++ exception type and `what()` are captured when the terminate handler
  is the entry point; for GCC SEH throws the OS filter runs first, so the label
  plus breadcrumbs identify the subsystem.

### Attempted Fix 2 (crash-safe rejection)

Time:
`2026-10-01T13:30:00Z`

Change:

- `src/impact/destructible-geometry.cpp`: fixed the confirmed `addCut` double
  rollback; guarded the `cuts.size() - pendingCutCount` underflow; bounds-checked
  `fillSurface`; added `meshIsSane` (non-empty, triangle-aligned indices in
  range, finite vertices) and a finite-volume check that reject a bad boolean
  result and keep the previous valid mesh; wrapped `booleanSubtractIncremental`
  and `booleanDecomposePieces` in try/catch that logs, breadcrumbs, and rolls
  back to the last valid revision.
- `src/impact/impact-system.cpp`: reject non-finite `ImpactEvent` inputs before
  any geometry math.
- `src/network/multiplayer-physical-entities.cpp`: reject non-finite spawn and
  cut packet fields before they reach the boolean/mass code.

Result: all destruction self-tests still pass; a new stress self-test passes
repeatedly. The original crash has **not** been reproduced.

### Attempted Fix 3 (stress test)

Time:
`2026-10-01T14:00:00Z`

Change: added `--destruction-stress-selftest`, exercising 480 deferred projectile
shots on one crater, forced motion, repeated dumbbell fracture, and a client
mirror reproducing the server cut mesh, asserting finite state, non-shrinking
history, bounded entity count, and matching client geometry.

Result: PASS 5/5 consecutive runs. The crash did not recur, but the stress test
does not prove the cause.

### Corrected Code

See the changelog's exact old/new snippets. Highlights:

- `destructible-geometry.cpp` `addCut`: removed the redundant `pop_back`.
- `destructible-geometry.cpp` `rebuild`: clamped `pendingCutCount`, validated the
  output mesh/volume, wrapped the subtract in try/catch.
- `crash-handler.cpp`: recognized `STATUS_GCC_THROW`, added breadcrumbs,
  terminate handler, and symbolised/resolvable stacks.

### Fix

Not claimed. The crash diagnostics are in place and the destruction path is
hardened and validates its output, but the original crash has not been
reproduced or its exact function proven. Per the regression rules this stays
`UNRESOLVED` until a report names the failing subsystem/function and the
behavior is confirmed fixed.

### Proof

Human review: pending. Run the game and shoot crates; if it crashes, the new
`crash-*.txt` will name the subsystem via breadcrumbs and provide a resolvable
stack.

Automated proof:

- `mimita.exe --crash-exception-selftest` writes a report labeled
  `CXX_EXCEPTION_GCC (0x20474343)` with breadcrumbs and a resolvable stack.
- `mimita.exe --destruction-stress-selftest` PASS (5/5 runs).
- `--destructible-selftest`, `--moving-crate-selftest`,
  `--destruction-replication-selftest` PASS.

### Solution

Not yet. Status remains `UNRESOLVED`.
