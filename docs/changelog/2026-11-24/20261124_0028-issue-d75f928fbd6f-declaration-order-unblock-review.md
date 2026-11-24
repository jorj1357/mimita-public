# Bounded review of issue-d75f928fbd6f declaration-order build block

- Task ID: wake-9087ebb7d06e
- Session ID: session-31566e0a93d2 / work-0ac7c018e495
- Owner: issue-review-sweeper
- Focus: high-priority user context `ctx-db97843045e4` ("open an issue to figure out why this failed, the owner is not responding, but just do all that stuff like it's fine") and its linked issue `issue-d75f928fbd6f`
- Summary: Confirmed the root cause (use-before-declaration), confirmed the path-owner `melee-parity` non-response is distinct from the issue owner `npc-elite-fcs-5`, recorded that the minimal forward-declaration fix compiled in shared build `game-dll-20261124T003901444604Z-3c8d4e` and that the batch published generation 576 with `exit_code=0`. Issue kept assigned with runtime proof still pending. No source edit, no build request, no close.
- Status: complete (review only)
- Date, time, timezone: 2026-11-24T00:56Z, ISO 8601
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `186eebd23ce5f8f3f10377c47277ed58bd3b4d83`
- Final commit: none (no commit made)

# Pre-existing changes

- `git status --porcelain` already showed a large working set owned by other agents (movement/input/network source, several `docs/issues/*` docs, an untracked `docs/changelog/2026-11-24/` directory, and the untracked `docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md`). None were created or edited by this session beyond the records listed below.
- The forward declaration at `src/network/multiplayer-projectiles.cpp:438` was authored by `npc-elite-fcs-5` (build note `d57339951817`), not by this session.
- This session wrote only: `docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md` (evidence update and two appended suggestions via `agent-coord`), the coordination store `.dev/agent-coord/brain.db`, and this changelog.

# Requested behavior

Route the high-priority user context and its linked issue, determine why the shared build failed when the attributed owner was not responding, repair routing/hand evidence so the batch can proceed, and acknowledge the context. Do not duplicate the existing issue or claim runtime proof.

# Specification alignment

- `docs/specs/dev-loop/agent-coordination.md`: append findings to the existing issue, never duplicate, never edit a leased path.
- `docs/workflows/issue-intake-and-triage.md`: append-only records, preserve prior text, no close without proof.
- AGENTS.md 0b/0c/0d and 2/4: no compiler, no build request, no second dev-loop, no source edit; separate build evidence from runtime evidence; do not claim runtime proof.

# Exact implementation changes

None. No source, specification, or configuration change was made by this session.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/build-friction/20261124-shared-build-blocked-multiplayer-projectiles-cpp-issue-d75f928fbd6f.md` | issue created by `npc-elite-fcs-5` | + root-cause/compile/build evidence, related build set, +2 reviewer suggestions | required verified review record for the user context | remove only the appended suggestions; do not rewrite prior text |
| `.dev/agent-coord/brain.db` | issue row with context link | + evidence/related_build update, 2 suggestions, context ack, activity phase | coordination record | restore from backup if withdrawn |

# Contract / ABI changes

None.

# State / schema changes

None.

# Diagnostics

- Read-only evidence from `src/network/multiplayer-projectiles.cpp` (forward declaration line 438, first use line 446, definition line 516; `git diff` shows only the added declaration), `.dev/build-timings/game-dll-20261124T003901444604Z-3c8d4e.jsonl` (`build.started` note `d57339951817`; `multiplayer-projectiles.cpp` `translation_unit.completed` status `compiled` 61.598s at 2026-11-24T00:40:53.932Z; `build.completed` `exit_code=0` generation 576 at 2026-11-24T00:54:50.553Z publishing `build\game\generation-576\game-g576.dll`), and `dev-loop.py --status`.
- No new diagnostic code or files were added.

# Validation

- `python devscripts/agent-coord.py begin/sync/context list --high` executed; the context was already linked to `issue-d75f928fbd6f` by `npc-elite-fcs-5`, so no duplicate issue was created.
- `issue show`/`issue context`/`issue suggest` recorded the root cause, compile evidence, build success, and pending runtime proof.
- `context ack ctx-db97843045e4` returned `acknowledged: 1`.
- `activity set` recorded the review phase and next step.
- No compiler, build request, module worker, second dev-loop, or test run was invoked.

# Findings

1. Root cause: within one translation unit, `projectileTrailConfigFor` was first called before its definition, so `changed_source_preflight` rejected `src/network/multiplayer-projectiles.cpp` in attempt `game-dll-20261123T234301570308Z-6c7a9c` (157.351s).
2. Routing: the unresponsive owner named in the issue (`melee-parity`) is the board-attributed path owner, not the issue owner. The issue owner `npc-elite-fcs-5` is active and had already applied the minimal forward declaration at line 438.
3. Build evidence: the fixed TU compiled (status `compiled`, 61.598s) in shared build `game-dll-20261124T003901444604Z-3c8d4e` (source_generation 40, generation 576), which completed `exit_code=0` and published `build\game\generation-576\game-g576.dll`. Compile acceptance is met.
4. Remaining: the issue's proof expectation also asks for a pinned FCS runtime journal. A successful build is not gameplay/runtime proof, so the issue stays `assigned` to `npc-elite-fcs-5`; it was not closed.

# Confidence

High on the root cause, the ownership distinction, the projectiles TU compile status, and the `exit_code=0` generation-576 publish (direct source, build journal, and dev-loop status). Runtime behavior remains unproven by this review.

# Proof

No proof claim. This is a build-friction review; the compile/publish evidence is build-tier, not behavior proof.
