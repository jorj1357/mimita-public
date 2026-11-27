# issue-ee02d14f4ccd: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-legitimate-wait-no-fix)

### 2026-11-27T00:52:15.550Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: reviewed — legitimate over-budget wait (real progressing build), not a stall. Acceptance met; evidence-identity defects reproduced; no source fix applied.

WAIT: game-dll-build:2026-11-27T00:20:48.836Z (stage game_build, elapsed 60.889s vs budget 60.0s). Run 20261127_001718. Journal: logs/2026-11-27/20261127_001718/dev-loop/events.jsonl.

ACCEPTANCE CHECK
- wait_id: pass
- stage: pass (game_build)
- elapsed/budget: pass (60.889 / 60.0)
- reason: present but unreadable — a mid-JSON tail, not a summary (see defect 1)
- build/run IDs: partial — recorded build_id=2026-11-26T22:52:33.481041Z is the prior EXE artifact and generation=null; the true in-flight attempt appears only inside the reason JSON: game-dll-20261127T002049012082Z-4dffbc, generation 743, source_generation 6, revision 5c1810fd380b022dd87a79ddd127ad912be6ab97e4b3a1b125797919425ac25b (defect 2)
- reviewer wake exactly once: pass — exactly one dev-loop.wait.over_budget and one dev-loop.wait.reviewer-wake for this wait_id (wake launched 00:22:08.299Z, pid 19172)

WHAT THE WAIT ACTUALLY WAS (build-timings game-dll-20261127T002049012082Z-4dffbc.json)
- Attempt started 00:20:49.012Z; staging 00:21:04–00:21:14 (manifest_snapshot 2.047s, source_snapshot 3.391s, codegen cache hit); build began 00:21:15Z with 644 sources, jobs=10.
- TUs were actively completing across the 60s crossing (e.g. module-identity-provider 7.281s; player-render-mesh 40.355s; avatar-commands 42.899s; auth-system 42.027s; vehicle-system 43.62s). This is a genuine header-fanout rebuild: cache_invalidations list src/entities/player.h and src/vehicles/vehicle-types.h, 29+ invalidated objects.
- Terminal result 00:22:41.451Z: FAILED compile, total 112.047s, compile phase 68.844s, 8 compiled / 405 skipped / 644 source_count.
- Compile failure root: src/vehicles/vehicle-system.cpp:2689 'spec' was not declared in this scope (VehicleSystem::updateArcadeControls). Build note note_id 1a8aa179b70c, agent rocket-league-feel. Confidence high this failure came from that uncommitted change. Current working tree no longer has that expression at line 2689 (now a comment; remaining uses at 1741/220/3839 are in-scope), so it is likely already superseded by later edits; no separate issue created.
- Conclusion: the 60s signal is a predictable consequence of a flat budget below natural two-header fanout compile cost, not a hang or resource wedge. Matches the class finding already recorded on issue-7d3dcfd7433b.

EVIDENCE-IDENTITY DEFECTS (reproduced, already tracked)
1. reason corruption: game_build_detail = line.rstrip()[-240:] of the last worker output line (devscripts/dev-loop.py:3112, assigned at 3142; read for the game-dll wait at 3892). For [GAME_FILE] lines this is a raw JSON fragment, so reason is unreadable. Tracked on issue-6e08d89b55e3.
2. stale build/identity: wait arm uses self.latest_build and self.game_build_generation (dev-loop.py:3896-3897) instead of the in-flight attempt/target generation. Tracked on issue-7d3dcfd7433b.
3. recurrence/noise: _wait_issue/record_wait_incident key only on exact wait_id (build_friction.py:131-169), so each new over-60s header-fanout rebuild mints a new issue + wake. Confirmed again here (4 waits in this run alone; 100+ identical open titles). Policy tracked on issue-9a4503f6ed68 and issue-7d3dcfd7433b.

CROSS-LINKS: issue-7d3dcfd7433b (class tracker), issue-9a4503f6ed68 (issue-creation policy), issue-6e08d89b55e3 (reason corruption), issue-136d4553ff92 (DLL build observability umbrella).

NEXT SMALLEST CHECK (not applied; shared dev-loop change needs human approval): (a) make the game-dll budget stage/progress-gated — only fire when no TU has completed for N seconds; (b) dedupe by (kind, stage, active attempt) and append occurrences to one class issue per root cause/time window; (c) record the in-flight build_attempt_id and target generation directly in wait evidence. An occurrence was appended to issue-7d3dcfd7433b rather than creating a duplicate. No source edits; no other agent's files claimed. Confidence: high on the wait classification and the three defects; medium on whether rocket-league-feel's compile error is already resolved.
