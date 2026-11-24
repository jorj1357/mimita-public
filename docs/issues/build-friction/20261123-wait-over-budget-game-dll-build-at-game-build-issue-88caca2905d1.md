# issue-88caca2905d1: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-not-a-stall;flat-budget-false-positive;stale-identity-recurred)

### 2026-11-23T19:26:23.316Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded build-wait review (build-wait-reviewer, wake-bb564fe4680a), read-only. No source edits, no files claimed. Verdict: acceptance met for this occurrence; the wait is a healthy long build, not a stall; the known stale-identity defect recurred.

OBSERVED
1. Single occurrence, single issue, single wake. wait_id game-dll-build:2026-11-23T19:13:41.970Z (stage game_build, elapsed 60.792s, budget 60.0s). Journal logs/2026-11-23/20261123_184017/dev-loop/events.jsonl has exactly one dev-loop.wait.over_budget (line 427, created=true) and exactly one dev-loop.wait.reviewer-wake (line 426) for this wait_id; no second journal mentions it. Wake wake-bb564fe4680a (.dev/agent-wake/requests/wake-bb564fe4680a.json) created 19:14:43.408Z, launched 19:14:50.884Z, queued once; this session is that wake. Acceptance ("one issue, one reviewer wake, wait_id/stage/reason/elapsed/IDs present") is satisfied.
2. The flagged build was healthy and not a stall. The wait fired during Mode 3 batch request 2026-11-23T19:13:37.823Z-2dae4ae4:mode3:game (AI-sealed at 19:13:46.938Z, 6 changed compiler inputs incl. two high-fanout headers). In-flight attempt game-dll-20261123T191346937240Z-35f07c (generation 570, source_generation 4) published successfully: dev-loop.game-generation.ready 19:20:49.235Z duration 420.906s, worker_total 398.887s; build/game/current.json total 417.695s, compile 385.061s, link 7.555s, 67 compiled / 576 skipped, changed_source_preflight 61.307s. Fanout: src/gamemode/gamemode.h=51, src/render/post-fx.h=20; slowest TU src/network/server-gamemode.cpp=157.5s. A flat 60s total-elapsed budget is simply below a legitimate high-fanout build, so this is an expected false positive, same root cause already recorded on issue-a236165fa6fc and tracked on issue-9a4503f6ed68.
3. Stale-identity defect recurred. Wait evidence records generation=null and build_id=2026-11-23T18:39:16.832027Z (the previous EXE build), while the actual in-flight attempt was generation 570 / game-dll-20261123T191346937240Z-35f07c (started from batch seal 19:13:46.938Z). This is the dev-loop.py check_wait_budget identity source defect already tracked on issue-9a4503f6ed68; it means this occurrence also does not carry the true build/run ID.
4. Reason field is clean this time: reason="[GAME BUILD] revision=30166b5b... sources=642 jobs=10 priority=high cache=build\game\objects\b10e3a31b3ae8637 seeded=0" (no truncated-JSON contamination), so the reason-quality defect on issue-6e08d89b55e3 did not recur.

SECONDARY DEFECT (not this wait's acceptance)
5. During the same run an exe-build wait (issue-3e601947fc16) was handled at 19:22:04.022Z; build_friction.wake_reviewer's synchronous `agent-wake.py run --once` failed with PermissionError [WinError 5] replacing wake-bb564fe4680a.json.tmp -> wake-bb564fe4680a.json (journal line 582). This was the retry/state write for THIS wake request (attempts=2), not a new wait id; the request still launched. It is the known wake request-store write race (cross-linked on issue-6907072bb155) plus the blocking synchronous drain (issue-9a4503f6ed68). Left to those owners; not duplicated here.

INFERRED / UNKNOWN
- Automation (detect -> one issue -> one wake -> durable journal) is working; remaining defects are policy (flat 60s vs legitimate builds) and identity (wrong generation/build_id), not the hook.
- Unknown: whether the flat budget should key on a phase/threshold that excludes healthy high-fanout recompiles; that is a policy decision for the aggregator owner (issue-9a4503f6ed68), not resolvable here.

NEXT DISCRIMINATING CHECK (for aggregator owner)
- Compare this pair with a future attempt: wait evidence should carry the live attempt id/generation once the identity source is fixed (issue-9a4503f6ed68). If it still shows the previous EXE build id, the fix did not take.
