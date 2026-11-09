// 2026-11-08
/* purpose
* specify how EXE builds expose progress and account for elapsed time
* identify whether time is spent waiting, compiling, linking, or publishing
* turn repeated build measurements into evidence for focused improvements
* this file DOES NOT change build authority, scheduling limits, or build inputs
* this file DOES NOT promise an accurate completion percentage
* this file DOES NOT make timing data proof of runtime or gameplay behavior
*/

# Build time and progress observability

## Audience and authority

This specification is for the dev-loop, its canonical EXE build worker, the
dashboard/status readers, and people diagnosing slow builds. It supplements
the build lifecycle in [`dev-loop.md`](dev-loop.md) and the build ownership in
[`agent-lanes-build-authority.md`](agent-lanes-build-authority.md). The
dev-loop remains the EXE build authority; the worker remains responsible for
compilation and linking.

The purpose is to answer, from each build attempt:

1. When was the change or request observed, and how long did debounce and
   scheduler waits take?
2. Which concrete phase is active now, and which source file is compiling?
3. How long did setup, PCH generation, translation-unit compilation, linking,
   validation, and publication take?
4. Which phase dominated wall time, and what evidence supports the next
   improvement?

## Cache reuse and live phase reporting (2026-11-09)

A lane build must reuse the coordinator's shared ccache when source, compiler,
flags, and precompiled-header inputs match. The build worker normalizes cache
inputs so absolute worktree paths do not create different keys:

- compile source, object, and project-owned include arguments use paths relative
  to the worktree root;
- `CCACHE_DIR` points at the shared coordinator cache;
- `CCACHE_BASEDIR` points at the current worktree root;
- `CCACHE_NOHASHDIR=1` prevents the working directory from changing the key;
- shared external paths (for example the installed GLFW toolchain) are the only
  expected absolute include paths.
- the precompiled header is compiled from a relative source/output path with
  working-directory and file-prefix normalization, so its content does not
  contain the lane worktree path; otherwise every lane PCH gets a different
  cache key even when the source is identical.

The build writes `build/progress.json` during the attempt. It reports the active
phase (`setup`, `pch`, `compiling`, `linking`, `staging`, `publishing`,
`build_complete`, `completed`, or `failed`), current translation unit, compiled
and skipped counts, and phase durations. Phase transitions and current-file
updates must be flushed as they happen. The dev-loop status and dashboard read
this file; they must not infer that a build is stuck on the last `[CXX]` line
while the worker is linking or publishing.

A cold build is identified by `compiled` substantially matching the full EXE
translation-unit count and `skipped` being zero. For such an attempt, report the
ccache direct-hit/miss counts and cache size. `player-animation.cpp` or any other
single filename is not evidence of a hang; a stall claim requires no progress
timestamp change, a live worker, and the current phase.

The timing record also reports `scheduler_wait_s`, `pch_s`, `compile_s`,
`link_s`, `staging_s`, ccache hits/misses/errors, and whether the source changed
during the attempt. A new lane with identical source should become an
incremental or cached build, not another full cold build.

## Time and identity rules

- Store event timestamps as UTC ISO 8601 with `Z`, following the universal
  format in [`../../architecture/time-and-formatting/time-and-formatting.md`](../../architecture/time-and-formatting/time-and-formatting.md).
- Measure elapsed durations with a monotonic clock. Never derive a duration by
  subtracting wall-clock timestamps.
- Identify every record by build-attempt ID, checkout/lane ID, source
  generation, build mode, and source snapshot identity. On success, also link
  the published artifact ID and executable SHA-256. Failed attempts still
  receive an ID even though they have no artifact ID.
- Build-attempt IDs are unique and sortable: use a UTC timestamp with
  microseconds plus a collision-resistant suffix. The artifact ID remains the
  separate identity of a successful published build.
- Distinguish request/change observation, build-worker start, first useful
  compiler output, compile completion, link completion, and publication
  completion. Do not label the worker's start time as the time compilation
  began if it was waiting for scheduler capacity.
- Preserve the final result and last completed phase for failures. A failed
  build must not inherit the timing summary or result of the previous build.

## Required lifecycle phases

Record start and end monotonic durations for every applicable phase. A phase
that does not apply is marked `not_applicable`; an unfinished phase remains
`running` and exposes its elapsed duration.

| Phase | Required measurements |
|---|---|
| Change detection and debounce | First observed input change, latest coalesced change, debounce start/end, source generation |
| Scheduler wait | Time waiting for the EXE slot; each wait reason and interval, including memory threshold waits; slot acquisition/release |
| Snapshot and setup | Snapshot/source-identity work, toolchain/cache discovery, build preparation |
| PCH | PCH cache decision and generation duration; inputs/hash when available |
| Compilation | Queue wait, start/end and outcome for each translation unit, aggregate compile wall interval, number of compiled/skipped units, and available cache hit/miss result |
| Link | Start/end, outcome, executable size when available |
| Validation and staging | Runtime-file staging, executable validation/hash, artifact metadata write, and each duration separately |
| Publication | Artifact copy/publish start and completion, result, artifact identity |
| Follow-up | Whether source changed during the attempt, queued generation, and when the follow-up was accepted |

Also record the first useful compiler output time and the last progress/output
time. Compiler output is useful progress only when it identifies a phase,
translation unit, or concrete error. Repeated generic status lines must not
make a stalled compile appear active.

For parallel compilation, report both the compilation wall interval and the
sum of per-translation-unit durations. The sum can exceed wall time and must
not be presented as elapsed build time. If process CPU time is available,
report it separately from wall time. Do not continuously sample processes at a
rate that materially slows builds.

## Live status and dashboard

While an attempt is pending or running, status must expose:

- attempt ID, lane/checkout, source generation, build mode, and latest
  successful artifact identity;
- overall state (`pending`, `waiting`, `building`, `linking`, `publishing`,
  `succeeded`, or `failed`);
- active phase, phase start time, phase elapsed time, total attempt elapsed
  time, and known scheduler wait durations;
- current translation unit while compiling, plus bounded recent compiler
  output and the latest error;
- last progress timestamp and worker/daemon liveness or heartbeat sufficient
  to distinguish a live long-running phase from stale status;
- whether a newer source generation is waiting behind the current attempt.

The build worker must flush phase transitions and current-file updates as they
occur. Piped stdout must not leave the dashboard showing an obsolete message
while compilation continues. Status must not invent a percentage when the
compiler cannot provide a trustworthy denominator and completion estimate.

Keep full per-file timing in the attempt record, not in the bounded live status
tail. Status and dashboard remain responsive while the worker runs. Build
observability must not start another compiler, alter scheduler policy, change
the artifact being run, or interrupt a running game.

## Persisted timing record

Write one machine-readable timing record owned by the build system for every
attempt, including failed attempts, at
`.dev/build-timings/<attempt-id>.json` in that checkout. An active attempt may
update its own record; once terminal, its record is immutable. Dev-loop
status/history must link to it by attempt ID. A successful artifact's identity
must also link to the attempt ID. Keep at least the newest 30 attempt records
per checkout so timing comparisons survive the shorter published-EXE
retention. Do not create an unmanaged debug file or put unbounded per-file
detail into the shared status file.

The record contains:

- schema version, attempt ID, lane/checkout, build mode, source generation,
  source identity, compiler/toolchain identity, job limit, and cache location
  identity (not credentials or private environment values);
- canonical UTC timestamps and monotonic elapsed durations for lifecycle and
  phase boundaries;
- per-translation-unit path, queue/start/end duration, result, and cache
  outcome when known;
- phase results, exit code/error, compiled/skipped counts, link output size,
  publish validation/hash result, and final artifact ID/hash when successful;
- source-change/follow-up facts and whether the output belongs to the latest
  source generation.

The record is append/finalized once per attempt; an active attempt may expose
its live partial form. Do not overwrite prior attempt records when a retry
starts. Retention follows the existing managed artifact policy.

## Diagnosis and use of results

Each completed attempt should produce a concise summary with total elapsed
time, time in scheduler waits, time in build phases, compiled/skipped unit
counts, the slowest translation units, cache outcomes, and publication time.
Where the evidence is available, compare with recent attempts that match build
mode, toolchain, and cache state. Clearly label a comparison as unavailable
when those conditions differ or history is insufficient.

Use the measured breakdown to direct investigation:

| Evidence | Investigation direction |
|---|---|
| Scheduler wait dominates | Check competing EXE builds, slot ownership, and wait intervals |
| Memory wait dominates | Check free-memory observations and configured threshold; do not infer compiler slowness |
| Many translation units compile after a header edit | Inspect dependency fan-out and whether the header belongs at the cold-build boundary |
| One or a few translation units dominate | Inspect those units, included headers, compiler cache outcomes, and toolchain behavior |
| PCH dominates or repeatedly rebuilds | Compare PCH inputs and invalidation causes |
| Link dominates | Measure linker wall/CPU time and linked object/executable size |
| Validation or publication dominates | Separate hashing, copying/staging, runtime-file work, and artifact metadata costs |
| Long phase has no recent progress | Check worker and daemon liveness before deciding it is stuck |

Reports may suggest a next investigation, but must label a cause as confirmed
only when the recorded evidence isolates it. Do not automatically change job
counts, scheduler policy, build mode, source ownership, cache configuration,
or hardware assumptions from one timing sample.

## Acceptance evidence

Validate the instrumentation using the existing dev-loop and its normal
compiler pipeline. Capture at least:

1. A no-op or unchanged-input build, showing skip behavior and any fixed
   setup/link/publish cost.
2. A small `.cpp` change, showing the affected translation unit, cache result,
   link, and publication.
3. A header change with multiple dependents, showing the compile fan-out and
   aggregate phase time.
4. A scheduler or memory wait when one occurs, showing that wait separately
   from compiler time.

For each run, reconcile phase durations with total monotonic elapsed time,
confirm the final result belongs to the recorded source generation, and verify
the dashboard updates during the active phase. These are build-process claims;
they do not prove runtime behavior or gameplay acceptance.
