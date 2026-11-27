# issue-a5ad251099c8: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-genuine-wait; false-issue-failed-label; 1h56m-wake-dispatch-delay; stale build_id/generation)

### 2026-11-27T20:35:59.992Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review by build-wait-reviewer (wake-24cb1aefae82). Read-only: no source edited, no build/run started, no runtime controlled. Evidence is source/build/journal, not behavior proof.

OCCURRENCE VERIFIED
- wait_id game-dll-build:2026-11-27T18:32:32.984Z; started_utc 18:32:32.984Z; elapsed 60.153s; budget 60.0s; stage game_build; source_generation 82; request_ids [build-request-dcee2f56ff3e].
- Pinned attempt: game-dll-20261127T183233046312Z-d5be13, generation 804, revision 7c1aaa711d0b089af43c826b18ecb3054140d4610532324aaf653daf02315781, success=true, completed 18:35:45.005Z, total 191.959s. Phases: changed_source_preflight 123.538s, compile 124.696s, link 46.214s, publish 0.515s (.dev/build-timings/game-dll-20261127T183233046312Z-d5be13.json).
- Slowest TU: src/npc/recast-navigation.cpp 123.472s (gcc time-report: parsing 58.76s/49%, template instantiation 52.80s/44%). Link: 646 object inputs / 506,491,652 bytes. Build note (map-authored-parity) declared expected_seconds 180.
- Conclusion: the wait is genuine, not a hang. A 60s budget is ~3.2x smaller than this attempt's 192s total. No code fault in the build itself.

ACCEPTANCE
- One issue created for this wait (coord.issue.opened, 18:33:33). Exactly one reviewer wake queued: wake.queued 18:33:34.719Z; claimed codex 18:34:04.119Z; launched codex 18:34:04.592Z; wake.retry opencode 18:47:45.495Z; claimed+launched opencode 20:29:57.98Z (.dev/agent-wake/events.jsonl L4025-4065). The codex and opencode launches are the same single request, not a second wake. "Exactly one wake" holds.

FIRST DIVERGENCE (this occurrence) - false failure label
- dev-loop event dev-loop.wait.issue-failed at 18:34:04.955Z says "could not record over-budget wait" with error "agent-wake.py run --once timed out after 30 seconds" (logs/2026-11-27/20261127_174537/dev-loop/events.jsonl L160). The issue and wake were in fact recorded/queued. Cause: build_friction.wake_reviewer runs `agent-wake.py run --once` synchronously with timeout=30 (devscripts/build_friction.py:244-246); the codex adapter did not finish within 30s, TimeoutExpired propagated, and check_wait_budget logged the exception as a recording failure (devscripts/dev-loop.py:3914-3931). Consequence: no dev-loop.wait.over_budget or dev-loop.wait.reviewer-wake event was emitted for this wait. Siblings at 17:46:16, 18:16:21, 18:21:05 show the identical false issue-failed; only the 18:41:02 wait emitted over_budget. So the normal path is currently "record, then log a false failure".

DISPATCH LATENCY (this occurrence)
- Build was ready at 18:35:45. The review wake queued at 18:33:34, the codex adapter launched at 18:34:04 and exited, and the opencode fallback was not launched until 20:29:57 - 1h55m55s after the wait, 1h54m12s after the build was ready. The reviewer is reviewing a long-completed build. This is the wait-to-review latency failure, not a wait-detection failure.

EVIDENCE QUALITY (consistent with sibling issue-4ddfb305b25b)
- Recorded build_id 2026-11-27T17:45:19.700118Z is the last EXE build, not this DLL attempt. Recorded generation 803 is the prior published value; the attempt completed as generation 804. A reviewer must read request_ids/attempt from the journal to identify the real build.
- reason is readable here ("[GAME BUILD] revision=... sources=646 jobs=10 priority=high ..."), so the [-240:] garbled-reason defect is intermittent, not universal.

RESOURCE EVIDENCE / GAP
- d5be13 sampling: mean host CPU 99.578%, peak 100.0%, min free memory 2672 MB. runtime_alignment: server vehicle_freeplay, client ~207 FPS (uncapped), 0 NPCs / 1 player. Unavailable: matched_prebuild_resource_baseline, host_commit_charge_and_limit, independently confirmed client mode. So "host contention caused the 123s TU" is INFERENCE, not proven; the direct observation is that host CPU was saturated while a live 207 FPS client and server ran.

RECURRENCE (risk confirmed)
- 254 issues titled "Wait over budget: game-dll-build at game_build" exist. wait_id embeds started_utc (dev-loop.py:3890), so every >60s build opens a new issue even when cause/title are identical. That is the risk field realized; it also contradicts the wake prompt's "add the occurrence instead of creating a duplicate issue". Cross-links: issue-e044a931c54b (wait elimination, owner build-runtime-subagents), issue-4ddfb305b25b and issue-5ac7ba75d89d (same evidence-quality and dispatch findings). No duplicate issue created here.

STATUS: reviewed; wait confirmed genuine; no fix claimed; no runtime_proven claim; no source edit or build/run requested.

## Context

### 2026-11-27T20:36:05.611Z — build-wait-reviewer

related_build: game-dll-20261127T183233046312Z-d5be13 (generation 804; evidence build_id 2026-11-27T17:45:19.700118Z and generation 803 are stale)
related_run: 20261127_174537
