# issue-747933df08a6: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-real;attribution+reason-gap-owned-by-issue-136d4553ff92)

### 2026-11-28T17:58:57.805Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded review of this wait occurrence. Read-only for source and builds: no source edited, no build or test requested by this agent, no fix or close claimed. Evidence read: the issue record, the root dev-loop journal `logs/2026-11-28/20261128_171225/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261128T173046371426Z-ba5757.json` (+ `.jsonl`/`.resource.json`), and the current `dev-loop.py`.

WAIT OCCURRENCE (confirmed real, not a stall)
- wait_id `game-dll-build:2026-11-28T17:30:46.287Z`; started 17:30:46.287Z; elapsed at check 60.939s; budget 60.0s; reported stage `game_build`; request `build-request-eb6dfad30815`.
- The waiting attempt is `game-dll-20261128T173046371426Z-ba5757`, source_generation 76, true generation 821, revision `865962dc1007e758f430fadcfdc5b137deb8d2aab7915360071e80e13e68d952`.
- It was progressing, then failed in compile at 17:31:49.350Z (2.06s after the wait check), total 62.969s: compile 54.39s, changed-source preflight 14.75s, object-cache scan 2.734s; compiled 12 / skipped 615. The terminal error is `src/network/server-npcs.cpp:2366: 'thinkingDue' was not declared in this scope` (worker_errors in the attempt JSON).
- Resource pressure was extreme during the attempt: mean host CPU 83.04%, peak host CPU 100.0%, minimum available memory ~4.7 MB (4,894,720 bytes); 55 samples at 1s. Max per-TU queue wait 33.016s. So the overage is 12 real compiles after a `src/npc/team-brain.h` fan-out under host starvation, not a hung/blocked wait.

ACCEPTANCE CHECK (partially unmet; gap already owned elsewhere)
- Present and correct: wait_id, stage, elapsed, budget, started_utc, source_generation, request_ids, evidence.
- Not met/incorrect: `reason` is a corrupted fragment, and the waiting attempt is not reliably named by structured fields. The stored reason is exactly the last 240 chars of the last `[GAME_FILE]` event JSON, e.g. `:60453.0,"build_attempt_id":"...-ba5757","source_generation":76,"generation":821,"revision":"865962dc...","progress_write_ms":6.184,"record_overhead_ms":7.133}`. Source: `_record_game_build_output` sets `game_build_detail = line.rstrip()[-240:]` for every stdout line, including structured `GAME_*` events (`devscripts/dev-loop.py:3129-3130`), and the wait reads it as `reason` (`devscripts/dev-loop.py:3859`, game-DLL branch `reason: self.game_build_detail` around 3912).
- The wait also reports stale identity: `generation` is 820 (last published) while the in-flight attempt is 821, and `build_id` is `2026-11-27T17:45:19.700118Z` (last EXE/runtime artifact), because `game_build_generation` is assigned only on success (`devscripts/dev-loop.py:3277`) and `build_id` comes from `self.latest_build`. The true attempt id/generation leak only through the corrupted reason string. `related_build` and `related_run` are empty.
- This exact reason/attribution defect is already recorded in `issue-136d4553ff92` (build-coordinator), including the `[-240:]` localization (doc entries dated 2026-11-21 and 2026-11-22; e.g. "dev-loop.py:2829-2864 ... wait reason is a truncated GAME_FILE JSON"). I am not creating a duplicate issue for it.

REVIEWER WAKE (meets "exactly once per occurrence")
- Journal has exactly one `dev-loop.wait.over_budget` and one `dev-loop.wait.reviewer-wake` for this wait_id, both at 17:31:56Z; `wake.queued=true`, issue `issue-747933df08a6`. No duplicate wake for this occurrence.

RECURRENCE (added here; no duplicate issue created by me)
Three distinct over-budget waits in ~10 minutes, each a different attempt and each producing one issue + one wake:
1. 17:30:46.287Z -> issue-747933df08a6, attempt ba5757, gen 821, compile failure (`thinkingDue`).
2. 17:34:21.168Z -> issue-6ed1b2897c5b, attempt 3b08fd, gen 822; link failed with undefined `MimitaNet::handleSpyKnifeHitClaim` (referenced `server-packets.cpp:2799-2831`; definition exists at `src/network/server-packet-handlers.cpp:1548`, i.e. the cache-fault signature, not missing code).
3. 17:40:46.167Z -> issue-004c549df353, attempt eb3d52, gen 824; object-cache quarantine cold rebuild (build note "two distinct undefined-symbol link failures"); still `status=building` as of 17:53:35Z.
This realizes the risk stated on this issue ("A noisy wait policy could create duplicate friction issues") and matches the flat-60s-budget finding already in issue-136d4553ff92.

ASSESSMENT / OWNERSHIP / NEXT
- The wait detection itself is correct for this occurrence: a genuine >60s build that then failed. The policy gap is attribution/reason/budget, owned by issue-136d4553ff92 (build-coordinator). Smallest next check there (already listed, restated for this data point): pass the active attempt id and target generation into wait evidence as structured fields, derive a readable `reason` from the parsed event payload instead of the 240-char raw tail, and set related_build/related_run; decide a stage/class-aware budget so a cold rebuild or a >60s compile set does not page a reviewer every retry.
- The immediate cluster cause is elsewhere: `tdm-npc-focus-fire` owns the in-flight edits/retries (active leases on `team-brain.*`, `server-npcs.cpp`, `server-gamemode.*`, `physics-collision-glb-sweep-slide.cpp`) and is executing the cache-quarantine runbook; the compile error and the cache-fault link are theirs, not wait-policy defects.
- Confidence: high. Claims are pinned to the attempt JSON/JSONL, the run journal wait/wake events, and current source lines. No runtime behavior proof is relevant to this tooling issue.
