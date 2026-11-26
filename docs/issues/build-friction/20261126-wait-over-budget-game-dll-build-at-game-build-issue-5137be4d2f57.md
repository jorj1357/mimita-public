# issue-5137be4d2f57: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-live-with-open-defects)

### 2026-11-26T04:14:53.085Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Read-only review of this over-budget wait occurrence. No source edited, no build started, no issue closed.

OBSERVED FACTS
1. The wait record is real. dev-loop.wait.over_budget at 2026-11-26T02:30:50.237Z (journal logs/2026-11-26/20261126_022514/dev-loop/events.jsonl line 61) has wait_id game-dll-build:2026-11-26T02:29:43.891Z, kind game-dll-build, stage game_build, elapsed 60.464s / budget 60.0s, started_utc 2026-11-26T02:29:43.891Z.
2. The flagged build genuinely exceeded the budget and was healthy/progressing, not stalled. In-flight attempt game-dll-20261126T022944049233Z-822054 started 02:29:44.049Z, terminal result 02:31:16.956Z (line 64), ready 02:31:19.540Z (line 65): generation 665, source_generation 8, status ok, total 94.875s, compile 52.391s, link 19.922s, compiled 7 / skipped 638 (.dev/build-timings/game-dll-20261126T022944049233Z-822054.json). Host CPU mean 97.36% / peak 100%, minimum available memory 88,260,608 bytes (~84 MiB) during the build.
3. Exactly one reviewer wake was queued for this wait occurrence: wake-bdaae328c6d8 (wake.queued 2026-11-26T02:30:46.325Z); journal has exactly one dev-loop.wait.reviewer-wake (line 60) and one dev-loop.wait.over_budget (line 61), and only one .dev/agent-wake/requests file references this issue. This part of the acceptance holds.

ACCEPTANCE GAPS (this issue does not fully satisfy its own acceptance)
A. Build/run identity is wrong or absent. Evidence records build_id=2026-11-26T01:00:35.913746Z (the previously published build), generation=null and no run_id, while the in-flight attempt is game-dll-20261126T022944049233Z-822054 / generation 665 / run 20261126_022514. The true attempt id and generation appear only inside the free-text reason. Source: devscripts/dev-loop.py check_wait_budget uses self.latest_build (line 3896) and self.game_build_generation (line 3897, assigned only on completion at line 3259); the wait dict (lines 3888-3900) carries no build_attempt_id or run_id. This violates the acceptance phrase "build/run IDs".
B. reason is not human-readable. Recorded reason is a mid-JSON tail starting `59297.0,"build_attempt_id":...`. Source: devscripts/dev-loop.py:3112 sets detail = line.rstrip()[-240:] for every worker output line; that value becomes game_build_detail (line 3142) and then the wait reason (line 3892). reason is present but low quality; tracked under issue-6e08d89b55e3.
C. Reviewer activation was ~92 minutes late. wake-bdaae328c6d8: queued 02:30:46, claimed+launched 02:30:50, then wake.retry 04:01:56 and claimed+launched 04:02:45 (this session), i.e. the first launch produced no review and the working review arrived ~5520s after the wait. This does not break "queued exactly once" but breaks the practical intent of "wakes one reviewer automatically"; tracked under issue-84c584957e65 (codex primary adapter failure with no daemon drain).

INFERENCE / CONFIDENCE
- High confidence on facts (dev-loop journal, build-timing summary, .dev/agent-wake events and request file all agree).
- The automation hook (detect -> one issue -> one wake -> durable journal events) is functioning. The defects are evidence quality (identity, reason) and wake dispatch latency, not an inert hook.
- This is an intrinsic incremental whole-game DLL cost occurrence (94.875s for 7 TUs due to gamepad-input.h fanout), so the flat 60s budget also mis-classifies a healthy build; that policy question stays on umbrella issue-9a4503f6ed68.

NEXT DISCRIMINATING CHECK
- After the identity fix lands, a fresh over-budget game-dll-build wait must record build_id/build_attempt_id/generation/run_id equal to that run's dev-loop.game-build.started attempt and result, and reason must be the readable [GAME BUILD] summary. Re-run this check before claiming acceptance.

No duplicate issue filed. Fresh occurrence appended to umbrella issue-9a4503f6ed68; wake-latency data point cross-linked to issue-84c584957e65; reason defect cross-linked to issue-6e08d89b55e3.
