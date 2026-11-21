// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Shared Issue Learning Contract

# Task

- Task ID: session-20261121-002356
- Session ID: session-20261121-002356
- Owner: root-shared-issue-evidence-rules
- Focus: Append-only shared issue memory, role-fluid collaboration, live evidence, and workflow/meta-review contract
- Summary: Created and triaged issue issue-70cbf4d13c02, updated the DLL build issue with the explicit unknown snapshot root cause and idle-maintained incremental source/dependency-index hypothesis, and added shared issue-learning and cross-agent workflow-review rules to issue intake, coordination, self-improvement, and context contract specs. Broadcast the contract to all agents. Documentation diff check passed; no tests or builds run.
- Status: complete
- Date, time, timezone: 2026-11-21T00:24:13+00:00 (2026-11-20 19:24 EST)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

# Pre-existing changes

- The shared checkout contained extensive concurrent source, issue, spec, and changelog changes. The captured status below is a shared-worktree snapshot, not a list of this session's edits.
- This session edited only `docs/workflows/issue-intake-and-triage.md`, `docs/specs/dev-loop/agent-self-improvement.md`, `docs/specs/dev-loop/agent-context-contract.md`, and `docs/specs/dev-loop/agent-coordination.md`, and created its issue and changelog records.
- Other active agent edits, including DLL build-coordinator files and unrelated gameplay/UI work, were preserved.

# Requested behavior

- Record nontrivial findings, uncertainty, recommendations, tests, and workflow gaps communicated to the user in the shared issue tracker.
- Let any qualified agent assume temporary roles, including workflow/meta-review, and make issues complete enough for cold handoff. Accumulate live evidence, tests, proposed fixes, counterevidence, and process-efficiency opportunities over time.

# Specification alignment

- Updated references: `docs/workflows/issue-intake-and-triage.md`, `docs/specs/dev-loop/agent-self-improvement.md`, `docs/specs/dev-loop/agent-context-contract.md`, and `docs/specs/dev-loop/agent-coordination.md`. Router routes already point issue intake, agent self-improvement, context/handoff, and coordination work to these authorities.
- Existing contract already required complete issue intake and agent-portable context; this change makes user-facing analysis append-only, labels claim status, requires positive and negative evidence, and states the temporary meta-review role explicitly.
- No permanent privileged meta-agent is introduced. Any agent can take the duty at phase boundaries or useful waits, with the issue owner coordinating the synthesis.

# Exact implementation changes

## Shared issue and specification updates

- Created issue `issue-70cbf4d13c02` and task `shared-issue-collaboration-contract`; linked the earlier DLL issue and a related user context about periodic agent reviews.
- Appended to `issue-136d4553ff92` the explicit unknown snapshot substage cause, idle-maintained incremental source/dependency-index idea, and completed-attempt evidence.
- Added shared-finding/claim-status and continued-issue-evidence rules in `docs/workflows/issue-intake-and-triage.md:19` and `:30`; role-fluid meta-review rules in `docs/specs/dev-loop/agent-self-improvement.md:39`, `:58`, and `:68`; issue analysis and cold-handoff fields in `docs/specs/dev-loop/agent-context-contract.md:16` and `:50`; and collaboration/meta-review process in `docs/specs/dev-loop/agent-coordination.md:150`.
- Broadcast the new contract to all agents through the shared coordination brain.
- No code paths, build authority, human approval boundaries, or runtime behavior were changed.

# Changed files and reverse steps

The generator's captured shared-worktree status below includes concurrent files. The table here lists only files created or edited by this session.

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/workflows/issue-intake-and-triage.md` | Required full issue intake but did not explicitly say to append user-facing findings | Added append-only findings, claim labels, cold-handoff, and ongoing evidence rules | Keep findings in shared brain | Revert only this session's paragraph block |
| `docs/specs/dev-loop/agent-self-improvement.md` | Improvement proposals were encouraged; meta-review and issue-collaboration duties were implicit | Added role-fluid collaboration, shared findings, and process meta-review contract | All agents can improve and review workflow | Revert only this session's section |
| `docs/specs/dev-loop/agent-context-contract.md` | Shared issue brief already contained required fields and evidence | Added explicit claim analysis, handoff, and evolving positive/negative evidence fields | Make cold handoff usable by any agent | Revert only this session's added bullets |
| `docs/specs/dev-loop/agent-coordination.md` | Coordination described issue context and review while waiting | Added role-fluid cross-agent collaboration and workflow/meta-review process | Make the role available to every agent | Revert only this session's section |
| `docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md` | Missing | New complete user-requested issue record | Shared tracker entry for the policy improvement | Remove only if this issue is explicitly withdrawn |
| `docs/changelog/2026-11-21/20261121_002418-shared-issue-learning-contract.md` | Missing | This session record | Preserve implementation/evidence/hand-off | Remove only if this session record is explicitly withdrawn |
# Contract / ABI changes

- Contract: Written issue/intake/coordination process only.
- Version bump: None.
- Compatibility: No code or coordination schema changed.
- Reverse: Revert only the documented additions in the four docs and keep the append-only issue/changelog history.

# State / schema changes

- Domain: None.
- Old schema: N/A.
- New schema: N/A.
- Migration: None.
- Reverse: N/A.

# Deleted paths

- Path: None.
- Before: N/A.
- Why: No paths deleted.
- Reverse: N/A.

# Diagnostics

- Owner/category: Shared coordination brain; issue discussion/context.
- Input: Nontrivial finding, claim, hypothesis, test result, counterexample, or workflow gap.
- Decision: Append to the best existing issue or create a complete new issue; label claim status and attach evidence/next check.
- Output: Durable issue discussion and agent-portable handoff context.
- Failure or rejection reason: Not applicable in this documentation-only update.
- Rate limiting: Record actionable findings, reuse existing issues, and avoid duplicate conversational noise.

# Validation

- Focused docs: `docs/workflows/issue-intake-and-triage.md`, `docs/specs/dev-loop/agent-self-improvement.md`, `docs/specs/dev-loop/agent-context-contract.md`, and `docs/specs/dev-loop/agent-coordination.md` reviewed for cross-reference and consistency.
- `git diff --check -- <four docs>` passed. No tests were run; this is a documentation/workflow update.
- No build requested. A concurrent root dev-loop DLL build completed independently; its evidence was appended to the DLL issue but is not proof of runtime gameplay behavior.
- No runtime evidence claimed.
- Outputs: issue `issue-70cbf4d13c02`, task `shared-issue-collaboration-contract`, and the updated DLL investigation issue `issue-136d4553ff92`.

# Measured evidence

- Before: issue specs asked agents to propose improvements and maintain full issue fields, but did not explicitly require user-facing hypotheses/recommendations to be appended, any-agent workflow/meta-review roles, or ongoing positive/negative evidence and process-gap reviews.
- After: those requirements are written in four canonical issue/agent workflow documents and broadcast through the brain. Adoption by future agents remains to be observed.
- DLL evidence appended to issue `issue-136d4553ff92`: attempt `game-dll-20261120T234259888678Z-a5067c` completed successfully in 1731.784s, generation 417. It recorded 301.514s manifest snapshot, 221.938s source snapshot, 337.838s object scan, 342.853s changed-source preflight, 72.067s compile phase wall time, 100.340s link, and 69.910s publish. The `src/npc/npc.cpp` TU took 342.631s. These are build timings only; no behavior claim is made.

# Regression review

- Regression entry appended: no.
- Why this is or is not a confirmed regression: No gameplay behavior changed or regressed.
- Related regression paths: None.

# Human acceptance

- No visual, gameplay, or multiplayer review applies to this documentation-only change.
- Still unverified: whether future agents consistently follow the new shared-issue and meta-review contract. The issue stays open for longitudinal evidence.

# Agent handoff context

- Owner: `root-shared-issue-evidence-rules`; issue `issue-70cbf4d13c02`, task `shared-issue-collaboration-contract`.
- Check the four updated documents listed above and the issue itself. DLL timing investigation is separately owned by `build-coordinator` under `issue-136d4553ff92`.
- Do not redo the issue intake or spec edits. Future agents should append evidence/examples and review actual adoption rather than rewriting the rules.
- Next exact step: use the issue brief on future activations; append examples where findings are captured or missed, and track whether any-agent handoffs work from the issue brief and artifacts alone.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d651e1332fccff70bc5cc4601dc24d55784820b5 / d651e1332fccff70bc5cc4601dc24d55784820b5
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-20T20:20:51.788061Z", "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55", "latest_successful_source_generation": 38}, "game_current": {"build_mode": "release", "code_hash": "95fb913f80de54772285da4373d367b47dfbcf882c1d2636f52ff692b791a644", "dll": "build\\game\\generation-417\\game-g417.dll", "generation": 417, "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-417\\game-g417.dll", "generation": 417, "published_utc": "2026-11-21T00:11:58.032Z", "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 31.195, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-20T21:40:39.596Z"}}
- Generation: 417
- Artifact: build\game\generation-417\game-g417.dll
- Journal path: C:\mimita-v9\logs\2026-11-21\20261121_000922\server-01\events.jsonl
- Session ID: session-20261121-002356

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path: None; issue/specification policy update.

## Captured git state

```text
M config/accounts/default.json
 M config/analytics.json
 M config/audio/music-settings.json
 M config/behavior-profiles.json
 M config/movement/movement-heavy.json
 M config/movement/movement-source.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/game_build_worker.py
 M devscripts/quality/test_game_build_snapshot.py
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md
 M docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
 M docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
 M docs/specs/dev-loop/agent-build-queue.md
 M docs/specs/dev-loop/agent-context-contract.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/workflows/issue-intake-and-triage.md
 M src/npc/npc.cpp
?? devscripts/quality/test_build_coordinator.py
?? docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md
?? docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md
?? docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md
?? docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md
?? docs/changelog/2026-11-21/
?? docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
?? docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
?? docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md
?? docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md
?? docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
?? docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md
?? docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
?? docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md
?? docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md
?? docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/behavior-profiles.json                      |  23 ++---
 config/movement/movement-heavy.json                |   3 +-
 config/movement/movement-source.json               |   3 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             | 110 +++++++++++++++++++--
 devscripts/dev_loop/game_build.py                  |  21 ++--
 devscripts/game_build_worker.py                    |  93 ++++++++++++++++-
 devscripts/quality/test_game_build_snapshot.py     |  30 +++++-
 ...e-issues-preview-overlay-first-frame-context.md |  30 +++++-
 docs/issues/README.md                              |  29 ++++--
 ...ventory-can-omit-or-miscl-issue-35fd6df3d947.md |  14 +++
 ...cy-no-frame-below-240-fps-issue-549680e53b26.md |   6 ++
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |  35 +++++++
 ...e-dll-build-at-game-build-issue-0322fa45b0e8.md |  18 ++++
 ...ative-editing-and-credite-issue-421c5be1669f.md |   6 ++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  36 +++++++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  12 +++
 ...ner-preview-overlays-and--issue-06c8cd76dc9d.md |  26 +++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  40 ++++++++
 .../counterstrike-npc-targeting-REG-2026-10-04.md  |  27 +++++
 docs/specs/dev-loop/agent-build-queue.md           |  11 +++
 docs/specs/dev-loop/agent-context-contract.md      |  12 +++
 docs/specs/dev-loop/agent-coordination.md          |  23 +++++
 docs/specs/dev-loop/agent-self-improvement.md      |  36 +++++++
 docs/specs/dev-loop/build-time-observability.md    |  22 +++++
 docs/workflows/issue-intake-and-triage.md          |  16 +++
 src/npc/npc.cpp                                    |  11 ++-
 31 files changed, 646 insertions(+), 59 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/behavior-profiles.json
M	config/movement/movement-heavy.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/game_build_worker.py
M	devscripts/quality/test_game_build_snapshot.py
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md
M	docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
M	docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
M	docs/specs/dev-loop/agent-build-queue.md
M	docs/specs/dev-loop/agent-context-contract.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/workflows/issue-intake-and-triage.md
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-20T20:20:51.788061Z",
    "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55",
    "latest_successful_source_generation": 38
  },
  "game_current": {
    "build_mode": "release",
    "code_hash": "95fb913f80de54772285da4373d367b47dfbcf882c1d2636f52ff692b791a644",
    "dll": "build\\game\\generation-417\\game-g417.dll",
    "generation": 417,
    "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-417\\game-g417.dll",
    "generation": 417,
    "published_utc": "2026-11-21T00:11:58.032Z",
    "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 31.195,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-20T21:40:39.596Z"
  }
}
```
