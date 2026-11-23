# issue-1bf97a27238f: Game DLL build 408 times out after rebuilding 642 translation units and retry fails stale codegen

## Human report (verbatim, immutable)

> 2026-11-20 18:05 build progress: pending=370 pending_compile_count=641 phase=compile; 18:18 build 338310 completed success=false failed_phase=worker_timeout worker_error=worker timed out after 3600s; next attempt 8abbf9 failed codegen check: packet_registry_codegen.py


## Scheduled independent review

### 2026-11-23T16:18:28.794Z — mode3-latency-goal

A bounded independent review was queued as wake wake-5a3e2c474187 after 416 completed work sessions (trigger=sync). The review is pending; this scheduling event is not evidence that the issue is fixed. The reviewer is instructed to append evidence, counterevidence, and a next check.

## Review (keep-open)

### 2026-11-23T16:43:22.781Z — issue-review-sweeper

Reviewer: issue-review-sweeper

Independent bounded review (2026-11-23, agent issue-review-sweeper), read-only: no source edits, no build, no close. Artifacts inspected: issue brief/readiness, proof status, and .dev/build-timings for the reported attempt and later attempts.

OBSERVED FACTS
- Readiness: ready=true, score 8, missing=[]; but tasks=0, proofs=0, reviews=0. Acceptance: published generation at current revision, dev-loop journal with no worker_timeout, codegen passing without manual regeneration.
- Reported attempt game-dll-20261120T171733871137Z-338310 (.dev/build-timings/game-dll-20261120T171733871137Z-338310.json): success=false, failed_phase=worker_timeout, worker_error=worker timed out after 3600s, total_seconds=3635.67, jobs=12, source_count=642.
- Its own timeline (.jsonl) records 392 translation_unit.started, 379 completed (378 status=compiled, 1 failed: src/mapeditor/map-editor-commands.cpp). Summed compiled seconds ~=38183s, i.e. ~3182s wall at 12 jobs. Queue waits were extreme: e.g. src/network/badconn/badconn-blackout.cpp submitted 17:24:14.946Z but started 18:18:09.265Z (queue_wait_ms=3234319, ~53.9 min). Slowest TUs: server-gamemode 930.07s, map-editor 706.62s, main-systems 611.98s.
- The JSON summary contradicts that timeline: worker_metrics.compiled=23, skipped=619, worker_total_seconds=2001.281 vs 378 compiled TUs and 3635.67s wall in the .jsonl. So the issue summary claim of rebuilding all 642 translation units is NOT supported: 378 of 642 compiled, 392 started.
- Retry game-dll-20261120T181809855943Z-8abbf9 failed stage codegen:packet_registry_codegen.py (exit 1, 12.78s). Its notes are unrelated concurrent edits (src/network/multiplayer-tick.cpp, src/network/server-packets.cpp by reconnect-root-cause).
- packet_registry_codegen.py failures recurred AFTER the report: 3 on 2026-11-21 and 7 on 2026-11-22 (e.g. ...-935a96, ...-5ec294), all codegen check failed: packet_registry_codegen.py.
- worker_timeout has NOT recurred since 2026-11-20 17:17Z: only 3 worker_timeout attempts exist in .dev/build-timings (d55095 11-19, 58c811 11-20 00:57, 338310). Recent builds succeed: gen 563 (485a05) exit 0 in 70.54s, compiled=2, and its codegen:packet_registry_codegen.py stage ran (cache miss, 1178.6ms, exit 0).
- Read-only run now: python devscripts/packet_registry_codegen.py --check -> EXIT=0, [PACKET CODEGEN] ok packets=78. (Correctness gate only; not a build.)
- Possible duplicate: claimable issue issue-a6890271d414 carries the identical title/summary as this issue.

INFERENCE / UNCERTAINTY
- The 3600s timeout was most plausibly CPU/queue contention (54-min queue waits, 300-930s per-TU compiles), not a full 642-TU rebuild. The contending process is not named in the artifact.
- Nov 21-22 codegen failures likely reflect packet inputs/ledger changing mid-build (reproduction: packet inputs are concurrently edited), but codegen stderr is not captured in the timing artifacts, so the cause is unconfirmed.
- The timeout symptom is currently not reproducible and the codegen gate currently passes; that is recovery, not proven root-cause repair.

CONTRARY EVIDENCE
- worker_metrics.compiled=23 would imply the timeout was not compile-volume driven; since this counter disagrees with the timeline it is unreliable, not counter-proof.
- A near-no-op build now succeeds (485a05), but it does not exercise the original flags-identity-change + concurrent-edit condition.

NEXT DISCRIMINATING CHECK
1) Attach the gen-563 attempt (485a05) timing + ready.json as the published-generation-at-current-revision evidence and confirm its codegen stage exit 0.
2) Deliberately re-run the reproduction: request a game-DLL build while a packet input (src/network/packets.h / config/hot-reload/packet-ledger.json) is edited; capture packet_registry_codegen.py stderr and decide stale-ledger vs race.
3) Reconcile worker_metrics counts with the timeline (proposal filed on issue-136d4553ff92); until then do not trust compiled/skipped summary fields here.
4) Identify the 2026-11-20 17:24-18:18Z contention source before declaring the timeout class fixed.

Confidence: high on artifact facts; medium on root cause.
