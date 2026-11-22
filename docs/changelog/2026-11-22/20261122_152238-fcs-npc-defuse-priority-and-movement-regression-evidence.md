// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# FCS NPC defuse priority and movement regression evidence

# Task

- Task ID: session-35e2255bd511
- Session ID: session-35e2255bd511
- Owner: fcs-npc-speed-combat-investigation
- Focus: Fun Counter Strike NPC movement, objective, and defuse behavior
- Summary: Expanded the existing FCS regression with the user's speed, movement, wall/stuck, combat, grenade, team-report and objective-timing acceptance. Added a minimal utility-scoring correction so an assigned CT pursues DefuseObjective before interaction range; root dev-loop built game DLL generation 500 with build note 4b3d4ffda2ea. The gen478 journal gives a coarse sparse-sample speed baseline (8.43 3D world-units/s time-weighted across 19 NPC IDs) and shows 2,223 stuck-episode events, 1,156 failed-progress records, one completed plant, five CT assignments, and no defuse start/completion before bomb explosion. Two isolated generation-500 FCS test starts failed updating run.json with WinError 5; an alternate root-managed runtime exited before server journaling. CT defuse behavior and full measurement/acceptance remain unproven; shared Juggernaut processes were preserved.
- Status: source correction built; runtime proof pending
- Date, time, timezone: 2026-11-22T15:22:28+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 70f3c05171fd81885326a1bc5d95684b45b277bb
- Final commit: 70f3c05171fd81885326a1bc5d95684b45b277bb

# Pre-existing changes

- Exact status output:
- Files not created or modified by this session:

# Requested behavior

# Specification alignment

- Current specification paths:
- Exact requirements:
- Why the change follows the specification:
- Conflicts or decisions:

# Exact implementation changes

## File: `path/to/file`

- Lines/functions/headings:
- Old content or behavior:
- New content or behavior:
- Reason:
- Why unrelated behavior is preserved:

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `AGENTS.md` |  |  |  | git checkout -- AGENTS.md |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/instances.py` |  |  |  | git checkout -- devscripts/dev_loop/instances.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `docs/changelog/2026-11-22/20261122_141412-cpp-latency-failure-containment-and-snapshot-benchmark.md` |  |  |  | git checkout -- docs/changelog/2026-11-22/20261122_141412-cpp-latency-failure-containment-and-snapshot-benchmark.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md` |  |  |  | git checkout -- docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md |
| `docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md` |  |  |  | git checkout -- docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md |
| `docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md` |  |  |  | git checkout -- docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/bug/20261120-juggernaut-67-npc-runtime-misses-60-hz-and-240-f-issue-8bfc1ca56222.md` |  |  |  | git checkout -- docs/issues/bug/20261120-juggernaut-67-npc-runtime-misses-60-hz-and-240-f-issue-8bfc1ca56222.md |
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` |  |  |  | git checkout -- docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md |
| `docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md` |  |  |  | git checkout -- docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md |
| `docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md` |  |  |  | git checkout -- docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md |
| `docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-3ffb3f29b1f7.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-3ffb3f29b1f7.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-40ebd14d5cf4.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-40ebd14d5cf4.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-5870fafa670e.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-5870fafa670e.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md |
| `docs/issues/improvement/20261120-any-agent-can-ask-the-user-open-questions-and-sh-issue-c9d8a98dd241.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-any-agent-can-ask-the-user-open-questions-and-sh-issue-c9d8a98dd241.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/regressions/npc-movement/README.md` |  |  |  | git checkout -- docs/regressions/npc-movement/README.md |
| `docs/specs/dev-loop/agent-coordination.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-coordination.md |
| `src/npc/npc-utility.cpp` |  |  |  | git checkout -- src/npc/npc-utility.cpp |
| `devscripts/quality/test_game_build_time_report.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_game_build_time_report.py' |
| `docs/changelog/2026-11-22/20261122_095800-cpp-latency-compiler-phase-report-and-failure-boundary.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_095800-cpp-latency-compiler-phase-report-and-failure-boundary.md' |
| `docs/changelog/2026-11-22/20261122_144600-issue-c9d8a98dd241-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_144600-issue-c9d8a98dd241-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_144624-build-wait-reviewer-overbudget-game-dll-a236.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_144624-build-wait-reviewer-overbudget-game-dll-a236.md' |
| `docs/changelog/2026-11-22/20261122_144800-build-wait-reviewer-overbudget-game-dll-40ebd14d.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_144800-build-wait-reviewer-overbudget-game-dll-40ebd14d.md' |
| `docs/changelog/2026-11-22/20261122_145620-mimita-cpu-evidence-and-agent-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_145620-mimita-cpu-evidence-and-agent-handoff.md' |
| `docs/changelog/2026-11-22/20261122_151300-cpp-latency-gen500-report-format-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_151300-cpp-latency-gen500-report-format-investigation.md' |
| `docs/changelog/2026-11-22/20261122_151340-juggernaut-horde-target-and-fire-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_151340-juggernaut-horde-target-and-fire-investigation.md' |
| `docs/changelog/2026-11-22/20261122_152126-fcs-npc-defuse-priority-and-movement-regression-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_152126-fcs-npc-defuse-priority-and-movement-regression-evidence.md' |
| `docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md' |

# Contract / ABI changes

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

- Domain:
- Old schema:
- New schema:
- Migration:
- Reverse:

# Deleted paths

- Path:
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Visual review:
- Gameplay review:
- Multiplayer review:
- Still unverified:

# Agent handoff context

- Owner: fcs-npc-speed-combat-investigation
- Focus: Fun Counter Strike NPC movement, objective, and defuse behavior
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 70f3c05171fd81885326a1bc5d95684b45b277bb / 70f3c05171fd81885326a1bc5d95684b45b277bb
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-22T08:00:31.376619Z", "latest_successful_build_id": "2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136", "latest_successful_source_generation": 3}, "game_current": {"build_mode": "debug", "code_hash": "907486df17b9ed036abfeeb830a262626b42587321a513d9438e0c5cc2decbdc", "dll": "build\\game\\generation-500\\game-g500.dll", "generation": 500, "revision": "f0cf38641ebb9c4e4850cf7055fc16229fdf3b065dd00aca6895b5b558c1ff8a", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-500\\game-g500.dll", "generation": 500, "published_utc": "2026-11-22T14:50:23.447Z", "revision": "f0cf38641ebb9c4e4850cf7055fc16229fdf3b065dd00aca6895b5b558c1ff8a", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 32.403, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-22T14:51:05.808Z"}}
- Generation: 500
- Artifact: build\game\generation-500\game-g500.dll
- Journal path: C:\mimita-v9\logs\2026-11-22\20261122_150906\server-01\events.jsonl
- Session ID: session-35e2255bd511

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path:

## Captured git state

```text
M AGENTS.md
 M config/accounts/default.json
 M config/analytics.json
 M config/audio/music-settings.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev_loop/instances.py
 M devscripts/game_build_worker.py
 M docs/changelog/2026-11-22/20261122_141412-cpp-latency-failure-containment-and-snapshot-benchmark.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
 M docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md
 M docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261120-juggernaut-67-npc-runtime-misses-60-hz-and-240-f-issue-8bfc1ca56222.md
 M docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
 M docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md
 M docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
 M docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-3ffb3f29b1f7.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-40ebd14d5cf4.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-5870fafa670e.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md
 M docs/issues/improvement/20261120-any-agent-can-ask-the-user-open-questions-and-sh-issue-c9d8a98dd241.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/regressions/npc-movement/README.md
 M docs/specs/dev-loop/agent-coordination.md
 M src/npc/npc-utility.cpp
?? devscripts/quality/test_game_build_time_report.py
?? docs/changelog/2026-11-22/20261122_095800-cpp-latency-compiler-phase-report-and-failure-boundary.md
?? docs/changelog/2026-11-22/20261122_144600-issue-c9d8a98dd241-independent-review.md
?? docs/changelog/2026-11-22/20261122_144624-build-wait-reviewer-overbudget-game-dll-a236.md
?? docs/changelog/2026-11-22/20261122_144800-build-wait-reviewer-overbudget-game-dll-40ebd14d.md
?? docs/changelog/2026-11-22/20261122_145620-mimita-cpu-evidence-and-agent-handoff.md
?? docs/changelog/2026-11-22/20261122_151300-cpp-latency-gen500-report-format-investigation.md
?? docs/changelog/2026-11-22/20261122_151340-juggernaut-horde-target-and-fire-investigation.md
?? docs/changelog/2026-11-22/20261122_152126-fcs-npc-defuse-priority-and-movement-regression-evidence.md
?? docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md
```

## Captured diff stat

```text
AGENTS.md                                          |  14 ++
 config/accounts/default.json                       |   8 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   4 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/instances.py                   |   9 +-
 devscripts/game_build_worker.py                    | 165 +++++++++++++++++++--
 ...y-failure-containment-and-snapshot-benchmark.md |   8 +
 docs/issues/README.md                              |  32 ++--
 ...nnecting-to-a-new-dev-loo-issue-55bd775c8026.md |  12 ++
 ...-snapshots-and-reconnecti-issue-453d0596e294.md |  12 ++
 ...cy-no-frame-below-240-fps-issue-549680e53b26.md |   6 +
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  36 +++++
 ...me-misses-60-hz-and-240-f-issue-8bfc1ca56222.md |   6 +
 ...rs-fail-to-consistently-e-issue-b95574b63f29.md |   6 +
 ...-not-fire-at-opposing-act-issue-1b2463da5666.md |   6 +
 ...igation-regresses-into-co-issue-b18e63894585.md |   6 +
 ...kes-dispatch-late-codex-p-issue-84c584957e65.md |   8 +
 ...e-dll-build-at-game-build-issue-3ffb3f29b1f7.md |  32 ++++
 ...e-dll-build-at-game-build-issue-40ebd14d5cf4.md |  34 +++++
 ...e-dll-build-at-game-build-issue-5870fafa670e.md |  21 +++
 ...e-dll-build-at-game-build-issue-a236165fa6fc.md |  46 ++++++
 ...ser-open-questions-and-sh-issue-c9d8a98dd241.md |  49 ++++++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  12 ++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  84 +++++++++++
 ...ompleteness-and-readiness-issue-3100aa7b5807.md |  12 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  12 ++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md |  12 ++
 docs/regressions/npc-movement/README.md            |   9 +-
 docs/specs/dev-loop/agent-coordination.md          |  17 +++
 src/npc/npc-utility.cpp                            |   4 +-
 32 files changed, 643 insertions(+), 43 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev_loop/instances.py
M	devscripts/game_build_worker.py
M	docs/changelog/2026-11-22/20261122_141412-cpp-latency-failure-containment-and-snapshot-benchmark.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
M	docs/issues/bug/20261120-diagnose-delayed-server-snapshots-and-reconnecti-issue-453d0596e294.md
M	docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-juggernaut-67-npc-runtime-misses-60-hz-and-240-f-issue-8bfc1ca56222.md
M	docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
M	docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md
M	docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
M	docs/issues/build-friction/20261122-over-budget-reviewer-wakes-dispatch-late-codex-p-issue-84c584957e65.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-3ffb3f29b1f7.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-40ebd14d5cf4.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-5870fafa670e.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a236165fa6fc.md
M	docs/issues/improvement/20261120-any-agent-can-ask-the-user-open-questions-and-sh-issue-c9d8a98dd241.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/regressions/npc-movement/README.md
M	docs/specs/dev-loop/agent-coordination.md
M	src/npc/npc-utility.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-22T08:00:31.376619Z",
    "latest_successful_build_id": "2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136",
    "latest_successful_source_generation": 3
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "907486df17b9ed036abfeeb830a262626b42587321a513d9438e0c5cc2decbdc",
    "dll": "build\\game\\generation-500\\game-g500.dll",
    "generation": 500,
    "revision": "f0cf38641ebb9c4e4850cf7055fc16229fdf3b065dd00aca6895b5b558c1ff8a",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-500\\game-g500.dll",
    "generation": 500,
    "published_utc": "2026-11-22T14:50:23.447Z",
    "revision": "f0cf38641ebb9c4e4850cf7055fc16229fdf3b065dd00aca6895b5b558c1ff8a",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 32.403,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-22T14:51:05.808Z"
  }
}
```
