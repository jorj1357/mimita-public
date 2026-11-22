# issue-b24f9f79794d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-build-failure-wait)

### 2026-11-22T08:10:45.166Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-2f85be04912d), read-only: no source edits, no build requested, no run started.

FACTS
- Wait occurrence: wait_id game-dll-build:2026-11-22T07:44:40.382Z, kind game-dll-build, stage game_build, elapsed 60.593s vs 60.000s budget (+0.593s, about 0.99% over); issue created 2026-11-22T07:45:41.102Z.
- Pinned attempt: .dev/build-timings/game-dll-20261122T074440480025Z-9387dd.json and .jsonl; request_ids [2026-11-22T07:44:40.322Z-7b158686]; started 07:44:40.480Z; revision cfae208529380ee361aac5901091ae9bb89eda3eb304b78e5261f15754242622; generation 473; source_generation 0; changed_paths [src/npc/npc.cpp, src/npc/npc.h]; change notes by fun-cs-npc-objective (dash suppression 07:42:56.847Z, dash logging 07:43:37.543Z). Result FAILED at 07:52:17.399Z after total_seconds 456.908 (worker_total_seconds 399.658); compile phase 352.853s, compiled 92 / skipped 506, jobs 10, compile_sum_ms 3,329,690.896. Error: src/network/server-npcs.cpp:904, class Npc has no member named hasLoggedDash.
- The wait was real and spanned the whole attempt: the first translation units did not complete until about 07:47, so nothing had finished when the 60.593s check fired at about 07:45:41. Slowest TUs: server-gamemode 73.517s, map-editor 60.593s, multiplayer-tick 55.175s.
- Root trigger: the same change set removed Npc::hasLoggedDash from src/npc/npc.h and npc.cpp while src/network/server-npcs.cpp still assigned it, so the header-fanout rebuild (92 TUs) could only end in a compile failure.
- Follow-up chain: retry game-dll-20261122T075217617830Z-a15284 (same revision) failed at 07:54:09.827Z with the identical error (total 112.196s, compile 74.916s). Fix attempt game-dll-20261122T075410208033Z-f3ff3f changed only src/network/server-npcs.cpp ("Remove the retired one-shot NPC dash-log reset field"), succeeded in 190.312s, generation 476, completed 07:57:20.532Z. Current source has 0 matches for hasLoggedDash, so the immediate defect is resolved.
- Wake: .dev/agent-wake/requests/wake-2f85be04912d.json created 07:45:42.532Z, one request for this wait_id, attempts=2; first launched via codex 07:45:43.992Z, the adapter exited, retried with opencode at 07:53:21.278Z, claimed/launched 07:55:16.044Z. The single-queue requirement passes; the adapter retry is not a duplicate wait issue.
- Readiness/acceptance fields: ready=true, missing=[], score 8/8; wait_id, stage, reason, elapsed, budget, request_ids, and evidence are present.
- Evidence defects that persist: evidence build_id is 2026-11-22T02:37:06.972018Z (the previously published artifact) and generation=null, because check_wait_budget reads latest_build/game_build_generation before the in-flight attempt identity is assigned; related_build is empty. The attempt is identified here by wait_id plus request_ids, not by the evidence build_id.
- Runtime journal gap: .dev/state.json events_path logs/2026-11-22/20261122_011031/dev-loop/events.jsonl does not exist (its dev-loop directory is also absent), so the proof expectation "Dev-loop wait event" cannot be evaluated from this run's journal; tracked by issue-0498cbef4ef3. The build-timings journal is the available pinned evidence.

ROOT CAUSE
This occurrence is not the benign wait-policy-only case seen in earlier reviews: a header-touching change (npc.h) forced 92 TUs to recompile and the edit set also contained a cross-file compile error, so the attempt could not complete within 60s and took 456.9s to fail. Two distinct problems: (a) a flat 60s game_build budget inevitably trips on any full header-fanout rebuild; (b) the edit set removed a field still referenced by src/network/server-npcs.cpp and cost two failed rebuilds to discover it.

VERDICT
Confirmed over-budget wait caused by a real failing header-fanout build, not a false stall. The wait record is complete and the reviewer wake was queued exactly once. The immediate compile defect is already fixed in source and generation 476 is published, so this review requests no new code change. A new issue for this wait_id is correct under the documented per-wait_id keying; the systemic issue-per-attempt flood remains owned by issue-9a4503f6ed68 / issue-e044a931c54b. No runtime_proven behavior proof is claimed or required for this tooling review.

RECOMMENDED (report only; not claimed by this agent)
1. Stage-aware game_build budget derived from observed duration (at least p95) and aggregation of repeated game-dll-build waits instead of one issue per attempt.
2. Populate wait evidence from the in-flight attempt id, generation, and revision.
3. Restore the dev-loop runtime journal so the recorded proof expectation can be checked.
4. For the edit-set defect: header-field removals should update every reference (or keep a compatibility member) so a candidate build does not spend 456.9s to surface a one-line missing member.

CONFIDENCE: high on the pinned attempt facts, the failure chain, and the wake record; high that the immediate defect is fixed in source; medium on the preferred policy remedy (budget vs aggregation), which is an owner decision.
