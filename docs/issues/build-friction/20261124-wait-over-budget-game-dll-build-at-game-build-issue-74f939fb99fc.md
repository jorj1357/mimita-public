# issue-74f939fb99fc: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-24T21:02:51.165Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed wait occurrence game-dll-build:2026-11-24T20:24:20.106Z (stage game_build, elapsed 60.629s vs budget 60.0s).

EVIDENCE (direct, pinned):
- Build attempt game-dll-20261124T202420131204Z-8cc10c started 2026-11-24T20:24:20.131Z, completed 2026-11-24T20:26:28.777Z, success=true, total_seconds=128.646, generation=617, revision=c7d650e...; request 2026-11-24T20:24:20.026Z-a67f1373:mode3:game. Source: .dev/build-timings/game-dll-20261124T202420131204Z-8cc10c.json.
- Phase split (same file): changed_source_preflight=70.499s, compile=13.517s (slowest TU src/entities/player-render.cpp 70.412s), link=33.852s, object_cache_scan=0.017s, publish=0.638s. The 60s threshold was crossed during changed-source preflight, not a hang.
- Resource sample (.dev/build-timings/game-dll-20261124T202420131204Z-8cc10c.resource.json): mean host CPU 99.99%, peak 100.0%, minimum available memory 16,961,536 bytes (~16.2 MiB), sample_count=81 over 128.6s; runtime pair same_run_server_client=true, server mode bloodtest, 1 NPC/1 player. Host was saturated.
- Recorded reason is readable ('[GAME BUILD] revision=c7d650e... sources=644 jobs=10 priority=high ...'), so issue-6e08d89b55e3's mid-JSON-tail symptom does not apply to this occurrence.

ACCEPTANCE CHECK:
- wait_id, stage, reason, elapsed, budget, started_utc, request_ids, generation/source_generation, and evidence are present; readiness=ready, missing=[]. Run ID is N/A for a compile wait (no runtime was waited on).
- One issue per wait occurrence: across 100 build-friction issues there are 90 unique non-empty wait_ids and 0 duplicate wait_ids. This wait_id maps to exactly one issue (this one). Mechanism tracked by issue-9a4503f6ed68 (proven) held.
- Exactly one reviewer wake was QUEUED: wake-5a26878e0275, wake.queued 2026-11-24T20:25:22.352Z, one request file. Good.
- DEFECT (separate owner): the same wake was launched twice and late. .dev/agent-wake/events.jsonl shows wake.claimed+wake.launched (adapter=codex) at 2026-11-24T20:25:24, wake.retry (adapter=opencode) at 20:36:52, then wake.claimed+wake.launched (adapter=opencode) at 20:38:43 - 13m21s after queue and two wake.launched rows. This violates issue-84c584957e65 acceptance ('one wake.queued and one wake.launched ... no duplicate launches'); appending the occurrence there.

INFERENCE / OWNERSHIP:
- The over-budget wait is real and expected because a one-source change took 128.6s under 100% host CPU and ~16 MiB free memory. Root latency is owned by issue-136d4553ff92 (owner build-coordinator); wait policy/parent by issue-9a4503f6ed68. No defect found in the wait detector or issue/wake creation for this occurrence. No source edited; no code fix claimed. Confidence: high for direct evidence, medium for the cross-issue root-cause attribution (inference).

## Agent suggestion

### 2026-11-24T21:24:47.266Z — build-wait-reviewer

Follow-up to the recorded review: the wait issue and one reviewer wake exist, but the dev-loop-journal half of the proof expectation is missing. No retained logs/2026-11-24/**/dev-loop/events.jsonl and no .dev/**/*.jsonl contains dev-loop.wait.over_budget, dev-loop.wait.reviewer-wake, or wait game-dll-build:2026-11-24T20:24:20.106Z. Current .dev/state.json events_path points at run 20261124_205956 (after the wait), so the journal active at 20:24 is not recoverable from current state. Recorded as an occurrence on issue-6e2d7b91e7d9 with cause unknown (events_path=None between runs vs rotated/absent journal). Reviewer: build-wait-reviewer.
