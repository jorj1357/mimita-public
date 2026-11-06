# Bloodtest NPC difficulty and respawn

- Recorded: `2026-11-06T02:57:07Z` (2026-11-05 21:57:07 EST)
- Branch: `20261011zt`
- HEAD: `e3cae756828e55225f85c401a39b9b739995b021`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Change

- `config/gamemodes/bloodtest.json:167` changed `npc_behavior_profile` from
  `""` to `"beginner"`. The shared behavior profile defines `beginner` in
  `config/behavior-profiles.json` and reduces NPC combat skill without
  changing role health or weapon damage.
- `docs/specs/gamemodes/bloodtest.md:22-26,67-68` now specifies the beginner
  NPC profile and first-eligible-fixed-tick respawn acceptance.
- No respawn code/config change was needed. Bloodtest already has
  `respawn_seconds: 0.01` at `config/gamemodes/bloodtest.json:14`;
  `serverMatchRespawnSeconds` in `src/network/server-gamemode.cpp` clamps the
  shared value to a minimum of 0.01 seconds, and the NPC simulation loop in
  `src/network/server-npcs.cpp` decrements it by `SERVER_DT` and respawns when
  it reaches zero. The resulting intent is the next eligible fixed tick, not
  a same-tick revive.

## Specification review

- Read `docs/ROUTER.md`, `docs/specs/gamemodes/bloodtest.md`,
  `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/npc-behavior/npc-behavior.md`,
  `docs/architecture/player-npc-systems/npc-movement.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/specs/dev-loop/dev-loop.md`,
  `docs/specs/dev-loop/quality-gates.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/live-development/live-development.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/regressions/README.md`, and
  `docs/regressions/npc-movement/README.md`.
- Skill `docs/skills/spec-behavior-review-v1.md`: PASS. The mode-level profile
  resolver selects `beginner`; the existing shared NPC respawn loop already
  meets the requested next-tick intent. No spec-code conflict remains after
  documenting the behavior.
- No confirmed regression was reported or observed; no regression entry was
  added.

## Validation and runtime evidence

- JSONC config/profile assertion: PASS (`npc_behavior_profile=beginner`,
  `respawn_seconds=0.01`, and a registered `beginner` profile).
- `python devscripts/gamemode_json_sync.py --check`: PASS.
- `git diff --check`: PASS; Git reported its existing LF-to-CRLF conversion
  warning for the edited Markdown file.
- Canonical dev-loop build request: SUCCESS. Published artifact
  `2026-11-06T02:56:03.028386Z`, executable
  `.dev/builds/2026-11-06T02-56-03.028386Z/mimita.exe`, SHA-256
  `7ac7b536bba40972881a5f76a9a23adfb42a89f2c48616b4029a773467836bde`.
  Its identity records `source_changed_during_build=true`. The dev-loop
  initially reported `build_succeeded`, `stale=False`, and the artifact as
  `published_current`. A later status detected source generation 28 and
  began another build; at final inspection that build was still running and
  `stale=True`. The newer source edits are outside this task and were left
  untouched; no claim is made about that build's result.
- The existing visible game process was left running without mode switching,
  restart, or other input. Bloodtest was visible, but no NPC was present in the
  observed view, so the changed profile and death/respawn cycle were not
  exercised in that executable. Runtime journal proof and human gameplay
  acceptance remain open.

## Pre-existing worktree changes

Before this task, the worktree already had edits in
`config/accounts/default.json`, `config/analytics.json`,
`config/audio/music-settings.json`, `config/ragdoll.json`, and the
`devscripts/blender-ai` submodule. Additional unrelated edits observed later
were also preserved: `config/camconfig.json`, camera sources/config including
`src/engine/engine-tick-camera.cpp`, ragdoll mode sources/config, and
`src/sim/simulate-tick.cpp`. No commit was created.
