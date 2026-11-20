// 2026-11-19 04:07 UTC
/* purpose
* record the DLL build warm no-op, lean scan, attribution, and launch dashboard changes
* separate source review from build, runtime, and human acceptance
* this file DOES NOT claim a live build result
* this file DOES NOT claim human gameplay acceptance
*/

# DLL build fast path, lean cache scan, and failure attribution

# Task

- Task ID: 20261119_dll_build_one_second_implementation
- Session ID: 20261119_dll_build_one_second_implementation
- Owner: dev-loop
- Focus: warm no-change DLL attempt <=1s, changed-build early failure, attributable phases, dashboard launch acknowledgement
- Summary: Implemented a marker-backed warm no-op, batched/single-pass object-cache scanning, snapshot-stamp coherence, sortable attempt identity, structured failure rows, parent hash-cache reuse, and asynchronous dashboard control dispatch. No build, compiler, dev-loop request, or runtime exercise was run by the agent.
- Status: worker-entry warm no-op verified live at 0.403 s; complete request-to-result is 36.3 s; changed build verified; failure/header/launch acceptance remains
- Date, time, timezone: 2026-11-19T04:07:31+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 214849f07f9a4bc045d1461b1dbaf463a2084a85
- Final commit: not committed by this session

# Pre-existing changes

- The checkout already contained unrelated C++, config, docs, submodule, and dev-loop edits.
- The previous turn had added changed-source preflight, GUI phase-display wording, and spec text. This session preserved and extended that work.
- The running dev-loop daemon was started before these edits and does not contain them; a human restart is required for daemon/GUI source. Worker code is read from disk per attempt.
- Live generation 351 had published before implementation; its measured scan was 1716.38 s with summed `record()` overhead 1658.10 s. The generation 348 exit-2 cause remains unconfirmed.

# Requested behavior

- Warm no-change whole-game DLL attempt completes from worker entry through validated no-op in at most one second on the reference machine.
- No-change attempts reuse the published generation/DLL without writing identity, compiling, linking, publishing, or incrementing generation.
- Changed `.cpp` failures stop before the full cache walk; successful preflight objects are reused.
- Every attempt exposes candidate identity, per-phase elapsed time, per-source rows, and an actionable failure phase/path.
- The dashboard acknowledges launch keypresses without blocking and reports acknowledgement, client start, and readiness separately by request ID.

# Specification alignment

- `docs/specs/dev-loop/build-time-observability.md`: added the warm no-op contract, stamp-based coherence, batched skip evidence, worker-entry versus request clocks, failure attribution, and marker persistence. Updated the launch latency acceptance.
- `docs/specs/dev-loop/dev-loop.md` section 22.2: dashboard sends control requests asynchronously, acknowledges the keypress, and reports separate correlated stages.
- `docs/architecture/live-development/incremental-build-and-header-fanout.md`: recorded the measured eleven-wrapper generated-identity fan-out and the gated runtime-identity proposal.
- `docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md`: appended generation 351 evidence. Generation 348 remains open and unexplained.

# Exact implementation changes

## File: `devscripts/game_build_worker.py`

- Added one shared compile-command builder and module flag map used by preflight and normal compilation (`module_extra_flags`, `compile_command`, `compile_one`, `run_compile_batch`).
- Preflight now runs changed/added `.cpp` units in a bounded pool, stops starting new work on failure, verifies the resulting object/dep through `dep_is_stale`, and records the preflight phase.
- Added sortable attempt context use, attempt-scoped timeline handling, and structured terminal events: `[GAME_ERROR]` for failures and `[GAME_RESULT]` for every terminal status.
- Added `build/game/last-attempt.json` with attempt ID, revision, generation, status, failed phase, bounded errors, and timing.
- Replaced per-skip `record()` calls with a single-pass scan: read each depfile once, stat each unique dependency once, collect external dependencies, and batch per-source rows into the attempt timing stream. Live progress is a bounded 2-4 Hz snapshot; heartbeat writes only when dirty; atomic-replace retries are bounded for live progress and longer only for published artifacts.
- Added `build/game/objects/<flags>/complete.json`: revision, generation, flags/link/compiler identity, object stat stamps, external dependency paths, and published code hash.
- Added a fast no-op path that validates identity bytes, the complete marker, published artifact, PCH, and external dependency mtimes; it finishes `no_op` with the existing generation and does not link or publish. A missing/invalid marker falls back to the lean dependency probe and writes a marker after a confirmed no-op.
- Coherence now uses manifest `size`/`mtime_ns` stamps and re-hashes only changed files.
- Skipped legacy object seeding when the persistent cache already has objects.
- Added `worker_total_seconds` so worker-entry latency is separate from request-to-result latency.

## File: `devscripts/dev_loop/game_build.py`

- Attempt IDs are sortable UTC timestamps with a suffix.
- Writes/consumes source hash stamps through the manifest and passes `worker_total_seconds`.
- Consumes `last-attempt.json`, merges `no_op`, failed phase, errors, and worker metrics into the result, summary, journal payload, and progress.
- Reports worker output for `[GAME_SCAN]`, `[GAME_ERROR]`, and `[GAME_RESULT]` and records handler durations for them.
- Skips `ready.json` publication for a no-op and reports the existing generation instead.
- Preserved the pre-existing exception wrapper that records a durable failed attempt.

## File: `devscripts/dev-loop.py`

- Journals scan, error, and terminal result events from the worker.
- Does not persist full daemon state for each scan snapshot.
- Journals `no_op`, worker latency, revision, attempt ID, failure phase, errors, last-attempt path, and timing/summary paths in the terminal generation event.
- Adds a correlated `request_accepted` launch stage carrying dashboard press epoch, accept latency, and queue wait.
- Initializes the stage build number safely before artifact selection.

## File: `devscripts/dev_loop/gui.py`

- Dashboard control requests now run on one background thread; keypress acknowledgement no longer waits for a socket round trip.
- Launch timing is derived from daemon monotonic `elapsed_ms`, not wall-clock subtraction.
- Displays acknowledgement, client start, and readiness separately.
- Shows worker-entry elapsed, scan parse/stat/unique-dependency detail, phase results, and an explicit `NO-OP` state; suppresses not-yet-run zero totals.
- Preserves all agent notes and changed paths for attribution.

## File: `devscripts/source_manifest.py`

- Records source and header `size` and `mtime_ns` next to the hash with a retry if a file changes during hashing.
- Added an optional stat-keyed hash cache for the parent manifest snapshot.
- Pruned `enumerate_repository_cpp` to avoid walking `.git`, `build`, assets, logs, and other non-source trees while preserving `src/website` and external sources.

# Measured source-review evidence

- A live generation 352 publish ran with the intermediate lean-scan worker while this session was still editing: object scan 14.22 s (parse 3.95 s, stat 9.03 s, 1,496 unique dependencies), 40 compiled, 602 skipped. The previous scans for comparable attempts were 315-1716 s. That build did not write the complete marker or link/compiler/worker-total fields because those edits landed after the worker process started.
- Generation 351 worker total 2016.97 s: preflight 201.23 s, scan 1716.38 s, compile 61.42 s, link 29.91 s, coherence 5.48 s, publish 2.50 s.
- Generation 351 scan summed dependency checks 58.13 s and summed per-skip `record()` overhead 1658.10 s.
- Read-only dep-file measurement: 642 depfiles, 1,844 unique deps, parse 640-860 ms, unique stat 869-872 ms on this machine. This is why the complete marker fast path is required for a one-second no-op rather than rereading depfiles every attempt.
- Read-only `marker_objects` on the current cache: 1,420 `.o` entries in 7.9 ms; marker validation with 642 expected objects returned true in 28.6 ms.
- Read-only enumeration after pruning: 808 `.cpp` files; 44.7 s previously, about 2.5 s now.

# Local tests

- Ran a temporary-directory Python test of the new pure build-tool logic: identity write/match, depfile parsing and continuation, external classification, `dep_is_stale`, object keys, stamped SHA, complete-marker positive/negative validation (generation, compiler, object stamp, external mtime, missing object), manifest hash-cache reuse and change detection, dashboard launch-stage parsing, and repository enumeration counts.
- All checks passed. This is build-tool logic evidence only; it is not a DLL build, runtime, or human acceptance.
- The live attempts later covered the `link_identity` branch and exposed the unencoded hash input that the pure tests did not exercise.

# Live acceptance evidence

- A live changed build published generation 353: 43 compiled, 599 skipped, object scan 21.71 s (17.93 s parse, 3.31 s stat, 1,474 unique dependencies under heavy machine load), compile 162.69 s, link 36.38 s, worker total 222.71 s. It wrote `build/game/objects/b10e3a31b3ae8637/complete.json` and the new link/compiler identity fields.
- The following auto-coalesced attempt (`game-dll-20261119T150618864547Z-0fb052`, source generation 21) completed as a validated no-op:
  - worker entry to no-op completion: **0.403 s** (objective <=1.000 s);
  - object scan: **0.096 s**, 642 skipped, 0 stale, marker path (`parse_ms=0`, 43 external stamps);
  - phases: toolchain 73.6 ms, identity 19.6 ms, scan 99.2 ms, no-op 9.2 ms;
  - generation 353 reused; no new generation, identity write, compile, link, or publish;
  - complete request-to-result: **36.346 s**, decomposed as codegen 8.5 s, manifest snapshot 16.6 s, validation 0.04 s, worker start queue ~3.9 s, worker 0.4 s, and result handling ~3.2 s.
- The run journal recorded `dev-loop.game-build.result` and `dev-loop.game-generation.ready` with `no_op: true`, the same generation/code hash, `worker_total_seconds: 0.403`, and the attempt timing stream path.
- Two earlier live attempts failed at `toolchain_setup` with `TypeError: Strings must be encoded before hashing`. The new `link_identity` hash passed a `str` to `hashlib.sha256`; the worker-level test had not exercised that branch. The call now encodes UTF-8; the next attempt passed toolchain and published generation 353. The parent now also retains a bounded worker stdout/stderr tail in the attempt summary and failure timing for crashes that exit without a structured error.
- A second no-op (`game-dll-20261119T151310460091Z-d7eef1`, generation 355, source generation 28) measured **0.290 s** worker time, **0.052 s** scan, 642 skips, and **9.927 s** complete request-to-result (codegen 3.69 s, manifest snapshot 5.28 s, validation 0.014 s, worker 0.29 s). The marker fast path is repeatable.
- Launch evidence from dashboard request `dashboard-497bb0f3`: press-to-accept 1.1 ms (target <=50 ms), queue wait 171.6 ms, client process start 21.80 s, readiness 70.43 s. The server was not already healthy, so the 100 ms press-to-Popen precondition did not apply.

# Build and runtime evidence

- No compiler, build worker, dev-loop build request, cold-link trial, or gameplay runtime was invoked.
- Operational warning observed during verification preparation: the watcher restarted daemons repeatedly and one daemon crashed while saving state with `OSError: [Errno 28] No space left on device`. About 18.6 GB was free afterward, but old `game-g*.dll` generations are roughly 239 MB each. Disk space must remain available. The newest daemon started after the final source edits, but its state file still read `starting`; confirm it is healthy before acceptance.
- A build note was queued for the next whole-game DLL attempt: `deeb0baadd60` (one unrelated pending note already existed).
- Acceptance still requires: first changed/publish build writes the complete marker; a following warm no-change build proves worker-entry <=1.000 s; a changed `.cpp` proves early failure; a header change reports fan-out; a launch proves press-to-ack, press-to-client, and readiness by request ID.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `devscripts/game_build_worker.py` | serial scan with per-skip reporting | marker no-op, lean scan, shared compile builder, structured failures | speed and attribution | `git checkout -- devscripts/game_build_worker.py` |
| `devscripts/dev_loop/game_build.py` | published-result driver | no-op/attempt/last-attempt-aware driver | attribution and no-op semantics | `git checkout -- devscripts/dev_loop/game_build.py` |
| `devscripts/dev-loop.py` | file/stage events | scan/error/result events and launch ack stage | dashboard evidence | `git checkout -- devscripts/dev-loop.py` |
| `devscripts/dev_loop/gui.py` | synchronous control and wall-clock stage math | async control, monotonic stages, no-op/scan display | responsiveness and truthful timing | `git checkout -- devscripts/dev_loop/gui.py` |
| `devscripts/source_manifest.py` | full walk and full rehash | pruned walk and optional stat hash cache | request-to-worker latency | `git checkout -- devscripts/source_manifest.py` |
| `docs/specs/dev-loop/build-time-observability.md` | preflight/spec draft | no-op, batching, failure, marker, clock rules | specification first | `git checkout -- docs/specs/dev-loop/build-time-observability.md` |
| `docs/specs/dev-loop/dev-loop.md` | dashboard launch requirements | async ack and separate stages | specification first | `git checkout -- docs/specs/dev-loop/dev-loop.md` |
| `docs/architecture/live-development/incremental-build-and-header-fanout.md` | unmeasured fan-out | measured fan-out and gated proposal | architecture record | `git checkout -- docs/architecture/live-development/incremental-build-and-header-fanout.md` |
| `docs/regressions/dev-loop/dll-build-time-accounting-REG-2026-11-19.md` | earlier evidence only | generation 351/353/355 evidence appended | append-only regression record | remove the appended sections only |
| `docs/regressions/dev-loop/README.md` | entry 29 summary | later-evidence lines appended | keep the index current | remove the appended lines only |
| `docs/specs/hotreload/hot-layer-latency-and-language.md` | absent | deferred 50-file hot-layer architecture/language spec | record the requested target without code changes | remove the file |
| `docs/ROUTER.md` | no hot-layer route | routes the new spec | discoverability | `git checkout -- docs/ROUTER.md` |
| `docs/specs/hotreload/hotreload.md` | no deferred target | pointer to the new spec | keep Cutover-1 authority | `git checkout -- docs/specs/hotreload/hotreload.md` |
| `docs/specs/code-wrapper/code-wrapper.md` | informal note | formal spec pointer | connect prior product intent | `git checkout -- docs/specs/code-wrapper/code-wrapper.md` |
| `docs/architecture/live-development/module-migration-plan.md` | one-DLL rule only | split evaluation note and spec pointer | architecture record | `git checkout -- docs/architecture/live-development/module-migration-plan.md` |
| `docs/handoffs/dll-build-one-second-investigation.md` | no deferred target | 50-file target pointer | handoff continuity | remove the appended section |

# Documentation-only follow-up (2026-11-19)

- Recorded the requested 50-changed-file, one-second target as a deferred spec:
  `docs/specs/hotreload/hot-layer-latency-and-language.md`.
- The spec states that native C++ 50-TU compile/link cannot meet one second on the
  reference laptop, rejects per-file DLLs, and defines the power-first hot-layer
  runtime bake-off (C#, C++ JIT, AngelScript, LuaJIT, TinyCC, DSL) plus coarse
  native modules.
- Cross-linked the spec from the router, hot-reload spec, code-wrapper note,
  build-time spec, architecture records, and handoff.
- No code was edited, no build was run, and implementation is explicitly deferred.

# Regressions and verification status

- Verified live: batched scan reporting, marker fast path, generation reuse, and
  the separated worker clock. Warm no-ops measured 0.403 s and 0.290 s worker time.
- Verified live: a changed generation publishes, then writes the complete marker;
  a following no-change attempt reuses it without compile/link/publish.
- Confirmed and fixed: the new `link_identity` hash initially passed a `str` to
  `hashlib.sha256`, failing two live attempts at toolchain setup.
- Unconfirmed: generation 348's exact exit-2 cause. Do not mark it fixed.
- Unverified: the deliberate compiler-error failure path with the new output-tail
  capture, and the 100 ms press-to-Popen precondition with a healthy matching
  server. Dashboard async control is source-reviewed, not directly timed.

# Remaining human decisions

- Implement the generated-identity runtime decoupling? The measured fan-out rebuilds eleven wrappers on every generation/revision change. The design in the architecture document requires an ABI/module-contract decision; it was not implemented.
