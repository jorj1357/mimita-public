# issue-ce4c245a8a70 Build-Wait Review

# Task

- Task ID: build/test wait review wake-d8333c8c5780
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-ce4c245a8a70 (game-dll-build wait over 60s at stage `game_build`)
- Summary: Verified the exact wait occurrence against the root dev-loop journal and pinned attempt `game-dll-20261122T042451390437Z-634baf`. The 60.214s trip landed during the compile tail of a healthy 81.407s build that then succeeded (generation 454). Recorded two evidence reviews (verdict `confirmed`, review ids 120 and 121) and one measured suggestion on systemic owner issue-136d4553ff92. No source edits, build, run, fix, or closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T04:45:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d595a308483a9f82c88349a5915c82a0ca

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents (including `src/gui/*`, `src/npc/*`, `src/network/multiplayer-tick.cpp`, `devscripts/game_build_worker.py`, and many `docs/issues/*` and `docs/changelog/*` files). This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only reviews and suggestion were written through `agent-coord.py issue review` and `issue suggest`; they appended to `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ce4c245a8a70.md` and `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.

# Requested behavior

Review build/test wait issue issue-ce4c245a8a70 without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete any missing readiness fields, append the review and evidence, and report a needed code fix separately.

# Findings

- Wait identity: wait_id `game-dll-build:2026-11-22T04:24:51.316Z`, kind `game-dll-build`, stage `game_build`, elapsed 60.214s vs budget 60.0s, started_utc 04:24:51.316Z, build_id `2026-11-22T02:37:06.972018Z`, generation 453 (prior artifact), source_generation 5, request_ids `["auto-5", "2026-11-22T04:25:14.288Z-fd08a724"]`.
- Journal proof: `dev-loop.wait.over_budget` event DEV_RUNTIME_8af536cf09514692b65918e20d11a0a6 at 04:25:52.749Z (`created=true`), and `dev-loop.wait.reviewer-wake` event DEV_RUNTIME_de5b274cf9374041801c405295707db4 (`queued=true`, wake-d8333c8c5780). One issue and one queued wake exist for this wait occurrence.
- Pinned attempt `game-dll-20261122T042451390437Z-634baf`: success true, no_op false, generation 454, started 04:24:51.390Z, completed 04:26:12.809Z, total 81.407s, worker 64.128s.
- Changed paths: `src/gui/ui-system.cpp`, `src/gui/ui-system-buttons.cpp`, `src/gui/ui-text-input.cpp`, `src/gui/ui-system-internal.h`; the header change compiled 26 TUs and skipped 617, with 639 dependency checks (avg 0.35 ms).
- Phase timeline (ms): changed_source_preflight 10785.963, object_cache_scan 2184.191, compile 40555.912 (slowest TU `online-menu.cpp` 17313.198), link 9589.146, coherence_validation 301.007, publish 550.858.
- Not wedged: the 60.214s trip occurred during the compile tail (last file event `online-menu.cpp` at 04:25:53.649Z), about 20s before successful completion; every interval showed forward progress.
- Budget miscalibration: `WAIT_ISSUE_SECONDS` defaults to 60s (dev-loop.py:109-110). Two consecutive builds both exceed it: this header-change build 81.407s, and generation 455 (only `src/main-systems.cpp`) 67.796s. Every routine game-DLL build trips the wait.
- Reason-attribution defect: the recorded reason is `game_build_detail` (dev-loop.py:3528), set to `line.rstrip()[-240:]` of arbitrary worker stdout (dev-loop.py:2830, :2860). This issue's reason is a truncated tail of a `[GAME_STAGE]` JSON record (`:57590.996,"build_attempt_id":...`) rather than a readable stage/reason. Acceptance requires the issue to carry `reason`.
- Duplicate-issue noise: uniqueness is per exact wait_id (`build_friction.py:131-170`), so identical titles are created per build and bypass the title dedupe at `:125-128`. This is consistent with the issue's own risk note.
- Duplicate reviewer activation: `.dev/agent-wake/events.jsonl` shows for wake-d8333c8c5780 one `wake.queued` (04:25:52.201Z), then `wake.launched` codex pid 26064 (04:25:52.726Z), `wake.retry` adapter=opencode (04:30:11.230Z), and a second `wake.launched` opencode pid 10364 (04:37:04.388Z). Queue-once holds, but one wake produced two reviewer sessions.

# Specification alignment

- Followed `AGENTS.md` gates 0/0b/0c/0d plus evidence rules and `docs/workflows/issue-intake-and-triage.md`. A budget, dedupe, or reason-format change needs human/owner approval, so this review records the mismatch instead of editing the shared build path.

# Exact implementation changes

- None to source. Append-only issue reviews (ids 120 and 121, verdict `confirmed`) and one `issue suggest` measured occurrence on issue-136d4553ff92.

# Validation

- Build status: no build requested; no compiler, module worker, or second dev-loop was invoked.
- Evidence read: `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261122T042451390437Z-634baf.json`, `.dev/agent-wake/events.jsonl`, `.dev/agent-wake/requests/wake-d8333c8c5780.json`, `.dev` build state current/ready, `devscripts/dev-loop.py` (`WAIT_ISSUE_SECONDS`, `check_wait_budget`, `_record_game_build_output`), and `devscripts/build_friction.py`.
- Review artifact: issue-ce4c245a8a70 now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8.

# Measured evidence

- Recorded wait: elapsed 60.214s, budget 60.0s (over by 0.214s); issue created 04:25:51.585Z; over_budget recorded 04:25:52.749Z.
- Pinned attempt: generation 454, success, total 81.407s, worker 64.128s, compile 40.556s, link 9.589s.
- Next build generation 455: total 67.796s for `src/main-systems.cpp` only; also over 60s.
- Wake lifecycle: queued once, launched twice (codex 04:25:52Z, opencode retry 04:37:04Z).

# Regression review

- No regression entry appended. The wait correctly detected a >60s window on a successful build; the recurring cause is flat-budget policy, per-wait-id issue creation, truncated reason text, and retry duplicating the reviewer session, not a broken DLL build.

# Human acceptance

- Not applicable (build/test wait review). Whether to gate the budget on measured attempt durations/forward progress, derive stage/reason from live build-timings phases, collapse recurring identical waits into occurrences, and prevent a retried wake from starting a second concurrent reviewer remains a human/owner decision.

# Agent handoff context

- Owner: unassigned; systemic build-stage/latency owner is `build-coordinator` on issue-136d4553ff92 (task `dll-build-stage-accounting`). Related umbrella issues: issue-e044a931c54b (remove avoidable waits) and issue-7d3dcfd7433b (measure wait stages).
- Next exact step: replace the flat 60s clock with a measured/class-aware or forward-progress-gated budget, derive stage/reason from live phase records, append the terminal attempt result as an occurrence on the same wait record, and ensure a wake retry reuses one reviewer session.
- Do not redo: this wait-id, wake, build-attempt, and journal review; do not spawn another duplicate wait issue for the same class.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue reviews and suggestion.
