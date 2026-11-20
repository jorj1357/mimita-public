# DLL build time was hidden by per-file timing and split clocks

- Recorded: 2026-11-19
- Status: FIXED for the observed display, scan-reporting, and split-clock
  failures — a live generation 353 warm no-op measured 0.403 s worker time.
  Generation 348's underlying compile/link cause remains unknown.
- Scope: Whole-game DLL cache-scan and failed-attempt timing, not Juggernaut
  gameplay and not a proven compiler/linker root cause.


## Evidence

The dashboard screenshot for generation 348 showed a 642-source cache scan at
208/642, an individual skipped translation unit reported as a 46 ms dependency
check, and `compile wall time`, `link`, and `whole attempt` all at `0.000s`.
Another screenshot showed `FAILED` after about 364 seconds. The matching
`dev-loop.game-build.failed` journal event reported about 470 seconds and only
generic worker exit code 2. No failed-file event or retained compiler/linker
diagnostic established the cause of that failure.

An earlier generation 346 trace in run `20261119_012255` covered 642 units,
with 628 skips, 13 compiles, and one failed compile. Its attempt reached about
231.6 seconds. The retained journal segment did not contain all per-file rows;
its summed dependency checks were much shorter than observed wall time. The
loaded daemon did not pass the attempt context required by the newer worker
timing stream, so this run has no complete `.dev/build-timings/<attempt>.jsonl`
record.

## Failure mechanism established

- The GUI rendered initialized zero-valued compile/link/total fields as actual
  durations before those phases had run.
- The GUI's per-file `46 ms` is one translation unit's dependency-check time;
  it is not the 642-entry scan wall time or attempt wall time.
- The worker performs a serial cache scan and synchronously persists/emits a
  row for each skipped translation unit. The available old-daemon evidence is
  insufficient to assign all missing wall time to any one of stat calls,
  worker output, or daemon event handling.
- The GUI attempt clock and dev-loop failure event clock do not agree for this
  attempt. Their start/observation boundaries were not captured consistently.

## Required response

Keep active phase elapsed time visible and suppress not-yet-run zero phase
totals. Record per-TU dependency time separately from scan/attempt wall time.
Correlate request acceptance, worker entry, output handling, and terminal
failure by attempt ID with monotonic durations. Preserve failure phase and
actionable compiler/linker output.

Compile directly changed/added `.cpp` inputs before the full cache walk so
those compiler errors fail promptly. This cannot catch header-only errors
before dependency discovery, nor link/coherence errors before their phases.
Retain the user's requirement that external headers such as Recast Navigation
participate in freshness checks.

## Verification status

The dashboard and worker changes are present in the working tree but were not
built or tested. The active daemon/build was left untouched. Generation 348's
underlying exit-2 cause remains unknown; do not state that it has been fixed.

## Additional evidence — generation 351 (2026-11-19)

The next live attempt (batch `game-dll-e6094819e0b649d0`, published
generation 351) completed and gave a measured breakdown that the earlier
daemon could not record. Worker-elapsed total was 2016.97 s:

- changed-source preflight: 201.23 s for three directly changed sources;
- object/dependency scan: 1716.38 s for 628 cache skips;
- compile: 61.42 s; link: 29.91 s; coherence: 5.48 s; publish: 2.50 s.

Inside the scan, summed dependency checks were only 58.13 s, while the
`record()` progress/log persistence overhead that this worker summed per skip
was 1658.10 s. That confirms synchronous per-skip progress reporting as the
dominant measured scan cost for this attempt. It does not yet separate worker
file writes, heartbeat lock contention, atomic-replace retries, pipe
backpressure, and daemon journal handling; the retained bounded tail had only
about 24 ms average overhead, so the dominant rows were earlier in the scan and
were not retained.

The generation 348 exit-2 cause remains unconfirmed; generation 351's
`coherence_failed` path on a later attempt is the same worker exit code but is
not proof of what failed generation 348. Future failures now persist a failed
phase, path, and actionable output in `last-attempt.json`, the attempt timing
stream, and the run journal.

## Lean-scan live check — generation 352 (2026-11-19)

A later published generation 352 attempt (using the in-progress lean-scan
worker, before the final marker and identity-stamp edits) reported an
object/dependency scan of 14.22 s, decomposed as 3.95 s parse and 9.03 s stat
over 1,496 unique dependencies, with 40 compiled and 602 skipped objects. This
is a 22x-121x reduction from the 315.5-1716.4 s scans observed earlier, but it
is not yet the complete-marker fast path and does not include the final
no-op accounting. The underlying scan-loop stall is not fully explained by
that attempt because it started before the final worker edits; the next live
warm no-op must demonstrate the <=1 s result.

## Verified live — generation 353 changed build and warm no-op (2026-11-19)

A live changed build published generation 353 with 43 compiled and 599 skipped
objects: object scan 21.71 s (17.93 s parse, 3.31 s stat, 1,474 unique
dependencies), compile 162.69 s, link 36.38 s, worker total 222.71 s. It wrote
the complete marker and the new link/compiler identity fields.

The next auto-coalesced attempt (`game-dll-20261119T150618864547Z-0fb052`,
source generation 21) completed as a validated no-op:

- worker entry to no-op completion: 0.403 s;
- object scan: 0.096 s, 642 skipped, 0 stale, marker path (`parse_ms=0`,
  43 external stamps);
- toolchain 73.6 ms, identity 19.6 ms, scan 99.2 ms, no-op 9.2 ms;
- generation 353 reused; no identity write, compile, link, publish, or
  generation increment;
- complete request-to-result 36.346 s, dominated by pre-worker codegen 8.5 s,
  manifest snapshot 16.6 s, validation 0.04 s, worker-start queue ~3.9 s,
  worker 0.4 s, and result handling ~3.2 s.

The terminal run-journal events carried `no_op: true`,
`worker_total_seconds: 0.403`, the same generation/code hash, and the attempt
timing stream path. This verifies the batched scan reporting and the separated
worker clock against the live dev-loop.

Two earlier live attempts failed at `toolchain_setup` with
`TypeError: Strings must be encoded before hashing` from the new
`link_identity` hash; the call now encodes UTF-8. Generation 348's exit-2
compile/link cause is still not established.

Launch evidence for dashboard request `dashboard-497bb0f3`: press-to-accept
1.1 ms, queue wait 171.6 ms, client process start 21.80 s, readiness 70.43 s.
The server was not already healthy, so the 100 ms press-to-Popen precondition
did not apply.

A second no-op (`game-dll-20261119T151310460091Z-d7eef1`, generation 355,
source generation 28) repeated the result: 0.290 s worker time, 0.052 s scan,
642 skips, and 9.927 s complete request-to-result (codegen 3.69 s, manifest
snapshot 5.28 s, validation 0.014 s, worker 0.29 s). The marker fast path and
the worker-entry clock are repeatable; the pre-worker stages now dominate.
