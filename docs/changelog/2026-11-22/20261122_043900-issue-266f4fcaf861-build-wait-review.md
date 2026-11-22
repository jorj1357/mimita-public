# issue-266f4fcaf861 Build-Wait Review

# Task

- Task ID: build/test wait review wake-79c8de08d15e
- Session ID: session-607d7ff88582 / work-875d55f6c91f
- Owner: build-wait-reviewer
- Focus: bounded independent review of issue-266f4fcaf861 (game-dll-build wait over 60s at stage `game_build`)
- Summary: Reviewed the exact wait occurrence against the root dev-loop journal and attempt `game-dll-20261122T041939996065Z-df61cc`. The 61.085s trip landed during a healthy object-cache-scan/pre-link window of a normal 85.547s build that then succeeded (generation 452). Recorded one evidence review (verdict `confirmed-build-friction`, review id 118) and one measured suggestion on systemic owner issue-136d4553ff92. No source edits, build, run, fix, or closure claimed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-22T04:39:00Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d595a308483a9f82c88349a5915c82a0ca

# Pre-existing changes

- The shared checkout contains pre-existing modified/untracked paths from other agents (including `src/gui/*`, `src/npc/*`, `src/network/multiplayer-tick.cpp`, `devscripts/game_build_worker.py`, many `docs/issues/*` and `docs/changelog/*` files). This session edited no source and reverted nothing.
- Files created by this session: this changelog. The append-only review and suggestion were written through `agent-coord.py issue review` and `issue suggest`; those commands appended to `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-266f4fcaf861.md` and `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`.

# Requested behavior

Review build/test wait issue issue-266f4fcaf861 without editing source, claiming another agent's files, or committing. Inspect the issue and dev-loop journal, complete any missing readiness fields, append the review and evidence, and report a needed code fix separately.

# Findings

- Wait identity: wait_id `game-dll-build:2026-11-22T04:19:39.819Z`, kind `game-dll-build`, stage `game_build`, elapsed 61.085s vs budget 60.0s, started_utc 04:19:39.819Z, build_id `2026-11-22T02:37:06.972018Z`, generation 451 (prior artifact), source_generation 1, request_ids `["auto-1"]`.
- Pinned attempt `game-dll-20261122T041939996065Z-df61cc`: started 04:19:39.996Z, success true, no_op false, generation 452, total 85.547s, worker 51.564s, changed paths `src/gui/ui-system.cpp` and `src/npc/npc-behavior-module.cpp`.
- Phase timeline: changed_source_preflight 19.967s, object_cache_scan 3.099s (ended 04:20:36.604Z), vendor 0.014s, compile 1.842s (ended 04:20:38.460Z), link 24.982s (gnu_linker 24.622s, ended 04:21:03.441Z), coherence_validation 0.627s, publish 0.801s; completed 04:21:05.554Z. The 61.085s trip at 04:20:42.393Z therefore landed after scan and compile, just before the link phase, with forward progress in every interval.
- No stall: the attempt was neither hung nor a no-op. The wait to publish after the trip was ~23s.
- Budget miscalibration: recent attempts 451=157.422s, 452=85.547s, 453=90.022s, 454=80.846s all exceed the flat `WAIT_ISSUE_SECONDS = 60` (dev-loop.py:109-110); every game-DLL build trips the wait.
- Duplicate-issue noise: uniqueness key is the exact wait_id, and a new wait_id always creates a new issue (`build_friction.py:131-140`, `:143-170`, docstring `:145-148`); the title dedupe at `:125-128` is bypassed. Identical-title issues created today include bc45be2f32a3 (04:10:48), 266f4fcaf861 (04:19:39), 6202f0e8d9f4 (04:23:20), ce4c245a8a70 (04:24:51), 628187de3d6d (04:29:08); the board showed about 28 open/in_review game-dll-build wait issues.
- Uninformative evidence: `stage` is hardcoded `game_build` (dev-loop.py:3527); `reason` is `game_build_detail` (dev-loop.py:3528), set to the last 240 chars of arbitrary worker stdout (dev-loop.py:2829-2830). This issue's reason starts mid-number (`58445.797,...`), the tail of the module-identity-provider GAME_FILE JSON, so it cannot name the active phase.
- Terminal outcome not appended: `occurrences` stays empty; the successful completion (85.547s, generation 452) is never appended for the same wait_id.
- Wake correctness: exactly one `dev-loop.wait.over_budget` and one `dev-loop.wait.reviewer-wake` exist for this wait_id, `created=true`, wake-79c8de08d15e targets this issue. Across today's journal, over_budget=24 and reviewer-wake=24 with identical wait_ids (1:1, no duplicates).
- Cross-check: sibling review of bc45be2f32a3 (review id 116, `not-wedged-policy-miscalibrated`) independently reached the same policy-miscalibration and evidence-attribution conclusions for attempt 451.

# Specification alignment

- Followed `AGENTS.md` gates 0/0b/0c/0d plus evidence rules and `docs/workflows/issue-intake-and-triage.md`. The change-to-evidence-latency spec expects measured per-class budgets, truthful per-stage accounting, and later observations appended to the same wait record. A budget or dedupe policy change needs human/owner approval, so this review records the mismatch instead of editing the shared build path.

# Exact implementation changes

- None to source. Append-only issue review (id 118, verdict `confirmed-build-friction`) and one `issue suggest` measured occurrence on issue-136d4553ff92.

# Validation

- Build status: no build requested; no compiler, module worker, or second dev-loop was invoked.
- Evidence read: `logs/2026-11-22/20261122_011031/dev-loop/events.jsonl`, `.dev/build-timings/game-dll-20261122T041939996065Z-df61cc.json`/`.jsonl`, attempt 451/453/454 summaries, `devscripts/dev-loop.py` (`WAIT_ISSUE_SECONDS`, `check_wait_budget`, `_record_game_build_output`), `devscripts/build_friction.py`, and the build-friction issue list.
- Review artifact: issue-266f4fcaf861 review id 118; state now `in_review`; reviewers `[build-wait-reviewer]`; readiness 8/8.

# Measured evidence

- Recorded wait: elapsed 61.085s, budget 60.0s (over by 1.085s); issue created 04:20:40.933Z; wake recorded 04:20:42.392Z.
- Build attempt: generation 452, success, total 85.547s; changed_source_preflight 19.967s, link 24.982s, publish 0.801s.
- Distribution over four consecutive attempts: 157.422s, 85.547s, 90.022s, 80.846s — 0/4 under the 60s budget.
- Journal ratio for run 20261122_011031: 24 over_budget events and 24 reviewer wakes, one per wait_id.

# Regression review

- No regression entry appended. The wait correctly detected a >60s window on a successful build; the recurring cause is flat-budget policy, per-wait-id issue creation, and truncated reason text, not a broken DLL build.

# Human acceptance

- Not applicable (build/test wait review). Whether to derive the budget from measured attempt durations, populate stage/reason from live build-timings phases, and collapse recurring identical waits into occurrences while waking once per occurrence remains a human/owner decision.

# Agent handoff context

- Owner: unassigned; systemic build-stage/latency owner is `build-coordinator` on issue-136d4553ff92 (task `dll-build-stage-accounting`). Related umbrella issues: issue-e044a931c54b (remove avoidable waits) and issue-7d3dcfd7433b (measure wait stages).
- Next exact step: same recommendation as sibling reviews — replace the flat 60s clock with a measured/class-aware or forward-progress-gated budget, derive stage/reason from live phase records, and append the terminal attempt result as an occurrence on the same wait record instead of creating a new issue per build.
- Do not redo: this wait-id, wake, build-attempt, and journal review; do not spawn another duplicate wait issue for the same class.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.
