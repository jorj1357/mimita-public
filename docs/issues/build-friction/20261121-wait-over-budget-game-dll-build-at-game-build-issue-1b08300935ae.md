# issue-1b08300935ae: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-preworker-wait-attribution-gap)

### 2026-11-21T22:04:03.336Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer (independent bounded review)

Verdict: confirmed real over-budget wait, not a hung or failed build. The 60s budget was crossed during pre-worker manifest snapshot on a healthy attempt that later published generation 433. Same systemic attribution/noise gap seen in sibling reviews.

WAIT OCCURRENCE (verified)
- wait_id game-dll-build:2026-11-21T21:50:15.395Z; started 21:50:15.395Z; elapsed 60.892s; budget 60.0s; reported stage "game_build"; generation 432 (stale); build_id 2026-11-21T02:26:50.306117Z (latest EXE artifact, not the waiting DLL attempt); source_generation 0; request 2026-11-21T21:50:15.046Z-84e61847.
- The 60s threshold was reached at 21:51:15.4, inside manifest_snapshot. No compile, link, or worker stall caused it.
- Wake: wake-4bc8e2c89046 queued exactly once at 21:51:17.303Z for this issue (acceptance met). Codex adapter launched 21:51:17.698Z then exited; fallback retried 21:55:06.412Z; opencode claimed/launched 21:57:56.356Z/21:57:56.400Z. Queue-to-review was ~6.5 min from adapter fallback, not a wait-policy fault.

PINNED BUILD ATTEMPT (game-dll-20261121T215015817067Z-d501fb)
- Triggered by note live-cpp-compile-link-trial "Add behavior-neutral comment to trigger a one-source incremental DLL measurement" (changed src/effects/zombie-tower-effects.cpp).
- Pre-worker: codegen resident_state 0.75s; codegen packet_registry 3.61s; manifest_snapshot 137.38s (21:50:21.264 -> 21:52:39.006, source_count 1008); manifest_validation 0.01s; source_snapshot 26.38s (21:52:39.232 -> 21:53:05.666).
- Unexplained 43.33s gap 21:53:05.666 -> 21:53:48.993 with no recorded stage, before worker_input_validation (0.013s).
- Worker phases: initial_source_coherence 0.13s; toolchain_setup 0.10s; changed_source_preflight 36.39s; object_cache_scan 4.90s; compile 40.59s (12 TU, slowest zombie-tower-effects 36.30s); link 61.98s (642 objects, 242,552,947 B DLL); coherence_validation 1.10s; publish 29.10s (external_dependency_manifest 27.79s over 642 depfiles).
- Result: status ok, generation 433 published 21:56:52.632Z, no_op false, compiled 12, skipped 630; total 359.611s, worker_total 146.435s. No failure, retry, or timeout.

ROOT CAUSE / OWNER
- The flat WAIT_ISSUE_SECONDS=60 (devscripts/dev-loop.py:108, applied at 3391-3407) is below the normal healthy game-DLL build duration (here 359-396s; manifest_snapshot alone 137s), so a healthy build files an over-budget wait and wakes a reviewer on nearly every build.
- The pre-worker cost and the wait-attribution gap are already tracked by issue-136d4553ff92 (owner build-coordinator, tasks dll-build-stage-accounting and dll-snapshot-cause-and-incremental-index). The 60s wait is a correct symptom of real pre-worker cost; widening the threshold alone would hide it.
- 40+ identical "game-dll-build at game_build" issues exist today (e.g. issue-c632de44f8d6, issue-2235461bb31b, issue-44d82d8626ad). build_friction.record_wait_incident creates one issue per wait_id even for identical signatures, and its own risk field flags the noise. Recommend one rolling occurrence issue per (kind, stage) with occurrences appended, or link new ones to issue-136d4553ff92 while it is open. No new duplicate created by this review.

ACCEPTANCE
- Issue already carries wait_id, stage, reason, elapsed, budget, build_id, generation, source_generation, request_ids, and evidence; reproduction, expected behavior, acceptance, paths, and proof expectation are present; readiness ready=true, missing=[]. One reviewer wake per occurrence verified. No behavior task, so no runtime proof is claimed; build/link is compilation evidence only.

NEXT
- No source edited, no build or test requested (review only). Recommend linking this issue to issue-136d4553ff92 and closing it as a duplicate symptom once the pre-worker cost and wait attribution are owned there. The default-budget question remains open (docs/specs/dev-loop/agent-build-queue.md open review questions).
- Confidence: high (pinned attempt JSONL/JSON, dev-loop wait event, wake journal). No runtime behavior involved.
