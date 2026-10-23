# Juggernaut Thread and Snapshot Evidence

Time created: 2026-10-22T22:04:47Z

Branch: `20261011zt`

HEAD: `cbfa4b0df4e56b6f217c45dbe986b49df599b50a` (unchanged; no commit created)

## Goal

Add bounded live-journal evidence for the previously unexplained Juggernaut
snapshot entity-loop time, sample per-thread CPU use, run the current release
executable through the Dev Loop, and inspect the exact run journal.

## Changes made

### `src/network/multiplayer-tick.cpp`

Existing outer `Network::SnapshotApply`, `EntityLoop`, and
`MembershipCleanup` scopes were already present before this session's edits.
This session added nested scopes to distinguish local-player state handling,
remote player registry access, replica lookup and map insertion, NPC life
reset, replica creation, avatar synchronization/rebind, interpolation insert,
and new-replica presentation. It also separated the NPC avatar-bind journal
write and console creation line from the rest of replica creation.

Before, replica maps were accessed inline:

```cpp
bool existsBefore = replicas->find(entity.networkEntityId) != replicas->end();
Player& p = (*replicas)[entity.networkEntityId];
bool isNew = !existsBefore;
EntityInterpolationState& interpolation = (*interpolationMap)[entity.networkEntityId];
```

After, lookup and state insertion are separately measured:

```cpp
{
    MIMITA_PERF_SCOPE("Network::SnapshotApply::ReplicaLookup");
    existsBefore = replicas->find(entity.networkEntityId) != replicas->end();
}
{
    MIMITA_PERF_SCOPE("Network::SnapshotApply::ReplicaStateAccess");
    replica = &(*replicas)[entity.networkEntityId];
    interpolationState = &(*interpolationMap)[entity.networkEntityId];
}
```

The NPC avatar bind diagnostic is now isolated as
`Network::SnapshotApply::NpcAvatarBindLog` (around lines 489-512), and the
per-actor creation print is isolated as
`Network::SnapshotApply::ReplicaCreateConsoleLog` (around lines 516-523).
Other new labels are at the scope sites around lines 228, 355, 387, 399, 412,
448, 429, 434, 472, 477, 483, 529, 555, 575, 584, and 589. The labels include
the source location in each resource-window record.

### `src/perf/perf-frame.cpp`

Before, the resource window measured only the thread executing the frame
boundary with `GetThreadTimes(GetCurrentThread(), ...)`. After, Windows thread
enumeration records start/end CPU-time deltas for threads belonging to the
client process. Each sample includes thread ID, whether it is the main thread,
and CPU milliseconds. The profiler samples every 20 resource windows (about
six seconds in this run) to reduce measurement overhead. The window event marks
whether a thread sample was taken; zero-valued per-thread totals on unsampled
windows are not measurements.

The Windows capture is bounded to 256 thread records and uses
`CreateToolhelp32Snapshot`, `OpenThread(THREAD_QUERY_LIMITED_INFORMATION)`,
and `GetThreadTimes`; summary fields are emitted in the existing
`performance.resource-window` event.

### `src/perf/perf-allocator.cpp`, `src/perf/perf-spike.cpp`, and `src/perf/perf-spike.h`

Before, the allocation hook updated only process totals:

```cpp
gPerfAllocCount.fetch_add(1, std::memory_order_relaxed);
gPerfAllocBytes.fetch_add(static_cast<uint64_t>(size), std::memory_order_relaxed);
```

After, both intercepted allocation entry points also call
`perfRecordThreadAllocation(size)`. The function in `perf-spike.cpp` gives each
thread one of 128 fixed atomic slots, assigned on that thread's first
allocation, and updates that thread's count and byte totals. Thread allocation
deltas and overflow counts are sampled in `performance.resource-window`.
This adds two atomic increments per intercepted allocation in the diagnostic
build. Scope allocation deltas remain process-wide and can include overlapping
worker-thread allocations. No call-stack sampler was added.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/performance/performance.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/skills/logging-checker-v1.md` — PASS for categorized, windowed records;
  thread CPU snapshots are sampled at a bounded interval.
- `docs/skills/efficiency-checker-v1.md` — review completed; Windows thread
  enumeration was reduced from every 60-frame summary to every 20th summary
  after measuring the initial diagnostic overhead risk.
- `docs/skills/spec-behavior-review-v1.md` — NOT APPLICABLE; this is diagnostic
  instrumentation and does not change gameplay behavior.
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/regressions/README.md`

## Validation and runtime evidence

- `git diff --check` reports two pre-existing trailing-whitespace lines in
  `docs/specs/dev-loop/dev-loop.md`; no whitespace issue was reported in the
  source changes from this session.
- Canonical Dev Loop release builds succeeded as 2859, 2861, and 2863. Build
  2864 was then cold-linked after the Dev Loop daemon restart and also succeeded.
- Visible build 2861 used
  `C:\mimita-v9\.dev\builds\2861\mimita.exe` and journal
  `C:\mimita-v9\logs\2026-10-22\20261022_220158\events.jsonl`; `--versioninfo`
  confirmed that exact path and run ID. Its runtime metadata matched client and
  server identity `2861:3b1e5c01e3a93807f76ead514dd67c233b28f4b0d0cfe6ae04dd19bc3f537dd4`.
- Visible build 2861 reached 67 NPCs. From 9,240 steady-state rendered frames
  after the first five seconds, 1,211 exceeded the 4.17 ms budget: 13.11%.
  Weighted average frame time was 3.38 ms. This fails the requested under-1%
  miss rate. The journal stopped at `t=38.38`; the main client thread remained
  alive and consumed CPU until the Dev Loop stopped the test client.
- A second visible run with build 2863 and the per-thread allocation hook
  reached 67 NPCs but stopped writing after `t=36.51`; the client remained
  alive. Its incomplete journal does not provide a full 60-second acceptance
  sample.
- Headless build 2864 used
  `C:\mimita-v9\.dev\builds\2864\mimita.exe` and journal
  `C:\mimita-v9\logs\2026-10-22\20261022_221358\events.jsonl`; `--versioninfo`
  confirmed that executable, path, and run ID. Client and server build identity
  matched. The run reached 67 remote NPCs and continued logging through
  `t=45.66`, then stopped writing before a full minute.
- Headless build 2864's measured steady portion was 7,140 frames with 3,979
  above-budget frames (55.73%), weighted average 5.479 ms, and worst frame
  1,063.19 ms. These are headless measurements and cannot establish visible FPS
  acceptance.
- The build 2864 worst frame was inside snapshot replication of the 67 new
  replicas: `ReplicaCreate` totaled 978.8 ms over 67 calls, of which 953.95 ms
  was self time. The entire 60-frame resource window reported 1,583,418
  allocations / 451,525,850 bytes. Per-thread counters accounted for
  1,570,565 allocations / 450,874,490 bytes (about 99.9% of bytes): main thread
  1,442,812 allocations / 124,049,611 bytes; worker TID 19476, 3,610
  allocations / 248,087,835 bytes; other worker threads accounted for most of
  the remainder. Main-thread CPU was 1,453.125 ms and total sampled thread CPU
  was 2,125 ms in that 60-frame window.
- In that same window,
  `Network::SnapshotApply::ReplicaCreateConsoleLog` overlapped 32,660,016
  process-wide allocated bytes over 67 calls. Per-scope allocation deltas are
  global and can include worker allocations that overlap the scope, so this is
  a lead for a follow-up, not proof that `printf` allocated all those bytes.
- Headless build 2864's final client journal event was
  `performance.resource-window` at `t=45.66`. The process remained alive; its
  main thread (TID 3076) stayed `Running` and accumulated 1,453.1 ms CPU time in
  the next two seconds. The Dev Loop then stopped the test client. No crash
  record was observed. The dedicated server was subsequently stopped through
  the Dev Loop after inspection was complete.
- The per-thread allocation capture reconciled closely for build 2864's large
  burst, unlike a prior build 2863 interval whose thread sample did not align
  with the large per-window global delta. Keep that earlier mismatch visible;
  do not combine it with the later, well-aligned sample.
- Windows Performance Recorder CPU stack capture was attempted for the first
  stuck client but Windows denied profiling privilege (`0xc5585011`). The
  exact function responsible for journal cessation remains unidentified.
- None of these client runs reached a full 60 seconds before journal output
  stopped. The strict 240 FPS requirement is not met. Repeated journal
  cessation with a live, CPU-consuming main thread is confirmed; a game crash
  or its cause is not.
- No automated tests were run. Human visual acceptance was not performed.

## Pre-existing work preserved

The working tree already contained unrelated edits before this session,
including changes to analytics, the Dev Loop, gameplay-issue documentation,
engine/network/performance code, the performance watcher, earlier changelogs,
and regression notes. They were preserved and are not claimed here.

## Remaining evidence needed

An elevated Windows CPU stack trace is needed to identify the exact function
executing after journal output stops. Thread-specific allocation attribution
or an external heap trace is needed before treating allocation traffic as the
cause of memory growth. A clean 60-second Juggernaut sample with no journal
cessation and under 1% of frames above 4.17 ms is still required; the target is
not met by these runs.
