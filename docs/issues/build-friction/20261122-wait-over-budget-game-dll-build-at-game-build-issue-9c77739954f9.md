# issue-9c77739954f9: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (false-positive-no-stall)

### 2026-11-22T07:36:39.028Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bound review of issue-9c77739954f9 (game-dll-build wait at game_build, elapsed 60.766s, wait_id game-dll-build:2026-11-22T06:52:24.599Z).

VERDICT: False-positive friction. The build was NOT stalled; it completed successfully 28.3s after the 60s alarm.

EVIDENCE
- Build attempt game-dll-20261122T065224709945Z-9a8cc6, generation 470, revision 0a705a43, changed source src/npc/npc-navigator.cpp. See .dev/build-timings/game-dll-20261122T065224709945Z-9a8cc6.json and .jsonl.
- success: true, total_seconds: 89.078, started 06:52:24.710Z, completed 06:53:53.797Z. Build resolved 28.7s after the wait was recorded (wake created 06:53:25.849Z).
- Measured phases: build start -> worker start ~22.0s setup/snapshot (unaccounted in phase sums); changed_source_preflight 29.1s (dominated by compiling npc-navigator.cpp at 28.578s); object_cache_scan 2.2s; compile(module_identity_provider) 3.3s; link 29.7s (gnu ld, 480,395,207 object-input bytes -> 242,705,300-byte DLL); coherence 0.43s; publish 0.76s.
- The wait fired at 60.766s, i.e. during the link phase (link began at ~58.0s elapsed). Every phase made forward progress; no stall/timeout/retry.
- Recent successful single-change candidate DLL builds routinely exceed 60s: gen472 191.9s, gen471 104.2s, gen469 118.7s, gen468 74.6s, gen467 61.7s, gen466 74.1s (no-op builds are 7-10s). The 60s WAIT_ISSUE_SECONDS threshold (devscripts/dev-loop.py:109) is below the normal cost of a real changed-source candidate build.

EXACTLY-ONCE WAKE: Confirmed. One wake request file .dev/agent-wake/requests/wake-4fb8ebd28b4f.json for this issue, created 06:53:25.849Z; no duplicate issue for this wait_id (build_friction.record_wait_incident dedups on wait_id).

INTAKE DATA-QUALITY DEFECTS (separate code fix, not edited here)
- evidence.build_id = "2026-11-22T02:37:06.972018Z" is the latest EXE build id, not the game-DLL attempt id; the real attempt id is only recoverable by parsing the next field.
- evidence.generation = 469 is stale (actual attempt generation is 470); self.game_build_generation lags the active attempt.
- evidence.reason is a truncated raw jsonl tail from self.game_build_detail, not a readable stage reason.

RECOMMENDED FIX (claim separately; not applied): either (a) raise/relax WAIT_ISSUE_SECONDS for the game-dll-build candidate path based on measured typical cost, or better (b) alarm on absence of timeline progress (no advance in .dev/build-timings/<attempt>.jsonl for N seconds) instead of absolute elapsed; and populate the wait payload with build_attempt_id, the active generation, and a human-readable stage detail. This is the exact noisy-wait-policy risk recorded on the issue.

SCOPE: Review/evidence only. No source edited, no other agent's lease touched.
