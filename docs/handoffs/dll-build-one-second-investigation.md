# Whole-game DLL build latency handoff

## Objective

Continue the investigation in the shared MiMITA checkout and reduce a warm,
no-change whole-game DLL attempt to **1 second or less**, measured from worker
entry through validated no-op completion. Also reduce changed-build latency as
far as evidence supports. Do not promise that a changed compile plus full DLL
link can complete in 1 second; measure those separately and optimize the
dominant wall-time stage.

The user also wants every attempt attributable: which DLL/generation is being
built, which sources compile or skip, per-file progress, what is taking the
time, and how long the dashboard Build/launch action takes to create a MiMITA
process and reach readiness.

## Repository and safety constraints

- Work in `C:\mimita-v9`, current shared branch, and preserve every existing
  user edit. The checkout currently contains unrelated modified C++, config,
  docs, and a submodule; inspect `git status --short` before touching anything.
- Read `AGENTS.md`, `docs/ROUTER.md`,
  `docs/specs/dev-loop/agent-lanes-build-authority.md`, and the routed build,
  runtime, logging, and task-completion docs before editing.
- Agents edit and observe. Do not run a build, compiler, build worker, ccache,
  dev-loop build request, cold-link trial, or wait-for-build. Do not launch a
  second dev-loop or kill/restart an active run. Allowed dev-loop operations
  are status/list/inspect/ensure-running only. A human owns the live build.
- Do not run tests unless the user asks. State source-level review separately
  from build, runtime, and human evidence.
- Keep exactly one final changelog for the receiving work session.

## Evidence already established

1. The user's screenshot shows a 642-source object-cache scan at 208/642, a
   `SKIP` translation unit with a 46 ms dependency check, and 0.000 s compile,
   link, and whole-attempt values. These fields do not mean the attempt took
   zero: the `46 ms` is one translation unit's check; scan wall time includes
   all units and synchronous progress/reporting work.
2. A later screenshot shows generation 348 failed after about 364 seconds in
   the GUI. The journal's failed event for the attempt reported about 470
   seconds. The old worker/daemon used different start clocks and emitted a
   generic exit code 2; no failed translation-unit event or definitive cause
   was captured. The exact failure cause remains unknown.
3. In a prior observed attempt (generation 346, run `20261119_012255`), 642
   translation units resolved as 628 skips, 13 compiles, and one failed
   compile, with about 231.6 seconds total worker attempt time. Only 354
   journal rows remained; those rows showed about 12 seconds summed dependency
   checks, ~315.5 seconds through the last retained skip, and several seconds
   in progress/record overhead. Journal observation lag is not itself proof of
   worker handler cost. Old daemon code did not receive the attempt context, so
   the newer `.dev/build-timings/<attempt>.jsonl` stream was absent.
4. The worker scans 642 entries serially. For each cache hit it reparses a
   depfile and stats dependencies; `Progress.record()` synchronously updates
   progress JSON/log/timing JSONL and prints a line, which the daemon then
   journals. Shared external headers (including Recast Navigation) appear in
   depfiles and must remain part of freshness checks.
5. A generated `module-identity.h` is an included dependency of module wrappers
   and can invalidate many otherwise unchanged wrapper objects. Measure this
   fan-out before changing the module identity/ABI contract.
6. The active dev-loop had a mismatched/stale loaded daemon relative to disk.
   Do not attribute its missing attempt metadata to the current worker source,
   and do not force a daemon restart while the user's build/run is active.

## Current in-progress changes to review

The preceding agent turn modified:

- `devscripts/game_build_worker.py`: added an early compile pass for directly
  changed/added `.cpp` inputs before the full cache walk. A failed changed
  source should stop early; a successful object should then be reused by the
  normal dependency scan. Review carefully: module-wrapper flags are duplicated
  between preflight and the existing `compile_one()` path. Extract a single
  command/flag builder before extending this. Confirm correct attempt counters,
  preflight phase timing, and that a successful preflight is not compiled a
  second time.
- `devscripts/dev_loop/gui.py`: describes an active cache scan using live phase
  elapsed time, labels per-file skip time as one TU's dependency check, omits
  initialized zero compile/link fields, and shows elapsed whole-attempt time.
  Review every build phase/status for accurate values.
- `docs/specs/dev-loop/build-time-observability.md`: adds changed-source
  preflight and timing budgets, an explicit ≤1-second warm no-change objective,
  and launch latency requirements.
- `docs/specs/dev-loop/dev-loop.md`: adds dashboard acknowledgement and
  keypress-to-client-process target of ≤100 ms when a valid artifact and
  matching healthy server are already available.

These edits have not been built or observed live. Preserve or improve them;
don't assume correctness from the presence of the code.

## Next investigation steps

1. Review current diffs and verify the actual root daemon/worker identity and
   current run state with allowed read-only dev-loop status/inspect commands.
   Never poll repeatedly or wait for the active attempt.
2. Inspect the existing `.dev/build-timings/`, run `events.jsonl`,
   `build/game/progress.json`, worker `progress.log`, and relevant attempt
   summary. Correlate by attempt ID, generation, revision, and monotonic
   boundaries. Keep missing/rotated data explicit.
3. Refactor preflight and normal compile command construction to share one
   function. Preflight only directly changed/added game `.cpp` files; state
   plainly that header-only compiler failures still need dependency discovery,
   and link/coherence failures happen later. Ensure a preflight success leaves
   a current `.o` plus `.d` that the cache scanner recognizes and does not
   double count or recompile.
4. Fix the no-change fast path based on measured wall time. Candidate to assess:
   parse depfiles once, collect unique dependencies, stat common dependencies
   once while retaining per-object correctness, and avoid synchronous
   full-progress rewrites/stdout/journal writes for every skip. Keep a bounded
   2–4 Hz live snapshot, per-source JSONL timing, and a compact run-journal
   summary. Do not discard Recast Navigation or other external dependency
   freshness.
5. Capture explicit intervals: accepted request → worker start; setup and
   toolchain; identity generation; changed-source preflight; dependency checks
   per TU and scan wall; cache-result serialization; worker stdout handling and
   daemon journal handling; actual compiler wall per TU and aggregate compile
   wall; vendor compile; link; coherence validation; publication; final
   completion. Measure with monotonic clocks and UTC ISO 8601 event timestamps.
6. Define the ≤1-second benchmark as a warm **no-change** worker attempt with
   existing valid object/dependency cache and no forced build. Record the
   hardware/toolchain/cache state and compare repeated human-triggered runs;
   don't create a synthetic self-test as a substitute for the real dev-loop.
   A full changed-source compile/link is a separate workload.
7. For dashboard launch timing, confirm the existing keypress-to-client and
   keypress-to-readiness events against the dashboard. The GUI currently calls
   the control path synchronously; an async request/queue may be needed to meet
   a responsive ≤50 ms acknowledgement. Persist stage records under the run's
   `events.jsonl` with the same request ID.
8. Record confirmed regression evidence append-only in
   `docs/regressions/dev-loop/` and its README. The 364s/470s disagreement is
   confirmed timing/accounting failure; the underlying generation 348 compile
   or link cause is not confirmed. Do not call the latter fixed without the
   exact failure artifact and a live recheck.
9. Ask the human/dev-loop owner to trigger the canonical live attempt when the
   changes are ready. Agents must not invoke the build. Inspect the resulting
   run journal, worker progress, final summary, artifact identity, and live
   runtime status separately.

## Acceptance evidence

- Warm no-change: worker start to validated no-op ≤1.000 s on the measured
  reference machine, with the complete request-to-result value also shown.
- Changed build: early compiler errors stop before the 642-unit cache scan for
  directly changed `.cpp` files; successful changed-file preflight is reused.
- Timing: the total wall clock is decomposed into measured stages; no single
  file's dep-check duration is presented as scan/attempt duration; failures
  preserve phase, actionable cause, revision, generation, and attempt ID.
- Correctness: external and project dependencies stay freshness-checked, the
  DLL links/publishes through the canonical dev-loop, and the exact published
  generation is identified in runtime evidence.
- Launch: dashboard responds promptly and reports keypress→Popen and
  keypress→readiness independently, with the same correlated request ID.
- Evidence labels source review, build result, live runtime, and human
  acceptance separately. No result is currently verified live.

## Pointers

- Build/timing specification: `docs/specs/dev-loop/build-time-observability.md`
- Live build authority: `docs/specs/dev-loop/agent-lanes-build-authority.md`
- Dashboard launch contract: `docs/specs/dev-loop/dev-loop.md` §22.2
- Worker: `devscripts/game_build_worker.py`
- Dashboard renderer: `devscripts/dev_loop/gui.py`
- Dev-loop worker event handling: `devscripts/dev-loop.py` around
  `_record_game_build_output()` and game-DLL worker startup
- Existing no-op cache regression: `docs/regressions/dev-loop/incremental-recompile-REG-2026-11-12.md`
- Current run journals: `logs/2026-11-19/<run>/dev-loop/events.jsonl`
- Per-attempt detail path: `.dev/build-timings/<attempt-id>.jsonl`

## Implementation status (2026-11-19)

Source changes for the build fast path were landed in the shared checkout:

- `devscripts/game_build_worker.py`: shared compile command, pooled changed-source
  preflight, complete-marker warm no-op, single-pass batched cache scan, stamp
  coherence, structured terminal failures, `last-attempt.json`, and separate
  worker-entry timing.
- `devscripts/dev_loop/game_build.py`: sortable attempt IDs, stat-keyed manifest
  hash cache, no-op/attempt merge, and `[GAME_SCAN]`/`[GAME_ERROR]`/`[GAME_RESULT]`
  handling.
- `devscripts/dev-loop.py`: scan/error/result journaling, no-op outcome, worker
  latency, and correlated launch acceptance metadata.
- `devscripts/dev_loop/gui.py`: asynchronous control dispatch, monotonic launch
  stage math, and no-op/scan/phase display.
- `devscripts/source_manifest.py`: pruned source enumeration, size/mtime stamps,
  and optional stat-keyed hash reuse.

Live evidence (2026-11-19): a generation 352 publish using the intermediate
lean-scan worker measured a 14.22 s object scan (3.95 s parse, 9.03 s stat,
1,496 unique dependencies) versus 315.5-1716.4 s in earlier attempts. A later
changed build published generation 353 and wrote `complete.json`; the following
auto-coalesced attempt completed as a validated no-op with **0.403 s worker
time**, a **0.096 s** marker scan, 642 skips, no compile/link/publish, and the
same generation 353. The complete request-to-result interval was 36.346 s:
codegen 8.5 s, manifest snapshot 16.6 s, validation 0.04 s, worker-start queue
~3.9 s, worker 0.4 s, and result handling ~3.2 s. The worker-entry objective is
met; the pre-worker stages now dominate the complete interval.

Live failures exposed a real source bug: the new `link_identity` hash passed a
`str` to `hashlib.sha256` and raised `TypeError: Strings must be encoded before
hashing`, failing two attempts at toolchain setup. The call now encodes UTF-8;
the next attempt passed toolchain and published generation 353. The parent now
also keeps a bounded worker-output tail for unhandled worker crashes.

Remaining verification: exercise a compiler-error failure (the live toolchain
crashes were exit-1 failures without the new tail capture loaded), and repeat
a launch with a matching healthy server already running to test the 100 ms
press-to-Popen target. Header fan-out was observed live: a changed
`src/gui/gui-main.h` rebuilt 43 units, and a changed `player-animation.cpp`
preflight was followed by 11 wrapper rebuilds. Generation 348's exit-2 cause
remains unknown. Launch evidence for `dashboard-497bb0f3`: press-to-accept
1.1 ms, queue wait 171.6 ms, client process start 21.80 s, readiness 70.43 s;
the server was not already healthy, so the 100 ms press-to-Popen precondition
did not apply.


Operational warning: during this session the watcher restarted daemons
repeatedly and one daemon crashed while saving state with
`OSError: [Errno 28] No space left on device`. About 18.6 GB was free
afterward. Old `game-g*.dll` generations are about 239 MB each; preserve them
unless the owner explicitly approves cleanup. Confirm the daemon that will run
the acceptance attempt was started after the final source edits.


The generated-identity decoupling is documented as a proposal in
`docs/architecture/live-development/incremental-build-and-header-fanout.md`
and was not implemented; the folded-module ABI/module-contract decision is
still open.

A second live no-op (`game-dll-20261119T151310460091Z-d7eef1`, generation 355,
source generation 28) repeated the result: 0.290 s worker time, 0.052 s scan,
642 skips, 9.927 s complete request-to-result (codegen 3.69 s, manifest
snapshot 5.28 s, validation 0.014 s, worker 0.29 s). The marker path is
repeatable; pre-worker stages dominate the complete interval.

## Deferred many-file target

The 50-changed-file, one-second target is not achievable with native C++
translation units and the monolithic DLL. The deferred architecture (stable
native kernel, power-first hot-layer JIT/script runtime containing the actual
logic, and 8-16 coarse native modules) is specified in
`docs/specs/hotreload/hot-layer-latency-and-language.md`. It does not change the
current Cutover-1 one-game.dll contract and is not implemented. Build-time and
hot-reload work on it is deferred until a human approves the benchmark result.
