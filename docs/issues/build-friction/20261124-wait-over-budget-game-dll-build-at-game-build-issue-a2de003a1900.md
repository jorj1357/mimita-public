# issue-a2de003a1900: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-24T22:46:21.116Z — build-wait-reviewer

related_build: game-dll-20261124T215807388728Z-84be9e
related_run: 20261124_215719_01

## Review (pass-with-open-defects)

### 2026-11-24T22:48:40.381Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-3145a03d66ba), read-only, 2026-11-24T22:46Z. No source, build, runtime, or close action taken.

ACCEPTANCE: PASS for the wait record and the exactly-once reviewer wake queue.
- Dev-loop run 20261124_215719_01, journal logs/2026-11-24/20261124_215719_01/dev-loop/events.jsonl L49 records dev-loop.wait.over_budget for wait_id game-dll-build:2026-11-24T21:58:06.724Z, kind game-dll-build, stage game_build, elapsed_seconds 60.382, budget 60.0, issue issue-a2de003a1900, created:true.
- L48 records dev-loop.wait.reviewer-wake for the same wait_id/issue with wake.queued:true.
- Exactly one wake request was created for this wait: wake-3145a03d66ba, created/queued 21:59:08.153Z/166Z, issue issue-a2de003a1900 (.dev/agent-wake/events.jsonl L2912; requests/wake-3145a03d66ba.json).
- Reproduction, expected_behavior, acceptance, paths, and proof_expectation were already present (readiness missing=[]). related_build and related_run were empty and are now set to the waited-on attempt/run.

THE WAITED-ON BUILD WAS REAL AND SUCCEEDED (this is not a stall):
- Attempt game-dll-20261124T215807388728Z-84be9e, request 2026-11-24T21:58:06.362Z-a390fc9e:mode3:game, revision d3113668..., source_generation 27, changed src/main-systems.cpp + src/physics/physical-entity.cpp, generation 625, status ok, no_op false.
- .dev/build-timings/game-dll-20261124T215807388728Z-84be9e.json: total_seconds 182.618, compile 67.398, link 89.348, object_scan 1.73, publish 0.652; slowest TUs src/main-systems.cpp 60.486s and src/physics/physical-entity.cpp 60.480s; dev-loop.event dev-loop.game-build.result status ok at 22:01:07.990Z and game-generation.ready generation 625 at 22:01:10.816Z.
- The flat 60s budget fired ~0.4s after the first changed TU completed and ~122s before publication. The over-budget signal is expected for this workload, not evidence of a hung build.

OPEN DEFECTS (already tracked; not re-filed as new issues):
1. Stale wait identity (known): evidence carries build_id 2026-11-24T19:27:02.742428Z (last completed EXE build) and generation null while the in-flight attempt is 84be9e / generation 625; related_build/related_run were empty. Recorded as an occurrence on issue-9a4503f6ed68 (owner wake-proof-wait); source dev-loop.py:3158-3170,3259.
2. Duplicate issue volume (known): this wait_id minted issue-a2de003a1900 and many near-identical "Wait over budget: game-dll-build at game_build" issues exist (issue-ee3e906c8273, issue-d6a3c6c35682, issue-7ef5043ce95c, issue-74f939fb99fc, issue-723d66575ccf, issue-121f867b430a and others). Per-attempt wait_id dedupe only. Tracked on issue-9a4503f6ed68; this review added no duplicate issue.
3. Reviewer-wake launch attribution and duplicate/late dispatch (known): the dev-loop.wait.reviewer-wake row at 21:59:11.373Z is stamped with this wait_id/issue, but its wake.launch_result (2000-char tail of agent-wake.py run --once, build_friction.py:250) embeds wake-59dc989b28d0 / issue-ee3e906c8273 / log 2026-11-24T21-45-00.685Z / pid 18976, not this wake. For this wait: codex claimed 21:59:09.006Z and launched pid 26300 at 21:59:09.145Z; wake.retry "stale launched request recovered by scheduler" at 22:01:27.596Z; opencode claimed/launched pid 35280 at 22:07:28.050Z/087Z. Queue-to-final-launch 499.9s (~8m20s); two wake.launched events for one wait_id. The exactly-once queue held; the one-launch target did not. Occurrence appended to issue-84c584957e65.
4. Build-resource evidence gap: .dev/build-timings/game-dll-20261124T215807388728Z-84be9e.resource.json: status captured, 142 samples @1s over 182.614s, mean host CPU 99.984%, peak 100.0%, minimum available memory 12,595,200 B (~12.0 MiB), no sampler errors; runtime_alignment unavailable (fresh_server_and_client_journal_evidence_not_matched). Missing matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics: unmeasured, not zero. gap_key build-resource-sampling:game-dll-20261124T215807388728Z-84be9e recorded once on issue-136d4553ff92 with a question to the owner; build-stress occurrence recorded on issue-2a5c5da53565. Near-full CPU and ~12 MiB free plausibly lengthened compile/link but causality is not proven.

PROOF / NEXT STEP:
- This is a tooling wait, so runtime/gameplay proof is not applicable; the required evidence is the dev-loop wait event + issue record + wake request, all present.
- Owner wake-proof-wait should choose one duplicate-issue policy (one canonical issue per kind+stage with occurrences, a compile-progress heartbeat budget, or a budget above the slowest TU) and fix the stale build_id/generation and launch_result attribution. Reviewer does not auto-close this issue; it stays in_review pending that owner decision.

CONFIDENCE: high for wait/wake/build facts; moderate/unknown for the 12 MiB memory floor cause. Cross-links: issue-9a4503f6ed68, issue-84c584957e65, issue-136d4553ff92, issue-2a5c5da53565, issue-6e08d89b55e3.
