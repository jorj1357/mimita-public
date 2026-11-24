# Bounded build-wait review of issue-b1744402b0a5

- Task ID: wake-25a9ef1b3f2a
- Session ID: session-2dbd2b34bc98 / work-965a374509c1
- Owner: build-wait-reviewer
- Focus: one read-only review of the over-budget `game-dll-build` wait `game-dll-build:2026-11-23T23:43:01.179Z`, plus occurrence handoffs for the known stale-identity and late/misattributed-wake defects
- Summary: Appended review (verdict `confirmed-failed-build-not-stall;stale-identity;late-misattributed-wake`) to `issue-b1744402b0a5`; one fresh identity/flat-budget/failing-build occurrence to `issue-9a4503f6ed68`; one fresh late-dispatch plus misattribution occurrence to `issue-84c584957e65`; one evidence message to `build-coordinator` (msg-36ac8fb14be7). No source edit, build request, fix, or close.
- Status: complete (review only)
- Date, time, timezone: 2026-11-24T00:24Z, ISO 8601
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `186eebd23ce5f8f3f10377c47277ed58bd3b4d83`
- Final commit: none (no commit made)

# Pre-existing changes

- `git status --porcelain` showed 19 paths already modified or untracked by other agents/sessions: `config/accounts/default.json`, `config/analytics.json`, `devscripts/blender-ai`, `docs/features/npc-behavior.md`, `docs/issues/README.md`, several `docs/issues/*` docs, `src/config/movement-config.cpp`, `src/entities/player.h`, `src/network/server-players.cpp`, `src/physics/movement/*`, an untracked `docs/changelog/2026-11-24/` directory, and `docs/issues/bug/20261124-coalesce-delayed-damage-...`. None were created or edited by this session beyond the records listed below.
- This session wrote only: `docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md` (review append via `agent-coord`), `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` (occurrence append), `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` (occurrence append), the coordination store `.dev/agent-coord/brain.db`, and this changelog.

# Requested behavior

Review build/test wait issue `issue-b1744402b0a5` (wait `game-dll-build:2026-11-23T23:43:01.179Z`, elapsed 61.338s), inspect the issue and dev-loop journal, complete any missing reproduction/expected/acceptance/paths/proof fields, and append the review and evidence. If the wait recurs, add the occurrence rather than a duplicate. If a code fix is needed, report it with evidence and claim it separately.

# Specification alignment

- `docs/specs/dev-loop/agent-coordination.md`: review findings are appended with `issue review`; waiting/review work never edits a path owned by another agent; duplicate reports are added to the existing owner.
- `docs/workflows/issue-intake-and-triage.md`: append-only reviews, preserve prior text, no close without proof.
- AGENTS.md 0b/0c/0d: no compiler, no build request, no second dev-loop, no source edit; separate observation, inference, and unknowns; prefer the existing generic owner for duplicate findings.

# Exact implementation changes

None. No source, specification, or configuration change was made.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/build-friction/20261123-wait-over-budget-game-dll-build-at-game-build-issue-b1744402b0a5.md` | issue created by dev-loop wait hook | + reviewer review, state `in_review` | required wake review record | remove only the appended review; do not rewrite prior text |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` | prior identity/flat-budget occurrences | + 1 occurrence (wait gen 574/`2026-11-23T19:22:15.804284Z` vs failed in-flight attempt `6c7a9c`) | fresh identity pair plus a failing-build case | remove only the appended occurrence |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` | prior late-dispatch occurrences | + 1 occurrence (wake-25a9ef1b3f2a, 1031.5s; misattributed `wake-30a0f17c872c` payload) | recurring late-dispatch and wake-misattribution defect | remove only the appended occurrence |
| `.dev/agent-coord/brain.db` | issue rows | +1 review, +2 occurrences, +1 message | coordination record | restore from backup if withdrawn |

# Contract / ABI changes

None.

# State / schema changes

None.

# Diagnostics

- Read-only evidence from `logs/2026-11-23/20261123_192224/dev-loop/events.jsonl` (lines 1476, 1487, 1488, 1517), `.dev/agent-wake/events.jsonl` (lines 2397-2412), `.dev/agent-wake/requests/wake-25a9ef1b3f2a.json`, `build/game/current.json`, and `.dev/agent-coord/brain.db` (`issue show`/counts).
- No new diagnostic code or files were added.

# Validation

- `python devscripts/agent-coord.py begin/sync/observe` executed; readiness returned `ready=true, missing=[], score=8`.
- `store.issue_review(...)` appended the review and moved `issue-b1744402b0a5` to `in_review` (1 review, verdict `confirmed-failed-build-not-stall;stale-identity;late-misattributed-wake`).
- `store.issue_occurrence(...)` appended occurrences to `issue-9a4503f6ed68` and `issue-84c584957e65`.
- `python devscripts/agent-coord.py send --issue issue-b1744402b0a5 ... build-coordinator` delivered the finding summary (msg-36ac8fb14be7).
- `python devscripts/agent-coord.py activity set ...` recorded the completed review phase.
- No compiler, build request, module worker, second dev-loop, or test run was invoked.

# Findings

1. Acceptance for this occurrence: run `logs/2026-11-23/20261123_192224/dev-loop/events.jsonl` line 1488 has exactly one `dev-loop.wait.over_budget` (created=true) for this wait_id, and line 1487 has exactly one `dev-loop.wait.reviewer-wake` for the same wait; a wait_id scan returns only those two rows, and `record_wait_incident` keys on wait_id, so no duplicate issue exists.
2. The waited-on build was real and **failed**: attempt `game-dll-20261123T234301570308Z-6c7a9c` (source_generation 49, revision `3b206b93...`) started 23:43:26.977Z and reached `dev-loop.game-build.result` status=`failed`, phase=`changed_source_preflight`, `"changed-source preflight failed: src/network/multiplayer-projectiles.cpp in 157.347s"` at 23:46:11.624Z.
3. Flat-budget timing: the 60s budget crossed at ~23:44:02.5 (1.338s over) while the attempt was still in preflight; ~25.8s of the 61.3s elapsed before `game-build.started`. Unlike the healthy 22:09/22:57 rebuilds in the same run, this occurrence coincided with a genuinely failing compile, so it is not a flat-budget false positive on a healthy build, though a flat total-elapsed threshold still cannot distinguish slow progress from a stall.
4. Stale identity: wait evidence records `generation=574` / `build_id=2026-11-23T19:22:15.804284Z` (previous published EXE) rather than the in-flight attempt id `game-dll-20261123T234301570308Z-6c7a9c`; `dev-loop.py:3878-3879` use `self.latest_build` and `self.game_build_generation`, so the acceptance requirement to carry build/run IDs is only partially met.
5. Late and misattributed reviewer wake: the sole reviewer-wake event is stamped with this wait_id/issue but embeds `wake-30a0f17c872c` (issue-e095a23117c8, wait `22:57:01.813Z`). The actual `wake-25a9ef1b3f2a` queued 23:44:07.312Z, codex claim/launch 23:44:14.577Z/14.599Z, `wake.retry` "stale launched request recovered by scheduler" to opencode 23:46:51.521Z, opencode claim/launch 2026-11-24T00:01:18.789Z/18.802Z — queue-to-final-launch 1031.49s (~17m11.5s). Mechanism: `build_friction.py:242-252` drains the oldest pending wake via `agent-wake.py run --once` and `dev-loop.py:3895-3902` attaches its result to the current wait.
6. Recurrence/backlog: this is the fifth over-budget game-dll-build wait in run `20261123_192224` (20:09:24, 21:51:15, 22:09:45, 22:57:01, 23:43:01). Current snapshot: 144 open `Wait over budget: game-dll-build at game_build`, 167 open `Wait over budget*`, 180 open build-friction, 319 open issues total.

# Confidence

High on the failed build outcome, the single-issue/single-wake acceptance, the stale identity, and the event misattribution (direct journal, wake request/events, `current.json`). Medium on why the codex claim/launch at 23:44:14.599Z did not yield a live reviewer before the 23:46:51 retry; delivery ultimately occurred.

# Proof

No proof claim. This is a tooling/observability review; the wait evidence is journal-tier, not a behavior proof.
