// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Juggernaut Horde target and fire investigation

# Task

- Task ID: work-36904658ce33
- Session ID: work-36904658ce33
- Owner: horde-targeting-investigation
- Focus: NPC targeting, team_pick role mapping, aim direction, hit tracing, DLL history
- Summary: Measured mixed-version Horde targeting, team-role assignment, and NPC miss evidence
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-22T15:13:26+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 70f3c05171fd81885326a1bc5d95684b45b277bb
- Final commit: 70f3c05171fd81885326a1bc5d95684b45b277bb

# Pre-existing changes

- At the changelog snapshot, the shared checkout already contained 30 modified
  tracked paths and 6 untracked paths from concurrent work. The exact snapshot
  is preserved under `## Captured git state` below. I did not revert, stage, or
  overwrite any of those files.
- This session appended investigation evidence to the existing Horde target
  issue and the existing Juggernaut no-fire issue through `agent-coord issue
  suggest`; it did not edit gameplay source or configuration.
- All other captured changes are pre-existing/concurrent and are outside this
  session's ownership.

# Requested behavior

The user reports a regression after live game-DLL reload: Fighters should keep
the human Juggernaut as an enemy, attack and swarm continuously, preserve
Fighter/Juggernaut team identity through `team_pick 2`, and make attacks lethal
when the shot path actually intersects the player's hit volume. The requested
attack and hit behavior should use shared NPC combat and weapon owners, not a
Horde-only combat implementation.

# Specification alignment

- Current specification paths: `docs/specs/gamemodes/juggernaut-horde.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/specs/weapons/weapons.md`, and
  `docs/workflows/runtime-scenario-validation.md`.
- Exact requirement: Horde Fighters target only Juggernaut actors and
  Juggernaut NPCs target only Fighter actors; friendly fire stays off. A
  failure must be traced on the same run from team/roster assignment through
  targeting, attack gate, firing, confirmed damage, and client receipt.
- The observed target and shot events show the intended opposing-team target
  is selected, but the role identity and hit chain fail in the sampled run.
  This is an evidence-based investigation, not a behavior change.
- User's desired lethality goes beyond the spec's ordinary hit validation.
  Hits still need to follow the shared legal weapon and authoritative contact
  rules; no accuracy or collision exception was added for Horde.

# Exact implementation changes

- No C++ or configuration changes.
- The new run evidence was appended to the existing issues
  `issue-b95574b63f29` (team targeting/role continuity) and
  `issue-1b2463da5666` (NPC firing/hit chain). The user's three verbatim
  messages were also entered once each in the coordination context and linked
  to the existing Horde issue.
- Source evidence identifies the first observed failures at runtime; the
  underlying cause of missing ordered team data and the user's reported
  kill/respawn target decay remain open.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` | No Nov 22 active-run evidence | Appended current team-role/target/shot measurements | Keep the existing Horde regression report current | Keep append-only; use issue correction if needed |
| `docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md` | No matching live shot sample | Appended shot/aim/hit-trace evidence | Connect the shared aim failure to existing no-fire report | Keep append-only; use issue correction if needed |
| `docs/regressions/npc-movement/jugger-horde-team-role-and-npc-aim-REG-2026-11-22.md` | No Nov 22 regression record | Appended observed role and aim failures; root cause/fix open | Preserve user-reported regression with measured evidence | Keep append-only; correct by another dated update |
| `docs/regressions/npc-movement/README.md` | Entry 10 was latest | Added Entry 11 | Index the regression | Keep append-only |
| This changelog | New file | Investigation summary and evidence | Required one-per-session record | Retain as session history |

The other paths in `## Captured changed paths` were already modified or
untracked in the shared checkout and were not changed by this investigation.

# Contract / ABI changes

- Contract: none.
- Version bump: none.
- Compatibility: not applicable.
- Reverse: not applicable.

# State / schema changes

- Domain: none; no schema or persistent game state was changed.
- Old schema: unchanged.
- New schema: unchanged.
- Migration: none.
- Reverse: not applicable.

# Deleted paths

- None.

# Diagnostics

- Owner/category: existing StructuredLogger NPC movement/combat and game
  transport events.
- Input: `team_pick 2`, server target state, NPC fire and hit traces.
- Decision: no new diagnostics were added; existing events distinguish the
  command fallback, team/role result, attack gate, aim error, and hitscan hit.
- Output: exact run journal below; no new log files.
- Failure evidence: the module command probe rejected migration, host role was
  empty, and the sampled shots failed to hit an entity.
- Rate limiting: existing attack-gate logger is rate-limited per NPC; no
  logging policy was changed.

# Validation

- Focused skill paths: `docs/skills/spec-behavior-review-v1.md` — PASS for
  source/spec comparison; runtime and acceptance remain open.
- Tests: none run; this was a read-only investigation.
- Build status: no build requested by this agent. The shared dev-loop later
  reported generation 500 published from a 32.403 s no-op (0 compiled, 87
  skipped). The running scenario journal remains pinned to its own build ID.
- Runtime evidence: inspected active run `20261122_144609_01` in
  `juggernaut horde` on `dust2cyberiav4`. The run began on EXE build
  `2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136`;
  the run's module aim-policy events report generation 499. Generation 500
  became ready after the run started and was not substituted into this evidence.
- Visual acceptance: not performed by this agent.
- Output: `logs/2026-11-22/20261122_144609_01/server-01/events.jsonl` and
  `run.json`.

# Measured evidence

- At the sampled checkpoint, the server journal contained 9,036
  `npc.attack-gate` events; 189 named player 1 as `target_id`. It contained 303
  `npc.shot` events, all aimed at player 1. All 303 were accepted/fired; 0 had
  `hit_entity=true`, 260 had `hit_world=true`, and summed
  `damage_on_target_mirror` was 0. The recorded player health remained 1,567.
- Shot angular error to the actual target: mean 36.85 degrees, maximum 174.60
  degrees, while the active module aim policy reported 2.0 degrees of requested
  error. Weapons among those shots: 186 revolver, 77 shotgun, 40 rocket
  launcher.
- The team command probe at server journal line 15541 logged
  `team_pick 2 accepted=0 authoritative=0 status=probe: policy not migrated`.
  The host event at line 15542 assigned player 1 to team 1 named `BLUE`, with
  `role=""` and `weapon_set=""`.
- Runtime identity: `run.json` names map `dust2cyberiav4`, mode
  `juggernaut horde`, server PID 4540, and the 08:00 EXE build ID above. The
  dev-loop later marked that EXE current after a no-op build; the NPC module
  generation 499 was distinct from subsequently published generation 500.
- Population caveat: the run manifest reports `roster_count=1`, but the server
  journal contains shots from numerous NPC IDs in the `2000xx` range. This
  metadata mismatch means this run does not establish the exact NPC population.
- No matched before/after scenario or per-NPC kill/respawn timeline was
  captured. The user's report that each replacement NPC eventually stops
  treating the player as hostile remains unmeasured.

# Regression review

- Regression entry appended:
  `docs/regressions/npc-movement/jugger-horde-team-role-and-npc-aim-REG-2026-11-22.md`.
- The user report and new active-run evidence were also appended to existing
  open bug issues `issue-b95574b63f29` and `issue-1b2463da5666` to avoid
  duplicate issue records.
- Confirmed current failures: the sampled `team_pick 2` lost the mode role, and
  accepted shots at player 1 recorded no entity hits. The user's DLL causal
  claim is not yet isolated from module/host state or prior runtime state.
- Existing contrary evidence: the Gen 436 Horde run recorded the correct
  Juggernaut role and 14 later target changes to player 1. That run did not
  capture a matching shot/damage chain, so it does not prove the firing or hit
  behavior worked before.
- Related paths: the new regression record, the existing Juggernaut role preset
  regression, and the two issues above. The new record keeps DLL causality and
  the proposed generic fix explicitly unproven.

# Human acceptance

- Visual review: not performed.
- Gameplay review: live journals inspected; user-visible feel and swarming were
  not accepted by this agent.
- Multiplayer review: no separate client/server controlled scenario was run.
- Still unverified: why ordered team descriptors were unavailable at
  `team_pick`, whether death/respawn is what drops hostility, actual rocket
  impact and damage, and the desired more-lethal shared combat behavior.

# Agent handoff context

- Owner: horde-targeting-investigation
- Focus: NPC targeting, team_pick role mapping, aim direction, hit tracing, DLL history
- Files to check: `src/gamemode/gamemode.cpp`,
  `src/network/server-gamemode.cpp`, `src/network/server-npcs.cpp`,
  `src/npc/npc-combat.cpp`, `src/combat/weapon-fire-raycast.cpp`,
  `src/network/server-projectiles.cpp`, and both linked issue files.
- Do not redo: the Gen 436 target-acquisition review or the 303-shot live-run
  summary already appended to the issues.
- Next exact step: on a pinned current EXE and explicit module generation,
  capture team/role before and after DLL reload, then follow same-run
  team-pick -> target -> aim vector -> hitscan/projectile impact -> confirmed
  damage and client receipt. Include a kill/respawn timeline and per-NPC team,
  target, visibility, and firing records.

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 70f3c05171fd81885326a1bc5d95684b45b277bb / 70f3c05171fd81885326a1bc5d95684b45b277bb
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-22T08:00:31.376619Z", "latest_successful_build_id": "2026-11-22T08:00:31.376619Z:4fa387f46e02a3a9d136abff3849b879306ecd354f07e145b35b10d20a8d2136", "latest_successful_source_generation": 3}, "game_current": {"build_mode": "debug", "code_hash": "907486df17b9ed036abfeeb830a262626b42587321a513d9438e0c5cc2decbdc", "dll": "build\\game\\generation-500\\game-g500.dll", "generation": 500, "revision": "f0cf38641ebb9c4e4850cf7055fc16229fdf3b065dd00aca6895b5b558c1ff8a", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-500\\game-g500.dll", "generation": 500, "published_utc": "2026-11-22T14:50:23.447Z", "revision": "f0cf38641ebb9c4e4850cf7055fc16229fdf3b065dd00aca6895b5b558c1ff8a", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 32.403, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-22T14:51:05.808Z"}}
- Generation: 500
- Artifact: build\game\generation-500\game-g500.dll
- Journal path: C:\mimita-v9\logs\2026-11-22\20261122_144609_01\server-01\events.jsonl
- Session ID: work-36904658ce33

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
?? docs/regressions/npc-movement/fcs-objective-speed-defuse-REG-2026-11-22.md
```

## Captured diff stat

```text
AGENTS.md                                          |  14 ++
 config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/instances.py                   |   9 +-
 devscripts/game_build_worker.py                    | 165 +++++++++++++++++++--
 ...y-failure-containment-and-snapshot-benchmark.md |   8 +
 docs/issues/README.md                              |  34 ++---
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
 ...uild-stage-and-investigat-issue-136d4553ff92.md |   6 +
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md |  12 ++
 docs/regressions/npc-movement/README.md            |   9 +-
 docs/specs/dev-loop/agent-coordination.md          |  17 +++
 src/npc/npc-utility.cpp                            |   4 +-
 30 files changed, 632 insertions(+), 38 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
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
