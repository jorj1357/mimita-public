# issue-f36679f4ba95 Build-Wait Review

# Task

- Task ID: review build/test wait issue-f36679f4ba95 (wake-f30a76fe5a01)
- Owner: build-wait-reviewer
- Focus: game-dll-build wait at game_build, elapsed 61.22s, wait_id `game-dll-build:2026-11-27T03:46:49.521Z`
- Summary: Verified the wait against in-flight attempt `game-dll-20261127T034649716100Z-80c771` (gen 759), its build-timings/resource sidecar, the run-20261127_032430 dev-loop journal, and the wake queue. Appended review (verdict `confirmed-build-wait`) and set the issue to in_review. Appended the truncated-reason + stale-identity occurrence to issue-6e08d89b55e3 and the 03:49-03:51 build-resource-sampling gap keys to issue-136d4553ff92. No source, build, test, or runtime was started by this agent.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-27T04:11Z
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 7176d28a

# Pre-existing changes

- The shared checkout already contained other agents' uncommitted config, devscript, source, and docs edits. This session edited no source and reverted nothing.
- Files created by this session: this changelog. Append-only issue records were written through `agent-coord.py issue review/suggest`.

# Requested behavior

Review the over-budget game-DLL wait, inspect the issue and dev-loop journal, complete any missing issue fields, and append the review and evidence. If the wait recurs, add the occurrence instead of creating a duplicate issue. Do not edit code or commit without human approval.

# Findings

- The wait was real, in-flight work, not a stall or false positive. Wait `game-dll-build:2026-11-27T03:46:49.521Z` (61.22s vs 60.0s budget, request `2026-11-27T03:46:49.303Z-5f223a6c:mode3:game`) maps to attempt `game-dll-20261127T034649716100Z-80c771` (revision `26a2b63e...`, source_generation 5). It started 03:46:49.716Z and terminated 03:48:43.676Z (113.953s) with success=false, failed_phase=link.
- First divergence for the wait is the changed-source compile, not object_cache_scan. Phase durations: changed_source_preflight 16344ms, compile 76672ms, object_cache_scan 2500ms, link 6047ms. 16 TUs compiled despite only `src/input/gamepad-input.cpp` changing; compile_sum 464684ms over jobs=10; slowest TU `src/engine/engine-tick-camera.cpp` 47.368s. Compile wall alone (76.7s) exceeds 60s, and preflight (16.3s) + slowest TU (47.4s) = 63.7s, so the wait was unavoidable once that TU set was chosen.
- Why 16 TUs from a one-file change: cache_invalidations shows 2x "object file is empty" (engine-tick-camera.cpp, multiplayer-physical-entities.cpp), 10x "dependency newer than object: src/vehicles/vehicle-types.h", and 1x "missing or invalid dependency file" (vehicle-system.cpp). 15 of 16 recompiles were not caused by the changed source.
- The link then failed with undefined symbols whose definitions exist in the tree (effectiveGameplayMouseButton input-poll.cpp:170; CommunityMatchClient selectFighterWeapon/fighterWeaponChoiceVisible/Committed/Id community-match-client.cpp:89-115; consumeScheduledTerminalMoveForNetwork/peekTerminalInteract/buildInputFrame input-poll.cpp:126/206/396; MapEditor::renderWorld). This is the known `docs/regressions/dev-loop/game-dll-link-loses-translation-units-REG-2026-11-27.md` cache/link-list regression; sibling attempts 034911, 034952, 035109 each failed on different symbols, confirming the cascade. The wait is downstream of that regression, not a new cause.
- Acceptance partially met: wait_id, stage, elapsed, evidence, and exactly one wake are present; but `reason` is a mid-JSON tail (known defect issue-6e08d89b55e3) and `build_id` is stale (`2026-11-27T01:43:27.658897Z`) with `generation=null`, not the actual attempt/gen 759.
- Exactly one reviewer wake: wake-f30a76fe5a01 created 03:47:54.778Z; queued/launched via opencode at 03:58:59.193/699Z (attempts=2). Queue-to-launch 664.92s (~11m5s); one issue and one wake for this wait_id (no duplicate queue).
- Resource sidecar `game-dll-20261127T034649716100Z-80c771.resource.json`: 86 samples @1s over 113.937s, mean host CPU 96.796%, peak 100%, minimum available memory 477,691,904 B (~455.6 MiB); runtime_alignment unavailable. Pressure observed, NOT proven as cause; the matching baseline/commit-charge/journal-alignment gaps are unknown, not zero.

# Specification alignment

- Followed AGENTS.md gates 0b/0c/4: read coordination state, kept build evidence separate from behavior proof, made no source/build change, and did not create a duplicate issue for the recurring wait class.
- Followed `docs/specs/dev-loop/change-to-evidence-latency.md`: the wait ID is the dedupe key, the exact-wait issue was reused, later observations were appended to the owning defect issues, and one reviewer wake was queued.

# Exact implementation changes

- None. Append-only issue records and this changelog only.

# Validation

- Build status: no build requested; no compiler, worker, dev-loop build request, or second loop invoked by this agent.
- Commands used: `agent-coord.py begin/sync/observe/activity set/issue review/issue suggest`, and read-only inspection of `.dev/build-timings/game-dll-20261127T034649716100Z-80c771.{json,resource.json}`, `.dev/build-timings/game-dll-20261127T034911750868Z-9e32c5.json`, `...034952585428Z-210073.json`, `...035109360100Z-487957.json`, `build/game/current.json`, `build/game/last-attempt.json`, `logs/2026-11-27/20261127_032430/dev-loop/events.jsonl` lines 65-96, `.dev/agent-wake/requests/wake-f30a76fe5a01.json`, and `docs/regressions/dev-loop/game-dll-link-loses-translation-units-REG-2026-11-27.md`.
- Review artifact: issue-f36679f4ba95 review (state in_review, reviewer build-wait-reviewer).

# Measured evidence

- Wait elapsed 61.22s vs 60.0s budget; attempt started 03:46:49.716Z; terminal failure 03:48:43.676Z (total 113.953s).
- Phase timings: changed_source_preflight 16.344s, compile 76.672s (16 TUs, max TU 47.368s, sum 464.684s, jobs 10), object_cache_scan 2.500s, link 6.047s.
- over_budget + reviewer-wake events at 2026-11-27T03:48:13.916/.915Z (journal lines 92/91); one issue, one wake.
- Resource: 86 samples, mean host CPU 96.796%, peak 100%, min available memory 477,691,904 B.
- Wake: created 03:47:54.778Z, launched 03:58:59.699Z, queue-to-launch 664.92s, attempts=2.

# Regression review

- No new regression entry appended by this session. The occurrence is attributed to the existing `game-dll-link-loses-translation-units-REG-2026-11-27.md` and the wait-record defects already tracked on issue-6e08d89b55e3 and issue-9a4503f6ed68.

# Human acceptance

- Visual review: not performed.
- Gameplay review: not applicable.
- Still unverified: whether the human-authorized object-cache quarantine + full rebuild completed and published (last-attempt at review time was a gen-763 link failure); the owner decision on content-hash invalidation; and the wait-record reason/build_id fix.

# Agent handoff context

- Wait policy/budget/duplicate owner: issue-7d3dcfd7433b, issue-136d4553ff92, issue-e044a931c54b.
- Wait-record stale identity owner: issue-9a4503f6ed68 (identity occurrences) and issue-7d3dcfd7433b.
- Reason-truncation owner: issue-6e08d89b55e3 (occurrence appended with this wait).
- Wake-delivery owner: issue-84c584957e65 (dispatch late 664.92s this occurrence; not filed as acceptance failure).
- Link-loses-TU regression: `docs/regressions/dev-loop/game-dll-link-loses-translation-units-REG-2026-11-27.md`; related active work issue-70b27f66e00e.
- Resource-pressure owner: issue-136d4553ff92 (sibling gap keys appended) / issue-2a5c5da53565.
- Next exact step: confirm the quarantine+full-rebuild published a generation whose `changed_sources` contains the input work, then re-triage further wait issues against the resolved cache state. Shared build changes need human approval.
- Do not redo: this attempt's evidence collection.
- Do not close as a code defect without owner/human review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue records.
