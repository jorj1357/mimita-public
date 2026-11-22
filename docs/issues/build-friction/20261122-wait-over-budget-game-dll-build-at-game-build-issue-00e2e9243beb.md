# issue-00e2e9243beb: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (findings)

### 2026-11-22T06:59:12.620Z — build-wait-reviewer

Reviewer: build-wait-reviewer

BLOCKED / FINDINGS (evidence collected; issue held in review, no code edited).

WAIT OCCURRENCE
- wait_id game-dll-build:2026-11-22T06:21:19.547Z; stage game_build; elapsed 60.036s; budget 60s.
- Actual game-DLL attempt: .dev/build-timings/game-dll-20261122T062119640904Z-8db74c.json, generation 465, changed src/npc/npc.cpp, success=true, total_seconds=121.59, completed 06:23:21.246Z.
- Cost breakdown: changed_source_preflight 69.18s (single TU src/npc/npc.cpp compile 68.873s), link 20.477s, object_cache_scan 4.937s. The wait fired at 60s simply because the build normally takes over 60s.

VERIFIED
- Issue record carries wait_id, stage, reason, elapsed, request_ids (auto-7), source_generation (7), and evidence: yes.
- Exactly one reviewer wake was queued for this occurrence: .dev/agent-wake/requests/wake-2aaad8bf8bbb.json (dedupe ok). Wake log: queued 06:22:21.098Z, claimed codex 06:22:22.183Z, launched pid 32852; retry event 06:30:49.997Z; opencode claimed 06:35:13.979Z, launched 06:35:14.013Z. Queue-to-effective-launch ~173s.
- Wake events are append-only in .dev/agent-wake/events.jsonl lines 1310, 1312, 1313, 1317, 1326, 1327.

FINDINGS
1) MISLEADING BUILD/GENERATION IDENTITY IN WAIT EVIDENCE.
   Evidence has build_id=2026-11-22T02:37:06.972018Z (latest EXE build) and generation=464, but the attempt that was building is game-dll-20261122T062119640904Z-8db74c / generation 465.
   Cause: devscripts/dev-loop.py:3525-3535 builds the wait from self.game_build_generation and self.latest_build. self.game_build_generation is only assigned on completion (devscripts/dev-loop.py:2967), so during building it still holds the previous published generation. Acceptance says the issue must carry build/run IDs; this field is wrong for the wait it describes.
   Fix: record the active attempt id/generation (game_build_active_revision or the worker build_attempt_id) and stop putting the EXE build in the game-DLL wait evidence.

2) PROOF EXPECTATION DEV-LOOP WAIT EVENT CANNOT BE VERIFIED FOR THIS RUN.
   .dev/state.json events_path = C:\mimita-v9\logs\2026-11-22\20261122_011031\dev-loop\events.jsonl, but that file/dir does not exist. No other dev-loop/events.jsonl for run 20261122_011031 exists. record_runtime_event (devscripts/dev-loop.py:2292-2297) only catches OSError and prints to console, so a missing parent dir silently drops dev-loop.wait.over_budget and reviewer-wake journal evidence. The issue and wake records survive, but the journal half of the proof expectation is absent. Suggest creating the parent dir (or falling back to a known-good path) before append.

3) BUDGET IS BELOW NORMAL BUILD DURATION -> ISSUE FLOOD.
   34 of 44 game-dll attempts on 2026-11-22 exceeded 60s (median 122.9s, max 700.6s). Result: 94 open Wait over budget: game-dll-build at game_build issues (106 build-friction issues total; 74 in_review). This is the risk recorded in the issue record. Recommend raising/scaling the game-DLL wait budget (e.g. 180s) and/or aggregating one open issue per kind+stage over a time window with occurrence counts, while keeping one reviewer wake per new signature.

RELATED
- Existing owner issue issue-9a4503f6ed68 (wake-proof-wait) defines this wait-hook contract; added a suggestion there with these evidence pointers.
- Known launch-path risk remains: devscripts/build_friction.py:244-246 runs agent-wake.py run --once synchronously with timeout=30 inside the wait try block; on timeout it raises and the outer handler records dev-loop.wait.issue-failed instead of over_budget/reviewer-wake after the issue/wake already exist. Not proven for this occurrence (codex retry, not a Python timeout), but reachable.

NO CODE CHANGED. Reviewer did not claim or edit any leased file.
