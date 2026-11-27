# issue-e99701641a1c: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-real-wait-not-stalled; fields-complete; journal-event-missing; late-double-launch)

### 2026-11-27T02:09:46.694Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded reviewer review (build-wait-reviewer, wake-0e4507371e7c). Read-only: no source edit, no build, no runtime command, no close, no fix claim.

WAIT / ISSUE IDENTITY
- wait_id game-dll-build:2026-11-27T01:38:23.090Z; kind game-dll-build; stage game_build; elapsed 60.319s vs budget 60.0s; request 2026-11-27T01:38:22.462Z-eb5ebd68:mode3:game; source_generation 6.
- Field completeness MET: the issue evidence carries wait_id, kind, stage, reason, elapsed_seconds, budget_seconds, started_utc, source_generation, generation, build_id, request_ids, and paths.
- The reason is a readable "[GAME BUILD] revision=7c0ee5b5... sources=645 jobs=10 priority=high cache=build\game\objects\b10e3a31b3ae8637 seeded=0" line, NOT the mid-JSON tail defect tracked under issue-6e08d89b55e3.
- STALE IDENTITY: evidence generation=749 and build_id=2026-11-27T00:44:04.497616Z are last-published values; the in-flight attempt was game-dll-20261127T013823160277Z-8b3773, generation 750, source_generation 6, revision 7c0ee5b5.... No attempt id is recorded in the wait evidence. This reproduces the stale-generation/build_id pattern already discussed on issue-9a4503f6ed68.

WAITED-ON BUILD WAS REAL, NOT STALLED
- .dev/build-timings/game-dll-20261127T013823160277Z-8b3773.json: started 2026-11-27T01:38:23.160Z, completed 2026-11-27T01:41:29.826Z, total_seconds 186.672, success true, generation 750, changed_paths [src/vehicles/vehicle-system.cpp], 2 compiled / 644 skipped.
- At the 60.319s trigger the attempt was still compiling the single changed TU: changed_source_preflight 88.421s and slowest compile src/vehicles/vehicle-system.cpp 88.292s; then compile 5.375s (identity provider) and link 66.219s.
- Interpretation: a flat-60s wait budget fired mid a legitimate ~88s changed-TU compile followed by a 66s link. The build succeeded; this is not a hang/stall. It is one more data point that a full changed-TU+link cycle naturally exceeds 60s.

REVIEWER WAKE (exactly-once queue vs dispatch)
- Exactly ONE wake.queued for this issue: wake-0e4507371e7c at 2026-11-27T01:39:29.622Z (single hit in .dev/agent-wake/events.jsonl for issue-e99701641a1c). The exactly-once QUEUE contract held.
- Dispatch was late and double-launched: codex claimed 01:39:41.915 / launched 01:39:42.117 (pid 11340), then exited ("previous adapter process exited; trying fallback" 01:46:15.791); opencode claimed 01:55:03.186 / launched 01:55:03.253 (pid 34076). Queue-to-final-launch 333.631s (~5m33.6s), above the <=5 min target; two wake.launched events for one wait. This reproduces issue-84c584957e65. No launch_result wake-record misattribution could be checked here because the dev-loop wait event is missing (below).
- Note: before this opencode review, a separate build-wait-reviewer activation (ended 02:05:10Z) reviewed sibling issues issue-5d8ebe281ce6 / issue-dea21a9c722d; it did not touch issue-e99701641a1c, which remained state=new, reviews=[] until now.

JOURNAL HALF OF PROOF IS MISSING
- No retained journal contains dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake for wait_id game-dll-build:2026-11-27T01:38:23.090Z or issue-e99701641a1c. A repo-wide search returns only .dev/agent-wake/requests/wake-0e4507371e7c.json.
- The 20261127_014331 dev-loop journal records wait events only for the later wait 01:44:56.218Z. Current .dev/state.json events_path points at the much later run 20261127_020014. Variant is most likely events_path=None / no active root run at 01:38-01:39Z (inference; no captured ENOENT line for this window). This reproduces issue-6e2d7b91e7d9 and drops the "Dev-loop wait event" half of this issue's proof_expectation.

RESOURCE EVIDENCE (direct; gap_key build-resource-sampling:game-dll-20261127T013823160277Z-8b3773)
- .dev/build-timings/game-dll-20261127T013823160277Z-8b3773.resource.json: status=partial; errors=["sampler background thread did not stop before timeout"]; final_sample_status=skipped_sampler_thread_timeout; sampler_thread_status=did_not_stop_before_timeout; sampler_shutdown_timeout_seconds=1.0; 127 samples @1.0s over 186.657s.
- Host: 12 logical CPUs, ~15.6 GiB total; mean_host_cpu_percent 99.191, peak 100.0; minimum_available_memory_bytes 370,589,696 (~353 MiB); first sample memory_used_percent 90.8. process_discovery_wall_ms_total 116,438.742; runtime_context_discovery_wall_ms_total 40,807.272; sampler_thread_cpu_ms 34,578.125.
- runtime_alignment mode_unverified: same run 20261127_011757, server TDM pid 5900 gen 749 with 9 NPCs/1 player; client pid 34092 fps 93.97 (95/95 frames over 4.17 ms budget, p95 14.35 ms); mode_match false, client mode stale_or_unreported.
- unavailable_fields: matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation.
- Inference only: CPU saturation is expected from a 10-job compile and the near-full memory is a candidate contributor, but with no matched pre-build baseline this does NOT prove pressure caused the >60s wait.

ACCEPTANCE VERDICT
- Issue field completeness: MET.
- Exactly-once reviewer wake queued: MET (one wake.queued).
- Exactly-once / timely launch: NOT MET (two launches, ~5m34s queue-to-launch).
- Proof expectation "Dev-loop wait event": NOT MET (event missing from all retained journals).
- Underlying build: real work, succeeded at 186.672s; not a stall.
- No code fix claimed or edited by this reviewer. The fixes remain owned elsewhere: issue-6e2d7b91e7d9 (journal drop), issue-84c584957e65 (late/duplicate dispatch), issue-9a4503f6ed68 (stale wait identity), issue-2a5c5da53565 (resource sampler/baseline). Human/human-owner decision still needed on a wait budget that accounts for natural full changed-TU+link wall time to stop the per-attempt issue flood.
