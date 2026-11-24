# issue-c7d9edb5e68b: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-24T12:21:49.560Z — build-wait-reviewer

related_build: game-dll-20261124T111512931313Z-6d43ee
related_run: 20261124_084513

## Review (confirmed-build-wait)

### 2026-11-24T12:22:16.079Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review by build-wait-reviewer (wake-22eb58218fa1), read-only. No source edit, build, close, or fix performed.

OBSERVED FACTS
1) Wait confirmed. wait_id game-dll-build:2026-11-24T11:15:12.525Z, stage game_build, elapsed 60.464s vs budget 60.0s, started_utc 2026-11-24T11:15:12.525Z. Journal logs/2026-11-24/20261124_084513/dev-loop/events.jsonl line 520 = dev-loop.wait.over_budget (created=true); line 519 = dev-loop.wait.reviewer-wake for the same wait_id, wake-22eb58218fa1, adapter codex, launch_error empty. Exactly one issue and exactly one reviewer wake for this wait_id, so acceptance 'reviewer wake queued exactly once' is met.

2) The flagged build was healthy, mid-progress, not stalled. In-flight attempt game-dll-20261124T111512931313Z-6d43ee (source_generation 72, revision d86144b...) dev-loop.game-build.result at 11:21:58.820Z: status ok, generation 591, compiled 79, skipped 566, compile_seconds 373.584, link_seconds 7.671, total_seconds 405.507, worker_total_seconds 389.228 (.dev/build-timings/game-dll-20261124T111512931313Z-6d43ee.json). At the 11:16:13 wait fire, src/gamemode/gamemode.cpp was still compiling (that TU took 56.652s, completed 11:16:27.171Z), i.e. active progress.

3) ACCEPTANCE GAP - recorded build/run identity is stale. evidence.build_id=2026-11-24T07:48:35.684015Z and generation=590 are the previously published EXE/generation, not the in-flight attempt (gen 591 / game-dll-20261124T111512931313Z-6d43ee); run_id is absent. Root: dev-loop.py:3896 build_id=self.latest_build and :3897 generation=self.game_build_generation, which is assigned only on completion (dev-loop.py:3259). Known identity defect tracked on issue-9a4503f6ed68; I filled this issue's related_build/related_run from the journal.

4) ACCEPTANCE GAP - reason is unreadable. evidence.reason is a mid-JSON tail of a [GAME_FILE] translation-unit compiler_timing ('reload CSE regs ... truncated':true},progress_write_ms...), not a stage summary. Root: dev-loop.py:3112 detail=line.rstrip()[-240:] -> game_build_detail at :3142 -> reason at :3892. Tracked on issue-6e08d89b55e3.

5) Systemic cause. Flat WAIT_ISSUE_SECONDS=60 (dev-loop.py:114-115) sits below normal whole-game DLL cost: this attempt 405.5s total; the sibling gen-593 build 163.4s. record_wait_incident keys on per-attempt wait_id (build_friction.py:131-169), so each >60s build mints a new issue+wake. This occurrence is one of ~160+ open 'Wait over budget: game-dll-build at game_build' issues. No new gap: consolidated on issue-9a4503f6ed68 with a measurable target (<=1 issue/24h per signature; stall/progress-based, not raw elapsed; true attempt identity per occurrence).

6) Same-run related friction (not this occurrence): earlier waits at 09:33:39, 09:46:54, 10:36:51 logged dev-loop.wait.issue-failed because build_friction.py:244-246 inline agent-wake.py run --once exceeded timeout=30; their issues/wakes were actually created. Tracked as DEFECT F on issue-9a4503f6ed68.

ACCEPTANCE ASSESSMENT: wait_id yes; stage yes; elapsed yes; evidence present yes; reviewer wake exactly once yes; reason readable no; true build/run IDs no (now supplemented via related_build/related_run).

VERDICT: confirmed-build-wait. Genuine >60s wait during a healthy long build; not a stall and not a broken hook; defects are budget policy, stale identity, and unreadable reason, all already owned.

NEXT DISCRIMINATING CHECK: after the identity/reason fixes, compare a future wait issue's recorded generation/build_id to that run's dev-loop.game-build.started build_attempt_id and confirm evidence.reason is the readable [GAME BUILD]/stage summary. This occurrence (gen 591 / game-dll-20261124T111512931313Z-6d43ee, 405.5s, reason tail) is a clean pre-fix pair for that regression test proposed on issue-9a4503f6ed68.
