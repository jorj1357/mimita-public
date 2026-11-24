# issue-5643605344b4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-healthy-long-build-flat-budget; per-attempt-duplicate; identity-and-reason-known-defects-recur)

### 2026-11-23T22:41:16.565Z — build-wait-reviewer

Reviewer: build-wait-reviewer

**Bounded build-wait review — build-wait-reviewer, wake-e3c9f4d9f098, 2026-11-23T22:2xZ UTC.** Read-only: no source edited, no build/run started, no fix claimed, no proof recorded. State left `new` and unowned.

**Observed facts**
- Wait identical to record: `wait_id=game-dll-build:2026-11-23T21:51:15.080Z`, kind `game-dll-build`, stage `game_build`, elapsed 60.37s, budget 60.0s. Journal `logs/2026-11-23/20261123_192224/dev-loop/events.jsonl` has `dev-loop.wait.over_budget` + `dev-loop.wait.reviewer-wake` at wall_time 2026-11-23T21:52:16.574Z (run 20261123_192224). Acceptance fields are present (wait_id, stage, reason, elapsed, request_ids, evidence), so the issue is complete.
- The flagged build **succeeded**. Attempt `game-dll-20261123T215115211280Z-e6886c`, generation 572, revision 9932401889fbd2086fa31ebf9982f017b160229043df0e66dce4f222d08aa0f4, from `.dev/build-timings/game-dll-20261123T215115211280Z-e6886c.json`: started 21:51:15.211Z, completed 21:52:26.906Z, `success=true`, `total_seconds=71.695`; compile 57.774s (3 compiled / 640 skipped; slowest `src/network/server-gamemode.cpp` 56.274s, `src/npc/npc.cpp` 32.095s), link 5.347s, object_scan 2.986s, publish 0.743s.
- So the wait fired at 60.37s (~84% of a 71.7s build) while the slowest TU was still compiling; nothing stalled, and the build finished 11.3s later. Resource sample for the same attempt: `mean_host_cpu_percent=99.578`, `peak_host_cpu_percent=100.0`, `minimum_available_memory_bytes=869203968` (~829 MiB), 68 samples over 71.69s. The build is compile-bound at full CPU and near memory floor.
- Wake chain: one request `wake-e3c9f4d9f098` queued 21:52:15.878Z (`.dev/agent-wake/events.jsonl`). Primary adapter `codex` launched 21:52:16.547Z (pid 32856) then exited without reviewing; `wake.retry` 22:10:52.365Z switched to `opencode`; `opencode` claimed 22:20:28.855Z and launched 22:20:29.420Z (pid 28580). `requests/wake-e3c9f4d9f098.json` shows `attempts: 2`. One wake request, but the actual review began ~28 min after the wait.
- Identity defect recurs: wait evidence carries `build_id=2026-11-23T19:22:15.804284Z` (previously published EXE build) and `generation=null`, while the in-flight attempt is `game-dll-20261123T215115211280Z-e6886c` / generation 572, visible only inside the contaminated `reason`.
- Reason defect recurs: `reason` is a mid-JSON tail `:40420.29,"build_attempt_id":"game-dll-20261123T215115211280Z-e6886c",...,"progress_write_ms":3.565,"record_overhead_ms":4.235}`.
- Duplication recurs: the next build wait (`game-dll-build:2026-11-23T22:09:45.196Z`, elapsed 60.745s) minted sibling `issue-1f0c919bdbaa`. Both are `new`, unowned, same title. This is the per-attempt dedupe (key = wait_id embedding start time) already counted at ~127 same-title issues.

**Inference / uncertainty**
- A flat 60s budget is below the normal cost of a changed-TU whole-game DLL build under 100% host CPU; a healthy >60s compile will mint one issue + one reviewer wake every build. The underlying friction (slow compile, ~57s single TU) is real but belongs to issue-136d4553ff92 / build-coordinator; the detector cannot tell "healthy long compile" from "stalled wait."
- This occurrence satisfies its literal acceptance (one issue, one wake request, no second scan duplicate) yet misses useful intent: it does not identify the in-flight build. Whether to change budget policy is a human/owner decision, not mine.

**Contrary evidence**
- The build completed successfully 11.3s after the wait fired, so this is not a stalled/failed build, and the detect→issue→wake→journal hook is functioning.

**Known-issue cross-links (recurrences appended there, no new issues)**
- Policy + per-attempt duplication: issue-9a4503f6ed68 (owner wake-proof-wait).
- codex-primary exit / fallback delay: issue-84c584957e65.
- mid-JSON reason: issue-6e08d89b55e3.
- Slow build inventory: issue-136d4553ff92 (owner build-coordinator).

**Next smallest discriminating checks**
1. Owner decides a stage-aware `game-dll-build` budget (or compares against last-build p90). Prove with the next two builds that a healthy >60s compile does NOT mint a new issue while a genuine stall does.
2. Read the in-flight `build_attempt_id`/generation from `dev-loop.game-build.started` for the wait record instead of the last published build; clean `reason` per issue-6e08d89b55e3.
3. Verify one codex→opencode fallback is not retried blindly and meets the <=5 min target per issue-84c584957e65.
