# issue-bcc410ceae2f: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-22T19:53:58.980Z — build-wait-reviewer

related_build: game-dll-20261122T190638183164Z-940916
related_run: 20261122_174002

## Review (reviewed-with-findings)

### 2026-11-22T19:56:10.979Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review of wait occurrence game-dll-build:2026-11-22T19:06:37.019Z (elapsed 62.175s, budget 60.0s).

VERDICT: reviewed-with-findings. Acceptance is met for this occurrence: the issue carries wait_id, stage, reason, elapsed, build/run IDs and evidence, and exactly one reviewer wake was queued for this wait_id. The over-budget event is a false-positive stall signal: the request was a successful NO-OP build whose only work was the deterministic pre-worker pipeline, and it reached its terminal result about two seconds after the 60s threshold.

EVIDENCE (direct observation, logs/2026-11-22/20261122_174002/dev-loop/events.jsonl):
1. dev-loop.control (line 155): request_id 2026-11-22T19:06:31.843Z-61eef48e, action game-dll, requested_by ai, accepted through dev-loop, wall_time 19:06:38.183Z.
2. dev-loop.wait.over_budget (line 169) at 19:07:49.423Z: wait_id game-dll-build:2026-11-22T19:06:37.019Z, kind game-dll-build, stage game_build, reason 'building whole-game revision a8f03688babd', elapsed_seconds 62.175, budget_seconds 60.0, started_utc 19:06:37.019Z, source_generation 11, generation 512, request_ids [2026-11-22T19:06:31.843Z-61eef48e], created true.
3. Attempt game-dll-20261122T190638183164Z-940916 (.dev/build-timings/game-dll-20261122T190638183164Z-940916.json): started 19:06:38.183Z, completed 19:07:43.712Z, success true, no_op true, generation 512, revision a8f03688babd, total_seconds 65.524, worker_total_seconds 3.021, compiled 0, skipped 642, changed_paths [], compile_seconds 0, link_seconds 0.
4. dev-loop.game-build.result (line 167) confirms status ok / no_op true at 19:07:41.285Z (total_seconds 62.51, worker_total_seconds 3.021); dev-loop.game-generation.ready (line 170) reused generation 512 at 19:07:51.068Z (duration 65.679s).

FIRST DIVERGENCE / ATTRIBUTION: The wait time is deterministic pre-worker stage overhead, not a stalled or expensive compile. Stage events in order: manifest_snapshot started +5.1s (line 156); manifest_validation started +22.5s, completed +25.5s (88ms, source_count 1008; lines 158-159); source_snapshot started +29.7s, completed +36.0s (1.2s, reused_published generation 512; lines 160-161); codegen:resident_state_codegen.py +39.5s to +42.1s and codegen:packet_registry_codegen.py +44.1s to +46.8s (both cache_hit; lines 162-165); game-build.started +49.0s (line 166). The worker then skipped all 642 objects in 3.021s. phase_durations_ms only accounts for ~0.59s total (identity_and_cache 106.574ms, object_cache_scan 45.349ms, toolchain_setup 434.523ms, no_op 4.441ms), so ~49-62s of the round trip is the pre-worker snapshot/validation/codegen pipeline. Because check_wait_budget measures elapsed since game_build_started_utc (request acceptance, not worker start) with a flat 60s budget (devscripts/dev-loop.py:108-109, 3486-3547), an ordinary no-op request can trip it. This is the same unattributed/snapshot-overhead gap owned by issue-136d4553ff92, whose acceptance already requires reconstructing a no-op attempt with no unknown interval.

ATTRIBUTION FIELD DEFECT (does not block this occurrence): evidence.build_id 2026-11-22T17:39:49.763492Z is the shared EXE artifact id while evidence.generation 512 is the game-DLL generation; the actual game-DLL attempt id game-dll-20261122T190638183164Z-940916 is absent from the wait record. Matches issue-136d4553ff92 and prior reviews (issue-9202020e4618, issue-d668de6bfc5e). related_build/related_run were corrected here for traceability.

WAKE EVIDENCE: exactly one wake per wait_id. wake-98a7e232869f was queued 19:07:47.032Z, claimed 19:07:48.708Z by codex, launched pid 4292 at 19:07:48.852Z; the codex adapter then went stale and was recovered at 19:09:42.482Z, and opencode claimed+launched at 19:35:27/28 (.dev/agent-wake/events.jsonl lines 1855-1870). The launch_result captured in the over_budget reviewer-wake event (events.jsonl line 168) belongs to the older queued wake for issue-9202020e4618 because wake_reviewer's immediate run --once launches the head of the queue; this does not mean this wait's wake was misaddressed, since its own wake events are correct. Late dispatch is the already-owned issue-84c584957e65.

RECURRENCE / POLICY: Another instance of the flat-60s budget producing one issue per over-budget occurrence (build_friction.py:143-202 keys uniqueness on wait_id; dev-loop.py:108-109 sets WAIT_ISSUE_SECONDS=60). Here the real margin was 2.175s on a no-op build, so the signal is noise rather than friction against the stated goal. Suggested direction (no code change made; policy/human decision): (a) measure the wait budget from worker start or the first compile/link stage instead of request acceptance so deterministic snapshot/codegen cache checks do not trip it, and/or (b) coalesce by kind+stage into one canonical issue with occurrences instead of one issue per wait_id, and/or (c) suppress the reviewer wake for no_op attempts. Budget ownership remains with issue-136d4553ff92 / build-friction; left unowned.

NO CODE FIX CLAIMED. No repository source or config edited; coordination records only.
