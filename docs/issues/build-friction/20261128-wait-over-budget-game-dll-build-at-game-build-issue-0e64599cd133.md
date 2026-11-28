# issue-0e64599cd133: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-wait-real;succeeded-at-125s;reason+identity-gap-owned-by-issue-136d4553ff92;budget-noise-owned-by-issue-9a4503f6ed68;wake-timeout-owned-by-issue-84c584957e65)

### 2026-11-28T18:36:34.858Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded review of this wait occurrence. Read-only for source and builds: no source edited, no build/test requested by this agent, no fix or close claimed. Evidence read: this issue record; the root dev-loop journal logs/2026-11-28/20261128_171225/dev-loop/events.jsonl; .dev/build-timings/game-dll-20261128T181102041294Z-a9cdb2.{json,jsonl,resource.json}; build/game/current.json + .dev/state.json; devscripts/dev-loop.py check_wait_budget/_record_game_build_output; devscripts/build_friction.py; and sibling build-friction issues.

WAIT OCCURRENCE (confirmed real; this one SUCCEEDED)
- wait_id game-dll-build:2026-11-28T18:11:01.936Z; started 2026-11-28T18:11:01.936Z; elapsed at check 60.051s; budget 60.0s; stage game_build; request build-request-cb8ecde49504 (dev-loop.control 18:11:02, requested_by tdm-npc-focus-fire).
- In-flight attempt game-dll-20261128T181102041294Z-a9cdb2, source_generation 87, true generation 826, revision 0bb9684fecab7ee5cf946c634d0f3659b7c064fd093f957b7850bcde18fa7b22. Changed src/network/server-gamemode.cpp plus header src/npc/team-brain.h (28 dependents; 29 compile-priority candidates; 30 compiled / 617 skipped).
- The build then published: dev-loop.game-generation.ready 2026-11-28T18:13:07.595Z, duration 125.14s, worker 116.422s, compile 107.985s, link 4.25s, scan 2.985s, publish 0.328s, no_op=false.
- At 60.051s the build was roughly half done (compile alone was 108s), so the trip is accurate, not a hung/blocked wait. Dominant TU src/network/server-gamemode.cpp 93.448s; then map-editor 61.608s, server-npcs 52.062s, server-packets 45.08s, server-packet-chat 41.975s.
- Resource pressure: mean host CPU 98.16%, peak 100.0%, minimum available memory 19.3 MB; queue_wait_max 86.391s, queue_wait_mean 33.955s, jobs 10. Postmortem suggestions match: compile dominates; CPU/memory pressure; per-TU queue wait 86.4s (worker pool starved/contended).

ACCEPTANCE CHECK (present fields good; same attribution gap already owned)
- Present/correct: wait_id, stage, elapsed, budget, started_utc, source_generation (87), request_ids, evidence.
- Incorrect: reason is a corrupted tail, e.g. `:51594.0,"build_attempt_id":"game-dll-20261128T181102041294Z-a9cdb2",...`. Cause: _record_game_build_output sets game_build_detail = line.rstrip()[-240:] for every stdout line, including structured GAME_* events (devscripts/dev-loop.py:3129-3130); check_wait_budget reads it as reason (games-DLL branch ~3912). This is the same defect the 17:31 review localized and already assigned to issue-136d4553ff92 (build-coordinator). Not re-filed here.
- Incorrect/stale identity: build_id is 2026-11-27T17:45:19.700118Z (last successful EXE/runtime artifact), and generation is 825 (last published) while the attempt is 826, because game_build_generation/latest_build update only on success (dev-loop.py ~3276-3286). The true attempt id/generation leak only through the corrupted reason. related_build and related_run are empty. Same owner as above (issue-136d4553ff92).
- "wake queued exactly once for this occurrence": MET. Journal has exactly one dev-loop.wait.over_budget and one dev-loop.wait.reviewer-wake for this wait_id, both at 18:12:24Z; wake.queued=true, issue=issue-0e64599cd133.

RECURRENCE (added here; no duplicate issue created by this reviewer)
Five over-budget game-DLL waits in one dev-loop run (20261128_171225), each a distinct attempt:
1. 17:30:46.287Z -> issue-747933df08a6, gen 821, compile failure (thinkingDue).
2. 17:34:21.168Z -> issue-6ed1b2897c5b, gen 822, link undefined symbol.
3. 17:40:46.167Z -> issue-004c549df353, gen 824, cache-quarantine rebuild.
4. 18:00:34.216Z -> issue-13b902990adc, 63.662s, gen 820. New reliability finding: this occurrence's reviewer wake did not complete. The journal shows dev-loop.wait.issue-failed at 18:02:12.828Z ("agent-wake.py run --once timed out after 30 seconds") and NO dev-loop.wait.over_budget event, even though the issue was created at 18:01:38. wake_reviewer runs `agent-wake.py run --once` synchronously (30s timeout) from check_wait_budget; on timeout the outer except records issue-failed and never reaches reported_wait_ids.add, so the occurrence is mislabeled and can be re-processed/duplicate-woken on a later check.
5. 18:11:01.936Z -> this issue (gen 826), a genuinely successful 125s build.
Brain-wide there are ~46 "Wait over budget: game-dll-build at game_build" issues (gen 429-826), nearly all at 60.1-65.9s. A flat 60s budget is below the normal full-build duration (compile alone 108s here), so this is expected every broad team-brain.h rebuild, not an anomaly. This is exactly the risk stated on this issue ("A noisy wait policy could create duplicate friction issues").

OBSERVED vs INFERENCE
- Observed: the timings, queue waits, resource samples, the single wake, the 18:00 timeout, and the corrupted/stale fields above, all pinned to the named artifacts.
- Inference: that a 60s flat budget should be raised/debounced for game_dll_build (and exe build) or made stage/class-aware; that the timeout ordering should mark the wait reported and journal the created issue before/independently of the wake result. Neither is proven by this review; both are the smallest next checks.

OWNERSHIP / NEXT
- Reason + stale identity + budget-model: already owned by issue-136d4553ff92 (build-coordinator); do not duplicate.
- Budget/noise design of this wait hook: owned by issue-9a4503f6ed68 (wake-proof-wait). Appended one evidence-backed suggestion there with this data point.
- Wake run --once timeout / issue-failed ordering / reported_wait_ids: appended one evidence-backed suggestion to issue-84c584957e65.
- Immediate cluster cause is tdm-npc-focus-fire's team-brain.h fanout rebuild under CPU saturation (their build/lease); not a wait-policy defect.

Confidence: high. No runtime behavior proof applies to this tooling issue; build success is compilation/link evidence only, not behavior proof.
