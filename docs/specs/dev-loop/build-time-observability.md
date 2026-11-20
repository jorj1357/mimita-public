// 2026-11-08
/* purpose
* specify how EXE and whole-game DLL builds expose progress and account for elapsed time
* identify whether time is spent waiting, compiling, linking, or publishing
* turn repeated build measurements into evidence for focused improvements
* this file DOES NOT change build authority, scheduling limits, or build inputs
* this file DOES NOT promise an accurate completion percentage
* this file DOES NOT make timing data proof of runtime or gameplay behavior
*/

# Build time and progress observability

## Audience and authority

This specification is for the dev-loop, its canonical EXE and whole-game DLL build workers, the
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

The manifest snapshot may reuse a prior content hash when the same path still
has the same size and `mtime_ns`; a changed stat re-hashes the file. This keeps
the request-to-worker interval short without weakening the revision identity
for ordinary edits. Delete `.dev/source-hash-cache.json` to force a full
re-hash, for example after restoring a file and its timestamps by hand.

The build writes `build/progress.json` during the attempt. It reports the active
phase (`setup`, `pch`, `compiling`, `linking`, `staging`, `publishing`,
`build_complete`, `completed`, or `failed`), current translation unit, compiled
and skipped counts, and phase durations. Phase transitions and current-file
updates must be flushed as they happen. The dev-loop status and dashboard read
this file; they must not infer that a build is stuck on the last `[CXX]` line
while the worker is linking or publishing.

For each whole-game DLL attempt, append a `dev-loop.game-build.started` event
to the dev-loop journal with the source generation, revision, a concise note
that saved edits were captured, and bounded changed paths when available.
Append one `dev-loop.game-build.file` event for every compiled or failed
translation unit, with its path, result, elapsed compile seconds, revision,
generation, and UTC observation time. Cache-skip detail must be persisted as
one row per translation unit in the attempt's `.dev/build-timings` stream, not
as one synchronous progress write, stdout line, and journal event per skip.
Report cache-scan activity through a bounded live snapshot at 2-4 Hz and a
compact periodic `dev-loop.game-build.scan` journal event carrying scanned,
skipped, stale, and elapsed values. Emit a final scan summary with the attempt
result. Keep the bounded recent compiled/failed results in
`build/game/progress.json` so the dashboard can show live CXX timing without
parsing journal history. Do not claim a natural-language agent prompt or
author identity unless the build input explicitly supplies it.

A cold build is identified by `compiled` substantially matching the full EXE
translation-unit count and `skipped` being zero. For such an attempt, report the
ccache direct-hit/miss counts and cache size. `player-animation.cpp` or any other
single filename is not evidence of a hang; a stall claim requires no progress
timestamp change, a live worker, and the current phase.

The timing record also reports `scheduler_wait_s`, `pch_s`, `compile_s`,
`link_s`, `staging_s`, ccache hits/misses/errors, and whether the source changed
during the attempt. Whole-game DLL attempts additionally report `build_cause`
(`no_op`, `changed_sources`, `header_fanout`, `identity`, `mixed`, or `rebuild`),
`changed_sources`, `changed_headers`, `changed_header_fanout`,
`header_fanout_top` (bounded top dependents), and `identity_wrapper_count`.
Whole-game attempts also report `source_snapshot`, the per-attempt immutable
source tree used by the worker; `source_root` in the worker record identifies
the same tree. Snapshot creation time is separate from manifest, compile, and
link time.
Dependency stats are collected in parallel and the scan reports parse and stat
time separately; neither is compile time. A new lane with identical source should become an
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
- A failure must persist and journal its failed phase, a bounded actionable
  cause, the affected path when one exists, the source revision, the attempt
  ID, and the terminal status. A generic nonzero exit code alone is not an
  acceptable failure explanation.

### Changed-source preflight and cache-scan budgets

When the source-generation report names changed or added game `.cpp` files,
compile those translation units before scanning the full object cache. A
compiler failure in one of those files must stop the attempt immediately and
record the path, compiler output, preflight duration, attempt ID, and source
revision. A successful preflight object is reused by the normal cache scan.
Header-only changes still require dependency discovery to identify affected
translation units; a preflight cannot promise to detect those compiler errors
before that discovery. Linker and final source-coherence failures likewise
remain failures of later phases.

For every attempt, distinguish per-translation-unit dependency-check time from
the wall time spent scanning all depfiles, cache-result recording, worker
output handling, and daemon journal writes. Never present one file's `46 ms`
dependency check as the explanation for a multi-minute scan. While a phase is
active, display its monotonic elapsed time. Do not display initialized `0.000s`
compile/link values as completed work; report a phase result after it runs.
The end-to-end attempt clock begins at accepted request and includes setup,
cache scan, compilation, linking, validation, publication, and any queue delay
owned by this attempt.

The no-change warm-build objective is at most one second from worker start to
validated no-op result on the reference machine, with a separate request-to-
worker measurement. This is a performance objective to measure, not a promise
that a changed translation unit, full DLL link, or cold build completes in one
second. Optimize the largest measured wall-time component first while keeping
external dependency freshness (including Recast Navigation) correct.

A many-file changed-build target is deliberately outside this build contract.
It cannot be met by compiling 50 native C++ translation units and relinking the
239 MB game DLL; measured link and compile costs are recorded in
[`../hotreload/hot-layer-latency-and-language.md`](../hotreload/hot-layer-latency-and-language.md).
That deferred specification owns the hot-layer language/runtime design,
50-file save-to-active budget, and coarse native-module latency; this document
continues to own no-change and ordinary native build measurement.

### Warm no-change fast path

When the requested manifest revision already matches the published
`build/game/current.json`, the published artifact exists, the build mode,
flags identity, object cache, toolchain, PCH, and link-input stamps match, and
a lean dependency scan finds every object fresh, the worker completes as a
validated no-op. A no-op must not write a new `module-identity.h`, must not
compile, link, rename, or publish a DLL, and must not increment the published
generation. It reports success with the existing generation, DLL path, and
code hash plus `no_op: true`; the dev-loop journals that outcome and does not
treat it as a new artifact. The attempt still receives its own sortable attempt
ID, timing stream, summary, and evidence rows.

The lean scan may parse each depfile once and stat each unique dependency once.
It must retain per-object freshness and must include project, PCH, external
(including Recast Navigation), compiler, and link-input stamps. A source
snapshot stamp may be trusted only for the attempt whose snapshot produced it;
a stamp mismatch re-hashes the file before reporting coherence.

Persist per-attempt phase and per-source timing rows beneath
`.dev/build-timings/` in UTC ISO 8601 JSONL, with a final summary and stable
attempt ID. Include request acceptance, worker entry, toolchain/setup, changed-
source preflight, each dependency check, cache-scan wall time, each compile,
queue wait, progress-file write, worker output-handler time, link, coherence
validation, staging/publication, and terminal result. The GUI reads a bounded
live progress snapshot; it does not scan the full JSONL stream during refresh.

### Launch latency objective

The dashboard acknowledges a launch keypress in the next rendered frame
(target at most 50 ms) and sends the control request asynchronously so a slow
daemon socket response cannot freeze the view. Record correlated UTC and
monotonic stage boundaries for dashboard press, request enqueue/acceptance,
daemon dispatch, artifact validation/staging, server reuse or readiness,
client `Popen`, and configured launch readiness. Persist these stage records in
the run's `events.jsonl` and surface the same request ID in the dashboard.

When a valid artifact is already selected and a healthy matching server is
running, target at most 100 ms from dashboard keypress to client process
creation. Show server startup/replacement, build wait, map readiness, join
readiness, and other readiness checks as separate elapsed stages. The target
does not collapse these distinct operations into a single zero-second claim.

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
  `no_op`, `succeeded`, or `failed`);
- active phase, phase start time, phase elapsed time, total attempt elapsed
  time (request acceptance basis), worker-entry elapsed time, and known
  scheduler wait durations;
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

Write an append-only event stream and a final summary owned by the build
system for every attempt, including failed attempts, at
`.dev/build-timings/<attempt-id>.jsonl` and
`.dev/build-timings/<attempt-id>.json` in that checkout. The stream holds full
per-file and phase timing; the JSON holds the bounded attempt summary and links
to the stream. Keep the active stream append-only and the final summary
immutable. Dev-loop status/history links to both by attempt ID. A successful
artifact's identity also links to the attempt ID. Do not put unbounded
per-file detail into the shared status file.

The summary contains:

- schema version, attempt ID, lane/checkout, build mode, source generation,
  source identity, compiler/toolchain identity, job limit, and cache location
  identity (not credentials or private environment values);
- canonical UTC timestamps and monotonic elapsed durations for lifecycle and
  phase boundaries;
- summary counts for compiled/skipped units, slowest sources, and phase totals;
- phase results, exit code/error, compiled/skipped counts, link output size,
  publish validation/hash result, and final artifact ID/hash when successful;
- source-change/follow-up facts and whether the output belongs to the latest
  source generation.

The JSONL stream contains one start and one completion row per applicable
phase and translation unit, including queue delay and cache result when known.
Cache-skip rows may be appended in bounded batches; the live snapshot remains
bounded and no journal event is required for each skip. Every terminal attempt
also writes `build/game/last-attempt.json` with its attempt ID, revision,
generation, status, failed phase, and bounded actionable error, so a failure
remains attributable after the live progress file is replaced. Do not overwrite
prior attempt records when a retry starts. Retention follows the existing
managed artifact policy.

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


## Header fan-out and incremental build budget (2026-11-15)

The build worker already has a source+hash object cache and `.d` dependency
files. The missing operational view is header fan-out.

Required reporting:

- top headers by dependent translation-unit count;
- PCH dependents and whether a PCH rebuild invalidated all objects;
- exact objects affected by a header edit;
- whether a build was incremental or forced a cold object set;
- any `cold-link-trial` object deletion, with the deleted count and requester.

Rules:

- No automatic object deletion during ordinary live work. A cold-link trial
  must be explicit and recorded.
- A header edit may rebuild its dependents; record the fan-out before treating
  the build as unexpectedly slow.
- Do not infer a hang from one slow translation unit; use live phase progress.
- Store persisted timing in the per-attempt `.jsonl` stream and `.json`
  summary specified above. Aggregate history must not be committed to git.
- The goal is not "one header changes one file". The goal is one header with
  one owner and a measured, shrinking dependent set.

See `docs/architecture/live-development/incremental-build-and-header-fanout.md`.

## Whole-game DLL generation trace

The root dev-loop builds one immutable whole-game generation at a time. Saved
source notifications that arrive during a build coalesce into the next
generation; they are not one DLL per save or per request ID. The queue view
shows one active generation and at most one next coalesced generation, with
request counts shown as triggers rather than DLL counts.

The trace begins when the dev-loop accepts a game-DLL build request and covers
code-generation checks, source-manifest snapshot and validation, source
coherence hashing, toolchain resolution, generation identity and object-cache
setup, dependency freshness checks for every source, compiler queue wait and
compile per source, optional GLAD compile, link, final coherence check, hash,
and publication. Each phase has UTC start/end times plus monotonic elapsed
milliseconds. A build attempt has one stable attempt ID, source generation,
manifest revision, and output generation.

The current worker selects manifest sources with owner `game`, then excludes
`src/game/probe/npc-target-probe-module.cpp` from the whole-game artifact. It
generates `build/game/identity/module-identity.h`; module wrapper translation
units include it and receive the module-entry symbol mapping. Debug builds use
`-Og -g1`, release builds use `-O2 -g`, both use C++17, project/vendor include
paths, the game-DLL defines, dependency-file output, and optional forced PCH.
Every stale source becomes an object in the persistent flags-keyed object
cache. The linker response file includes all selected object paths, the
optional diagnostic `src/glad.c` object, `libmanifold.a`, GLFW search path,
and the Windows/system libraries in `game_build_worker.py`. The resulting
staging DLL is source-coherence checked, renamed to `game-g<N>.dll`, hashed,
and published via `current.json`.

The generation number and source revision are written into
`module-identity.h`. Since that generated header is an included dependency of
the module wrappers, changing it can invalidate those wrapper objects on each
generation even when their own C++ files are unchanged. Treat the repeated
wrappers as a concrete fan-out candidate; moving generation identity out of
compile-time inputs could reduce rebuilds, but only after verifying the module
identity/ABI contract.

A successful build also writes `build/game/objects/<flags>/complete.json`,
which records the revision, generation, object stat stamps, external
dependency paths, compiler/link identity, and published code hash. The warm
no-op path may validate that marker, the published artifact, and the identity
header without rereading every depfile; any mismatch falls back to the full
per-object dependency scan.

For each translation unit, record dependency-check start/end and duration,
whether a fresh object was reused or compilation was needed, executor queue
wait, compiler start/end, compiler duration, per-event progress-file write
time, and dev-loop output-handler time. Keep all per-source rows in the
attempt's JSONL timing stream; keep only a bounded tail in the live
`progress.json`. The status/dashboard may refresh from the bounded progress
file but must not parse the full timing stream on every refresh.

`total_seconds` measures the entire attempt from accepted build request to
publication. It is not the sum of compile and link alone. Display compile,
link, cache scan, validation, and publication separately. A percentage may
describe source units resolved during the dependency scan or compilation,
using the source count as its denominator. Link, validation, and publication
show elapsed time without a percentage.

Agents may attach a short explanation to the next whole-game DLL generation
with:

```text
python devscripts/build-note.py --changes "NPC behavior fix" --files src/npc/npc-behavior.cpp
```

Before the first compiler-input edit for a DLL change, the agent must queue its
note. Queue a separate note for each independent agent contribution before
those edits are saved. If one coalesced DLL attempt contains changes from
several agents, it consumes and preserves every pending note; it must not
select only the newest note or merge away a contribution. The attempt's live
progress, `[GAME_NOTE]` event, final JSON summary, and final dev-loop journal
event all carry the complete note list. Do not wait for a build to start before
queuing a note.

The script writes a bounded note in the repository's ignored `.dev` state.
The note is explanatory metadata, not proof of which source files compiled.
Changed paths come from the source-manifest diff. Include an author label only
when it is explicitly supplied to the script.

Each terminal whole-game DLL result must be appended to the active dev-loop
`logs/<date>/<run-id>/dev-loop/events.jsonl` as
`dev-loop.game-generation.ready` or `dev-loop.game-build.failed`. The result
record includes the build-attempt ID, source generation, revision, success,
duration, DLL generation/path, code hash when available, request IDs, all
attached change notes, changed compiler inputs, and relative paths to the
attempt `.dev/build-timings/*.jsonl` stream and final `.json` summary. These
run-journal events are the searchable outcome; the attempt JSONL and summary
remain the detailed per-build phase and timing record. A missing note list is
recorded as empty and visible as missing metadata; build success does not
retroactively supply an agent explanation.

### Observed generation 346 trace (2026-11-19)

The live journal for run `20261119_012255` shows generation 346 resolving 642
game translation units: 628 dependency-clean object reuses, 13 compiled
objects, and one failed compile. The worker had reached roughly 194 seconds of
attempt elapsed time before the last cached-unit checks completed; its
compile phase then ended at 231.6 seconds. Summed compiler time is not wall
time because source compilation runs concurrently. The failure occurred in
`src/network/server-npcs.cpp`; the captured GCC output reports an
`rtmintrin.h` include failure and `stdout: Bad file descriptor`. The cause is
not established; do not treat this as a Juggernaut runtime failure.

The 13 compile inputs were the 11 module wrappers
`src/navigation/navigation-policy-module.cpp`,
`src/network/network-policy-module.cpp`,
`src/render/world-rendering-module.cpp`,
`src/npc/npc-perception-module.cpp`, `src/map/map-geometry-module.cpp`,
`src/mapeditor/map-editor-module.cpp`,
`src/physics/movement/actor-contact-policy-module.cpp`,
`src/world/chunk-collision-module.cpp`, `src/npc/npc-behavior-module.cpp`,
`src/render/actor-rendering-module.cpp`, `src/worldgen/infinite-worldgen-module.cpp`,
plus `src/npc/npc-combat.cpp` and `src/network/multiplayer-shots.cpp` (the
change report listed the latter as changed). `src/network/server-npcs.cpp` was
the other changed source and its compile failed. The reason
`src/npc/npc-combat.cpp` was rebuilt is not established by the retained
change report. Thus, 11 of 13 successful object compiles were wrapper fan-out
from the generated identity header, not direct edits to those wrapper files.

In the 244 per-file rows still present in the run journal, the median delay
between the worker's file-completion timestamp and the dev-loop's journal
timestamp was 14 ms, but the maximum reached 25,057 ms. This tail indicates
that output handling or process scheduling also stalled during the scan; it is
not yet attributable to one owner. Progress-file writes were usually in the
tens of milliseconds, with some spikes, and do not alone explain the long
cache-scan phase. The exact contribution of dependency checks, progress
writes, dev-loop journal handling, and machine contention is unavailable
because this daemon did not write the attempt timing stream. The next trace
should compare `dependency_check_ms`, progress-write duration, and
`dev_loop.file_output_handled` duration per source. If handler time dominates,
batch status persistence while keeping journal events live; if dependency
checks dominate, reduce depfile filesystem work. Do not tune compiler jobs
from the overlapping per-file durations.

### Observed generation 351 trace (2026-11-19)

Generation 351 published successfully but took 2016.97 s of worker-elapsed
time. Its measured phases were: changed-source preflight 201.23 s for three
directly changed sources, object/dependency scan 1716.38 s for 628 cache
skips, compile 61.42 s, link 29.91 s, coherence validation 5.48 s, and publish
2.50 s. The scan rows summed only 58.13 s of dependency checks, while the
measured per-skip progress/record overhead summed 1658.10 s. That isolates
synchronous per-skip progress persistence and reporting-wait as the dominant
measured scan component; it does not yet separate worker file writes, heartbeat
lock contention, atomic-replace retries, pipe backpressure, and daemon journal
handling. The retained last 100 rows had small overhead (about 24 ms average),
so the cost concentrated earlier in the scan and was not preserved by the
bounded live tail. Bulk skip reporting and per-source JSONL rows are required
before the next trace can reconcile the remainder.

### Verified generation 353 warm no-op (2026-11-19)

Generation 353 was published by a changed build and wrote
`build/game/objects/b10e3a31b3ae8637/complete.json`. The following
auto-coalesced attempt (`game-dll-20261119T150618864547Z-0fb052`) validated the
marker and finished as a no-op:

- worker entry to completion: 0.403 s;
- object scan: 0.096 s, 642 skipped, 0 stale, `parse_ms=0`, 43 external
  dependency stamps;
- toolchain_setup 73.6 ms, identity_and_cache 19.6 ms, object_cache_scan
  99.2 ms, no_op 9.2 ms;
- no identity write, compile, link, publish, or generation increment;
- complete request-to-result: 36.346 s (codegen 8.5 s, manifest snapshot
  16.6 s, validation 0.04 s, worker-start queue ~3.9 s, worker 0.4 s, result
  handling ~3.2 s).

The run journal carried `no_op: true`, the same generation and code hash, and
`worker_total_seconds: 0.403`. This verifies the batched scan and the separated
worker clock live. The pre-worker codegen and manifest stages remain the
dominant complete-interval cost.

A second live no-op (`game-dll-20261119T151310460091Z-d7eef1`, generation 355,
source generation 28) repeated the result: 0.290 s worker time, 0.052 s scan,
642 skips, 9.927 s complete request-to-result (codegen 3.69 s, manifest
snapshot 5.28 s, validation 0.014 s, worker 0.29 s). Repeated no-op validation
is expected to stay under one second at the worker boundary while pre-worker
codegen and manifest work remain separate measured stages.

## Change-to-evidence and build-friction (2026-11-20)

This specification measures build stages; it does not by itself measure whether
an agent proved the change works. The end-to-end metric is defined in
[change-to-evidence-latency](change-to-evidence-latency.md):
intent/edit → build request → queue → worker → publish/activate → required
`events.jsonl` observation.

For every request record the agent, task/issue, request ID, class, queue wait,
and wait reason. For every build record the artifact/build ID, game generation,
`build_cause`, changed paths/headers, source snapshot, source-changed-during-
build flag, and phase durations. For every pinned test record the EXE and game
DLL identity, run ID, activation stage, and decisive event time.

Create a `build-friction` issue when:

- an EXE rebuild is required;
- `cold_change_guard` classifies a change as `COLD`, `COLD_ABI`,
  `HEADER_REVIEW`, or `UNKNOWN`;
- `build_cause` is `header_fanout`, `rebuild`, or `identity`;
- a compile, link, scan, queue, or activation stage exceeds its measured class
  budget;
- a test proof exceeds its class budget;
- an agent waits for a build/test and the wait is avoidable or above budget.

Record every wait even when it is below budget. An agent may open a
build-friction issue for any wait it considers unnecessary. Auto-created issues
start in `needs_triage` and carry the exact evidence. The original observation
is append-only; a human makes the final decision on the resolution.

No budget is lowered to make a result pass. A build success is not runtime
evidence; build time and event-proof time are reported separately.
