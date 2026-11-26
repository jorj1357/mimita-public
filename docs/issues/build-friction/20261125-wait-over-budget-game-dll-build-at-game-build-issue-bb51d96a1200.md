# issue-bb51d96a1200: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (reviewed-needs-followup)

### 2026-11-25T22:32:48.386Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer
Reviewed UTC: 2026-11-25T22:32Z
Dev-loop run: 20261125_212905
Journal: logs/2026-11-25/20261125_212905/dev-loop/events.jsonl
Wait: game-dll-build:2026-11-25T21:53:31.363Z (stage game_build) - distinct occurrence, not a duplicate.

Acceptance check (evidence):
- wait_id: PASS. Issue evidence wait_id matches the single dev-loop.wait.over_budget event (21:54:45.339Z) and the single dev-loop.wait.reviewer-wake event (21:54:45.231Z) for this wait. grep count = 1 each.
- stage: PASS (game_build).
- elapsed: PASS 61.397s vs 60.0s budget; build was genuinely still compiling, not idle.
- evidence: PASS.
- reason: QUALITY FAIL. Value is a truncated mid-JSON tail (e.g. "9906.0,\"build_attempt_id\":...") from game_build_detail = last 240 chars of a [GAME_FILE] worker line (dev-loop.py:3112,3142; consumed at 3892). Tracked by issue-6e08d89b55e3; no new issue filed.
- build/run identity: PARTIAL. evidence.build_id=2026-11-25T20:35:21.601166Z is the prior latest EXE artifact and generation=null; the in-flight game-DLL attempt game-dll-20261125T215331554351Z-19bd08 / generation 651 appear only inside the corrupt reason tail. Tracked by issue-7d3dcfd7433b.
- reviewer wake queued exactly once: PASS on queue, but dispatch was late. wake-88fb777780c8 queued 21:54:34.570Z; codex launched 21:54:45.150Z; a "stale launched request recovered by scheduler" retry fired 22:29:01.006Z; opencode launched 22:29:01.956Z (~34m16s after queue). Tracked by issue-84c584957e65.

Build outcome (not a stalled worker): attempt 19bd08 SUCCEEDED, generation 651, total_seconds=84.812, compile=64.235s, link=9.266s, compiled=27, skipped=618 (dev-loop.game-build.result @21:54:55.108Z; dev-loop.game-generation.ready @21:54:56.827Z). The wait crossed 60s while compiling and the build finished ~23.4s later; the signal is a true build-friction record. Compile alone (64.2s) exceeds the 60s budget on a 27-TU actor-body.h fanout rebuild.

Resource evidence (observed): peak host CPU 100%, mean 98.434%, minimum available memory 66,490,368 B (~63.4 MiB), 68 samples over 84.797s (.dev/build-timings/game-dll-20261125T215331554351Z-19bd08.resource.json). Appended to issue-2a5c5da53565.

Cross-links: issue-6e08d89b55e3 (reason), issue-7d3dcfd7433b (identity/latency), issue-84c584957e65 (wake dispatch), issue-136d4553ff92 (stage speed), issue-9a4503f6ed68 (auto-issue pipeline), issue-2a5c5da53565 (resource pressure), sibling waits issue-8b7208f291a8 (21:39:39) and issue-63ec094b8859 (21:45:58).

Conclusion: one genuine over-budget wait recorded exactly once with one queued reviewer wake. Reason readability and build-identity fields are defective but already tracked. No duplicate issue created; no code edited or claimed; this occurrence needs no separate code fix.
