# Over-budget game-DLL wait review 34993e32 (build-wait-reviewer)

Date: 2026-11-22 ~18:02Z
Agent: build-wait-reviewer
Context: wake-0eacfd7f99aa
Issue: issue-34993e327e19 (Wait over budget: game-dll-build at game_build)

## Scope

Read-only evidence review. No source, config, or build-script edits were made. No
compiler, build.py, module worker, or second dev-loop was invoked. No new runtime
was started, no files were leased, and no duplicate issue was created.

## Wait occurrence

- wait_id `game-dll-build:2026-11-22T17:17:22.162Z`, stage `game_build`,
  elapsed 60.227s, budget 60.0s, request `2026-11-22T17:17:20.462Z-c2fd63f7`,
  source_generation 0, generation null.
- Matching attempt `.dev/build-timings/game-dll-20261122T171723932165Z-86a612.json`:
  started 17:17:23.932Z, completed 17:21:59.497Z, generation 502,
  success=false, total 275.557s, failed_phase `changed_source_preflight`,
  worker error `changed-source preflight failed: src/npc/npc.cpp in 202.979s`
  (`npc.cpp:3836-3837` `class Npc has no member named hasLoggedDash`).
- At the 60.227s mark the attempt was still in `changed_source_preflight`
  (source_snapshot completed 17:18:16.484Z, packet_registry codegen completed
  17:18:22.377Z; npc.cpp/server-npcs.cpp preflight compiles still running), so
  the overrun is real build latency, not a released or stalled worker.
- Cluster: retry `game-dll-20261122T172200384686Z-a8e690` failed npc.cpp preflight
  (65.67s, total 105.448s); `game-dll-20261122T173219711862Z-89e54d` failed
  npc.cpp preflight again (27.429s, request `auto-5`). Same failure class as
  issue-b24f9f79794d (07:44) and issue-a7464f307b2f (07:52); current tree has no
  `hasLoggedDash` in `src/npc/npc.cpp` or `src/npc/npc.h`.
- Wake `.dev/agent-wake/events.jsonl` for wake-0eacfd7f99aa: queued 17:18:27.783Z;
  codex claimed/launched 17:18:36.732Z/17:18:37.191Z then exited; wake.retry to
  opencode 17:23:09.539Z; opencode claimed/launched 17:24:14.245Z/17:24:14.446Z.
  Queued exactly once, but dispatch to a working launch was ~346.7s (5m46.7s).

## Findings

1. Legitimate over-budget wait caused by a failing changed-source preflight on
   npc.cpp (concurrent shared-source field mismatch), followed by auto-retries.
2. Wake queued exactly once for this wait occurrence; no duplicate wait issue.
3. Evidence gaps (reported, not fixed):
   - `dev-loop.wait.reviewer-wake` at 17:18:39.195Z is stamped with this
     wait_id/issue but embeds the stale wake-53e8678c15f2
     (issue-cfe902b4b659), not wake-0eacfd7f99aa.
   - wait evidence `build_id` is the prior published artifact
     `2026-11-22T08:00:31.376619Z` and `generation` is null; the in-flight
     attempt id/generation (86a612 / 502) is only in build-timings.
   - dispatch delay 5m46.7s exceeds the proposed <=5 min target
     (issue-84c584957e65).
4. The 60s budget remains below normal game-DLL compile/preflight latency;
   issue-per-attempt flood remains owned by issue-9a4503f6ed68 and
   issue-136d4553ff92.

## Claims

- Source evidence: review-only (devscripts/dev-loop.py, devscripts/build_friction.py,
  build timings, agent-wake events).
- Build evidence: attempt game-dll-20261122T171723932165Z-86a612, generation 502,
  275.557s, failed at changed_source_preflight.
- Runtime evidence: agent-wake events only; no gameplay runtime required.
- Human acceptance: not performed.

## Handoff

- Appended review verdict `confirmed-real-wait; root-cause=compile-error;
  evidence-gaps` to issue-34993e327e19; state `in_review`.
- Appended one structured occurrence to issue-84c584957e65 (late reviewer
  dispatch) with the wake sequence, 346.7s delay, and misattribution evidence
  instead of creating a duplicate issue.
- No code fix claimed.
