# Bounded build-wait review of issue-1f0c919bdbaa

- Task ID: wake-6008dd7db0f2
- Session ID: session-d9fa5a7f0c03 / work-a1c8999d746b
- Owner: build-wait-reviewer
- Focus: one read-only review of the over-budget `game-dll-build` wait `game-dll-build:2026-11-23T22:09:45.196Z`, plus occurrence handoffs for the known flat-budget/identity and late-misattributed-wake defects
- Summary: Appended review (verdict `confirmed-not-a-stall;flat-budget-false-positive;stale-identity;late-misattributed-wake`) to `issue-1f0c919bdbaa`; one fresh identity pair plus flat-budget recurrence to `issue-9a4503f6ed68`; one fresh late-dispatch plus misattribution occurrence to `issue-84c584957e65`; one evidence message to `build-coordinator`. No source edit, build request, fix, or close.
- Status: complete (review only)
- Date, time, timezone: 2026-11-23T22:45Z, ISO 8601
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `5e08e1054f8293ba0db96983d482e3bf56938d08`
- Final commit: none (no commit made)

# Pre-existing changes

- `git status --short` showed many paths already modified or untracked by other agents/sessions (`AGENTS.md`, `config/*`, `devscripts/dev_loop/*`, `docs/ROUTER.md`, `docs/issues/*`, `docs/changelog/*`). None were created or edited by this session beyond the records listed below.
- This session wrote only: `docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-1f0c919bdbaa.md` (review append via `agent-coord`, review id 304), `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` (occurrence append), `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` (occurrence append), the coordination store `.dev/agent-coord/brain.db`, and this changelog.

# Requested behavior

Review build/test wait issue `issue-1f0c919bdbaa` (wait `game-dll-build:2026-11-23T22:09:45.196Z`, elapsed 60.745s), inspect the issue and dev-loop journal, complete any missing reproduction/expected/acceptance/paths/proof fields, and append the review and evidence. If the wait recurs, add the occurrence rather than a duplicate. If a code fix is needed, report it with evidence and claim it separately.

# Specification alignment

- `docs/specs/dev-loop/agent-coordination.md`: review findings are appended with `issue review`; waiting/review work never edits a path owned by another agent; duplicate reports are added to the existing owner.
- `docs/workflows/issue-intake-and-triage.md`: append-only reviews, preserve prior text, no close without proof.
- AGENTS.md 0b/0c/0d: no compiler, no build request, no second dev-loop, no source edit; separate observation, inference, and unknowns; prefer the existing generic owner for duplicate findings.

# Exact implementation changes

None. No source, specification, or configuration change was made.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-1f0c919bdbaa.md` | issue created by dev-loop wait hook | + reviewer review (id 304), state `in_review` | required wake review record | remove only the appended review; do not rewrite prior text |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` | prior identity/flat-budget occurrences | + 1 occurrence (wait gen 572 vs in-flight gen 573) | fresh identity pair for the open next check | remove only the appended occurrence |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` | prior late-dispatch occurrences | + 1 occurrence (wake-6008dd7db0f2, 580.7s, misattributed launch result) | recurring late-dispatch defect | remove only the appended occurrence |
| `.dev/agent-coord/brain.db` | issue rows | +1 review, +2 occurrences, +1 message | coordination record | restore from backup if withdrawn |

# Contract / ABI changes

None.

# State / schema changes

None.

# Diagnostics

- Read-only evidence from `logs/2026-11-23/20261123_192224/dev-loop/events.jsonl` (lines 645-647, 723-724), `.dev/build-timings/game-dll-20261123T220945368970Z-09cabb.json`, `build/game/current.json`, `.dev/agent-wake/events.jsonl` (lines 2380-2391), `.dev/agent-wake/requests/wake-6008dd7db0f2.json`, and `.dev/agent-coord/brain.db` (`issue show/readiness`).
- No new diagnostic code or files were added.

# Validation

- `python devscripts/agent-coord.py begin/sync/observe` executed; readiness returned `ready=true, missing=[], score=8`.
- `store.issue_review(...)` appended review id 304 and moved the issue to `in_review`.
- `store.issue_occurrence(...)` appended occurrences to `issue-9a4503f6ed68` and `issue-84c584957e65`.
- `python devscripts/agent-coord.py send --issue issue-1f0c919bdbaa ...` delivered the finding summary to `build-coordinator` (msg-c68ad70f66f2).
- No compiler, build request, module worker, second dev-loop, or test run was invoked.

# Findings

1. Acceptance met for this occurrence: journal `logs/2026-11-23/20261123_192224/dev-loop/events.jsonl` line 647 has exactly one `dev-loop.wait.over_budget` (created=true) for this wait_id, and exactly one wake request `wake-6008dd7db0f2` exists (created 22:10:47.979Z). A scan of all build-friction issues found exactly one issue carrying this wait_id.
2. The waited-on build did not hang: attempt `game-dll-20261123T220945368970Z-09cabb` succeeded as generation 573 at 845.325s total (compile 772.452s, link 50.334s, 72 compiled / 571 skipped), published 22:23:53Z. It was a 3-header high-fanout rebuild (`aimbody-config.h`=12, `gamemode.h`=51, `post-fx.h`=20) with queue waits up to ~592s across 10 jobs. The flat 60s total-elapsed budget fired on a healthy build, so this is a policy false positive, not a stalled worker.
3. Stale identity: wait evidence records `generation=572` / `build_id=2026-11-23T19:22:15.804284Z` (previously published) while the in-flight attempt was generation 573; the true attempt id lives only in the `reason` text. Same defect tracked on `issue-9a4503f6ed68`.
4. Reviewer wake was late and misattributed: `wake-6008dd7db0f2` queued 22:10:47.979Z, first codex claim/launch 22:10:51.040Z/22:10:51.622Z, `wake.retry` 22:20:20.510Z, opencode claim/launch 22:20:28.658Z/22:20:28.690Z — 580.7s (~9m41s) queue-to-launch. The `dev-loop.wait.reviewer-wake` event (line 646) is stamped with this wait/issue but embeds the unrelated `wake-ba365e6b1b58` (issue-review-sweeper, issue-87d8c9c7ffb1), because `build_friction.wake_reviewer` synchronously drains whichever pending wake `agent-wake.py run --once` claims first. Same defect tracked on `issue-84c584957e65`.
5. `reason` was clean this occurrence (`[GAME BUILD] revision=... sources=642 jobs=10 ...`), so the truncated-JSON reason defect (`issue-6e08d89b55e3`) did not recur.
6. Backlog context: 142 open issues titled `Wait over budget: game-dll-build at game_build` (165 open `Wait over budget*`) exist; per-wait_id dedupe intentionally mints one issue per attempt.

# Confidence

High on the build result, the single-issue/single-wake acceptance, the stale identity, and the event misattribution (direct journal, `current.json`, and wake request/events). Medium on the exact cause of the ~569s codex-adapter gap (not visible in this journal slice; the wake still ran).

# Proof

No proof claim. This is a tooling/observability review; the wait evidence is journal-tier, not a behavior proof.
