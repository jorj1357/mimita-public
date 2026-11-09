// 2026-11-09T18:28:06Z
/* purpose
* preserve the build-speed diagnosis and policy that restored confidence in the dev loop
* separate build scheduling, compiler throughput, incremental boundaries, and measured causes
* this file DOES NOT prove that high priority alone caused the prior long build
* this file DOES NOT replace the build-time regression, build authority, or runtime evidence
*/

# Gold reference: build speed, one dev-loop, and liveable iteration

- Timestamp: `2026-11-09T18:28:06Z` (13:28:06 EST)
- Audience: MiMITA developers and AI agents working on builds, agent lanes, or hot reload
- Status: operating direction; full cold-build speedup still needs a matched comparison
- Related regression: [`../regressions/performance/build-time-REG-2026-11-08.md`](../regressions/performance/build-time-REG-2026-11-08.md)
- Related policy: [`../specs/dev-loop/agent-lanes-build-authority.md`](../specs/dev-loop/agent-lanes-build-authority.md)
- Related timing contract: [`../specs/dev-loop/build-time-observability.md`](../specs/dev-loop/build-time-observability.md)
- Related integration record: [`../changelog/2026-11-09/20261109_181852-lane-import-integration-build-speed.md`](../changelog/2026-11-09/20261109_181852-lane-import-integration-build-speed.md)

## What happened

The user saw builds taking around 20–25 minutes and first considered whether a
specification was unclear or a hidden behavior was involved. Inspection found a
real build-policy mismatch in the previous source: the canonical EXE build
used at most four jobs on a machine with 12 logical processors, and the
DevLoop launched it with below-normal process priority. This was a concrete
throughput setting problem, not evidence that the gameplay specification was
wrong.

The user also described frustration during the delay. After working with
DeepSeek V4.1 Flash, the user reported that the EXE had built and tests were
underway, and that the workflow felt back in a good state. That report matters
as human context. Technical claims still come from the actual build artifact
and runtime records.

## Current build direction

```text
source edit
  -> one persistent dev-loop notices and coalesces changes
  -> dev-loop owns the EXE request
  -> one machine-wide EXE slot compiles at a time
  -> all logical processors are available to that build
  -> Windows worker runs at HIGH_PRIORITY_CLASS
  -> incremental objects are reused when their dependencies are unchanged
  -> artifact is published, then the selected executable is exercised
```

- Agents edit and observe. They never start `build.py`, a compiler, or a
  competing build loop.
- Keep the dev-loop open; it is the automatic incremental builder and the
  authority for build identity, publication, launch, and status.
- Keep one active EXE build slot machine-wide. A later request waits; it must
  not compile at the same time as the current request.
- Use `os.cpu_count()` logical processors for the EXE compiler job count. On
  this machine that is 12 logical processors, across 8 physical cores.
- On Windows use `HIGH_PRIORITY_CLASS`, the priority class immediately below
  real-time. Do not use real-time priority.
- Keep module work behind the existing `ModuleHost` owner. Move behavior behind
  hot module boundaries so ordinary gameplay edits need fewer cold EXE builds.

A waiting `build.py` process can still exist while the scheduler slot is busy;
that process has not started compiler work. If “exactly one spawned build
process” becomes a strict requirement, queue before spawning the worker and
verify that behavior separately. Do not describe the present single-active-slot
guarantee as proof that only one build-related process exists.

## What the evidence proves

The 2026-11-09 integration build published:

- Artifact: `.dev/builds/2026-11-09T18-11-10.953010Z/mimita.exe`
- Build ID: `2026-11-09T18:11:10.953010Z:f538b96de6225c213ac8bbe8dd672e9479074157eb8c0f1048b33ad7ad73a6c6`
- Build output: 1 source compiled, 622 skipped, 33.66 seconds.

This is evidence that a one-file incremental build was quick and that the
EXE published. It is not a cold-build timing result. The same integration
record includes successful compile-time checks and several live runs, but some
modes timed out during startup or map synchronization, and server tick stalls
remained. Build success, runtime behavior, and human acceptance stay separate.

The long lane attempt is less conclusive: the pasted investigation showed
object-file counts rising from 162 to 169 while the compiler was alive, despite
a status timer frozen at 680 seconds. Later the lane status showed 2,688
seconds and a current source filename, but at a subsequent inspection the
process and scheduler slot were gone with no recorded terminal result or
published artifact. Record its outcome as unknown; do not claim either a
successful 45-minute build or a still-running compiler from stale status alone.

## Do not confuse build speed with build scope

High priority and more compiler jobs can reduce elapsed time for independent
compile work. They do not decide how many translation units are invalidated.
A wide shared-header edit or cold object/PCH state can still require a large
rebuild. The dependency graph, source snapshot, cache, PCH, memory pressure,
link, and publication phases all affect wall time.

Therefore:

- “The build used low priority/few jobs” is confirmed for the old policy.
- “That policy alone caused the 25-minute build” is not yet proven.
- “Low priority caused a full rebuild” is inaccurate: source dependencies
  determine what must rebuild; priority affects scheduling time.
- The 33.66-second one-source build cannot be compared directly with a
  588-source cold rebuild.
- Use the per-attempt phase/per-file timing contract before claiming which
  phase dominated or how much the policy change improved a full build.

## Repeatable build-speed check

1. Confirm the exact checkout/lane, daemon PID, source generation, scheduler
   slot, and build identity. Reuse the existing dev-loop.
2. Confirm the active worker's effective `HIGH_PRIORITY_CLASS` and job count
   equals the machine's logical processor count. Do not infer this from the
   intended source setting alone.
3. Wait for the current EXE slot; do not start another build or compile command.
4. Record scheduler wait and memory wait separately from compilation.
5. Record source snapshot, toolchain, cache/PCH state, compiled/skipped counts,
   per-file timings, link, and publish time.
6. Compare only runs with matching build mode, source/dependency change,
   toolchain, cache state, and comparable memory conditions.
7. Confirm that one `.cpp` edit stays incremental and separately measure any
   cold/header-fan-out build. Do not use one as proof of the other.
8. After publication, run the target behavior in the exact executable and
   inspect its `events.jsonl`; keep visual/gameplay acceptance separate.

## What should improve next

The current priority/jobs correction improves the worker policy. Long-term,
move gameplay and tool behavior behind safe hot-reloadable module boundaries
so a normal behavior edit does not require a full EXE compile and link. Preserve
one canonical build owner and one active EXE slot while doing that migration.

The user should not need to guess whether a long delay means a bad spec,
concealed work, or a hung compiler. Build status should identify the active
phase and file, scheduler/memory waits, progress heartbeat, effective job
limit, and final result. A recent object file or a running process is progress
evidence; a successful artifact and a live journal answer different questions.

## Cache reuse and apparent stalls across lanes

The 2026-11-09 18:52 UTC lane trace completed in 699.798 seconds after
compiling 624 units, skipping none, with `cold_build=true` and
`link_reason=exe_missing_or_zero`. Its supplied ccache stats were 0 hits,
2,466 misses, and 35 errors. The same trace showed `player-animation.cpp`
producing an object and the linker starting later; the last filename shown by
status was stale progress, not proof of a hang.

The EXE lane worker already configures the shared `<coordinator>/.dev/ccache`
and lane-root normalization. The module worker previously did not; it now
receives the same cache directory, `CCACHE_BASEDIR`, and `CCACHE_NOHASHDIR`
before compiler/link subprocesses. The shared-workspace guide now names the
correct `.dev/ccache/` location. Cache reuse across different worktree roots
still requires a real second-lane measurement. The pasted shell `ccache -p`
does not prove the exact settings seen by the worker.

Keep project checkout paths relative where possible and derive lane/cache/temp
roots from the coordinator and lane roots. Machine-specific compiler/GLFW
installations remain external dependencies with environment overrides; they
must not become lane artifact or scheduler roots. A path audit should flag
new absolute checkout, cache, or temporary paths while retaining only the
minimal configurable tool-install fallbacks required by this machine.
