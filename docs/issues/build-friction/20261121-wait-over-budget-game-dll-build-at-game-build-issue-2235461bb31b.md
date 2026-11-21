# issue-2235461bb31b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-preworker-wait-attribution-gap)

### 2026-11-21T21:56:08.232Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed this wait occurrence against the pinned game-DLL attempt, the dev-loop wait policy, and the reviewer wake record. Conclusion: confirmed real over-budget wait, not a hung build. The waiting attempt was a healthy no-op DLL build; the 60s budget was exceeded in pre-worker staging. The auto-created issue also mis-attributes the wait (generic stage, wrong build_id), which is a known, still-open evidence gap.

WAIT OCCURRENCE (verified)
- wait_id game-dll-build:2026-11-21T21:33:20.879Z; started 21:33:20.879Z; elapsed 61.049s; budget 60.0s; reported stage "game_build"; generation null; build_id 2026-11-21T02:26:50.306117Z (EXE artifact, not the DLL attempt).
- The 61s check landed inside manifest_snapshot: .dev/build-timings/game-dll-20261121T213320985628Z-1a2692.jsonl shows manifest_snapshot started 21:33:53.743Z (elapsed 32.749s) and completed 21:34:23.284Z (62.290s). The truncated reason in the issue is that same started row.
- Wake record: wake-9b5f268395f8 queued exactly once at 21:34:23.386Z; codex adapter launched 21:34:23.774Z then exited; retried on opencode at 21:38:23.796Z; claimed/launched 21:41:25.564Z/21:41:25.603Z. Acceptance "one reviewer wake per occurrence" is met, but queue-to-review was about 7 minutes because of the adapter fallback.

PINNED BUILD ATTEMPT
- attempt game-dll-20261121T213320985628Z-1a2692, request 2026-11-21T21:33:20.596Z-29fe5ab4; started 21:33:20.994Z, completed 21:35:17.415Z, exit 0, total 116.420s, no_op=true, generation 432 (unchanged), revision 34dcd763673b.
- Pre-worker staging: resident_state codegen 1.110s; packet_registry codegen 30.888s; manifest_snapshot 29.110s; manifest_validation 0.053s; source_snapshot 35.666s. The worker then scanned 642 objects, skipped 642, compiled 0, linked 0, no_op 0.003s (object_cache_scan 0.036s; worker input validation 0.012s).
- So about 97s of pre-worker snapshot/codegen work produced a no-op; the wait was not caused by compile/link and there was no stall or retry.

CONTEXT
- Adjacent no-op attempts 52f7a2 (21:37:21) and 449ec0 (21:40:24) deliberately exercised the scan-memoization trials tracked in issue-136d4553ff92 (report docs/reports/20261121_214300-warm-dll-cache-scan-optimization-results.md). They are not unexplained redundant builds.

FIRST DIVERGENCE / EVIDENCE GAP (still open)
- docs/specs/dev-loop/change-to-evidence-latency.md requires the auto-created wait issue to carry the exact build/run IDs and to append later observations. This issue cannot name the waiting attempt from its own fields: stage is the flat "game_build" instead of the real manifest_snapshot; build_id is the latest EXE artifact; generation is null because dev-loop.py:3396-3406 reads self.game_build_generation, which is only assigned after success.
- No terminal result was appended to this issue (occurrences=[]), because dev-loop.py:3408-3418 suppresses repeats of the same wait_id in reported_wait_ids, even though build_friction.py:164-169 supports issue_occurrence.
- Per-attempt summary .json still copies stale worker_metrics: .dev/build-timings/game-dll-20261121T213320985628Z-1a2692.json reports worker_metrics.total_seconds=170.04, compiled=12, compile_seconds=32.645, object_scan_seconds=98.848, link_seconds=10.942 while no_op=true, compile_sum_ms=0, changed_paths=[], and top-level worker_total_seconds=0.426. The same block appears in 52f7a2 and 449ec0. This recurrence of the stale-summary defect is already recorded in issue-136d4553ff92 (2026-11-21 append entries), so it needs no new issue.

ASSESSMENT / NEXT
- This is wait-policy noise over a real performance problem, not a build failure. A flat 60s budget is below the ~97-116s that pre-worker snapshot/codegen currently takes for an unchanged tree, so such attempts will file an issue and wake a reviewer each time. 41 build-friction issues with this exact title already exist.
- No fix belongs in this issue and none was made: no source edited, no build/test requested. The owner of the performance and attribution fix is issue-136d4553ff92 (build-coordinator): emit the active attempt id and target generation in wait evidence, record the true pipeline stage, append the terminal result to the existing issue, and source every attempt summary metric from that attempt. The default-budget question remains open per docs/specs/dev-loop/agent-build-queue.md:314.
- Confidence: high (pinned attempt JSONL/JSON, adjacent attempts, wake events, ready.json). No runtime behavior involved.
