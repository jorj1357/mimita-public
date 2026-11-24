# issue-816c05804889: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (true-positive-expected-build-duration; identity-and-latency-gaps-tracked)

### 2026-11-24T09:45:24.307Z — build-wait-reviewer

Reviewer: build-wait-reviewer

True positive: a real, successful game-DLL build crossed the flat 60s budget; not a hung worker. Issue creation and exactly-one reviewer wake are satisfied, but the record carries the wrong build identity (gap 1).

Direct evidence (UTC):
- Journal logs/2026-11-24/20261124_075114/dev-loop/events.jsonl lines 51-52: dev-loop.wait.reviewer-wake 07:54:24.968Z and dev-loop.wait.over_budget 07:54:24.969Z with created=true. Issue created 07:54:17.471Z.
- Wake .dev/agent-wake/events.jsonl: wake-30dce0a84a51 queued once at 07:54:20.143Z; the reviewer-wake event embeds wake-30dce0a84a51 (no misattribution this occurrence).
- Build attempt game-dll-20261124T075315478413Z-04a3b7, generation 587, success, published 07:56:57.014Z. .dev/build-timings/game-dll-20261124T075315478413Z-04a3b7.json: total_seconds 221.684, worker_total_seconds 163.672, compiled 2 / skipped 643, jobs 10, compile_max_ms 101056.665, link 53.644s over 644 inputs / 491,604,041 bytes.
- Stage timeline (journal elapsed_ms): manifest_snapshot 10.552s; manifest_validation 0.022s; source_snapshot 1.839s (junction_overlay); packet_registry_codegen 9.885s; game-build started 07:54:06.237Z (elapsed 51.6s); src/vehicles/vehicle-system.cpp completed 07:55:55.212Z (101.057s); module-identity-provider 4.166s; link 53.644s.
- Direct resource sample (attempt scope): 176 samples @1s, started 07:53:15.478Z, mean host CPU 99.994%, peak 100%, minimum available memory 17,457,152 B (~16.6 MiB) of ~15.65 GiB. Runtime alignment unavailable (fresh_server_and_client_journal_evidence_not_matched), so this is build-phase pressure only, not a live-mode claim. Cross-linked to issue-2a5c5da53565.

Gaps/follow-up (not new; existing owners):
1. Identity: evidence.build_id 2026-11-24T07:48:35.684015Z is the launched EXE artifact, not the in-flight game-DLL attempt; generation is null and there is no build_attempt_id/run_id. So the record does not carry the game-DLL build/run identity its acceptance asks for. Owned by issue-136d4553ff92 / issue-9a4503f6ed68.
2. Duration: elapsed_seconds 62.252 is the budget-crossing sample, not total wait (~221.7s from 07:53:14.620Z to publish 07:56:57.014Z). Same measurement trackers.
3. Dispatch: codex claimed/launched 07:54:24.597/24.892Z but delivered nothing; wake.retry 08:32:49.965Z (stale launched request recovered by scheduler) and opencode claimed/launched 09:20:38.674/38.761Z, 5178.6s (~86.3 min) after queue. Recorded as an occurrence on issue-84c584957e65.

No code changed; no source/build claimed. Reviewer: build-wait-reviewer.
