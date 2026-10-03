# Actor-preset NPC movement policy (forward local-sensing)

Date: 2026-10-03
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and automated (pure + real `NpcSystem`) evidence are proven. No
live multiplayer session or visual acceptance was performed; the human checklist
at the end remains required.

## Scope

Add a generic, JSON-controlled NPC movement policy owned by actor presets and
configure the existing Counter-Strike preset to use forward, local-sensing
movement. No Counter-Strike-specific branches were added to the NPC code. The
first success condition: CS NPCs keep moving forward, never randomly select
Circle/Strafe/ZigZag/RandomWalk, sense only nearby obstacles, make small local
corrections around walls, jump only when necessary, retreat on a percentage of
health, and update live from `counter_strike.json`.

Per the human's answers this session:

1. Retreat is emergent: it is a probability that rises as health drops and
   dominates at or below ~60% of the configured threshold (about 20% health for
   the default `0.35`), not a hard branch.
2. The policy owner lives in `src/npc/npc-movement-policy.*`.
3. The acceptance test builds a real `NpcSystem` and drives the actual update
   path (`--npc-movement-policy-selftest`).

## Pre-existing edits (NOT mine, NOT touched or claimed)

The working tree already contained large uncommitted edits from earlier
sessions. In particular, before this session `config/actor-presets/counter_strike.json`
had revolver `damage` 35 (HEAD has 100), and `src/game/game-cli.cpp` was already
modified. The current `--actor-preset-selftest` asserts the old revolver damage
`== 100.0f`, so it FAILS on the pre-existing weapon tuning, independent of this
change. My policy assertions in that command print and pass; the authoritative
policy test is `--npc-movement-policy-selftest`. I did not modify the pre-existing
weapon values.

## Configuration contract (unchanged owners)

- `config/movement/movement-heavy.json` — body physics.
- `config/behavior-profiles.json` — combat skill.
- `config/npc-difficulty.json` — global NPC difficulty/combat.
- `config/actor-presets/counter_strike.json` — NPC movement/decision policy (new
  `npc_behavior` block).
- `config/gamemodes/counterstrike.json` — teams/rounds/bomb/objectives (untouched).

## Files changed

### New policy owner: `src/npc/npc-movement-policy.h` / `.cpp`

- `NpcMovementPolicy` struct exactly as specified (plus `configured`).
- `parseNpcMovementPolicy(json, out, error)`: strict on enum strings and boolean
  types (unknown/wrong type rejects the preset), clamping on numeric ranges
  (`retreat_health_fraction` → [0,1], `movement_noise` → [0,1]).
- Pure queries reused by the brain and tests:
  - `npcHealthFraction`, `npcLowHealth` (boundary-inclusive so 35/100 at 0.35 is
    low, matching the acceptance examples).
  - `npcRetreatChance`: 0 at/above `retreat_health_fraction`, linear severity
    below it, and `0.75 + 0.25 * kneeDrop` at or below ~60% of the threshold.
  - `NpcPolicyMovement` + `npcPolicyAllowsMovement`.
  - `NpcJumpReason` + `npcPolicyAllowsJump` (never/obstacle_only/navigation_only/
    obstacle_or_navigation).
  - `NpcDashReason` + `npcPolicyAllowsDash` (never/attack/escape/navigation/
    attack_or_navigation).

### Actor-preset loader: `src/gamemode/match-roles.h` / `.cpp`

- `MatchRoleDefinition` gains `movementPolicy`.
- `readActorPreset` returns success and delegates `npc_behavior` parsing to the
  policy owner.
- `loadActorPresets` keeps the previous valid preset by id when a file is
  malformed or has an invalid policy, records the bad file's write time so it is
  not retried until edited again, and increments `mActorPresetRevision`.
- Emits one rate-limited `[NPC POLICY] preset=... revision=... travel=...`
  warning per configured policy via `Debug::warn(Duel)` (same channel as the
  existing `[ACTOR PRESET]` log).

### Spawn plumbing: `src/network/server-gamemode.{h,cpp}`, `src/npc/npc.h`, `src/network/server-npcs.cpp`

- `ActorSpawnProfile.actorPresetId` is resolved from the role's `actor_preset`
  (or the preset's own id); the live `Npc.actorPresetId` is set on adoption,
  respawn, and round-reset spawn.

### Policy consumption

`src/npc/npc-internal.h` / `npc.cpp`:
- New `activeMovementPolicy(npc)` resolves the policy live from
  `MatchRoleRegistry` by id every tick, so a hot reload reaches already-living
  NPCs with no stored pointer and no restart. Null means legacy brain.
- Deleted the dead `shouldJump` (its random-jump branch contradicted the policy).
- Jump now carries an explicit `NpcJumpReason`; a policy actor jumps only for an
  allowed reason (obstacle/climb/traversal), never merely because it is stuck.
- Dash reasons are gated by the policy (`attack`/`escape`); the legacy random
  `shouldDash` is only consulted for allowed reasons via a navigation fallback.
- Stuck recovery for a policy actor uses local turn (already applied by
  `wallAvoidDirection`) + `requestRepath`; the random `unstuckDirection` and the
  reflexive jump/dash are skipped. The multi-second backtrack is skipped too
  (`blocked_behavior: turn_then_repath`).
- Cover-seeking uses the policy's `retreatHealthFraction` instead of 0.4.

`src/npc/npc-states.cpp`:
- `movement_noise` scales the directional offset; `0.0` means zero jitter.

`src/npc/npc-state-machine.cpp`:
- `pickNextState` resolves the policy. For `travel_style == "forward"`:
  - stuck → `Advance` (never `RandomWalk`),
  - search close to last-known → `Patrol` (never `Circle`),
  - with a target → `Retreat` when the emergent `npcRetreatChance` roll hits,
    otherwise `Advance` (deterministic forward, not scored).
- Legacy scored selection is still used for non-forward travel styles, and the
  `allow_*` flags filter `Circle`/`Strafe`/`ZigZag`/`HoldPosition`/`RandomWalk`
  out of the candidate list.

### Config

`config/actor-presets/counter_strike.json` gains the `npc_behavior` block
(`travel_style: forward`, `combat_style: forward`, all `allow_*` false,
`retreat_style: low_health`, `retreat_health_fraction: 0.35`,
`jump_style: obstacle_or_navigation`, `dash_style: attack_or_navigation`,
`world_knowledge: local_sensing`, `blocked_behavior: turn_then_repath`,
`movement_noise: 0.0`) with the repository's `//` comment style.

## Tests

- `tests/npc-movement-policy-test.cpp` (new, standalone `check()` harness, links
  only `npc-movement-policy.cpp`):
  - parses and asserts every `counter_strike` field;
  - rejects unknown enums, wrong-typed booleans, non-object `npc_behavior`;
  - clamps `retreat_health_fraction` (-0.5→0, 1.5→1) and `movement_noise` (→0),
    rejects non-numeric noise;
  - percentage health at 100/1000/10000/1000000 maximums;
  - emergent retreat chance (full health 0, 20% ≥ 0.75, 0% = 1, scale-invariant);
  - movement/jump/dash permission matrices.
- `src/game/game-cli.cpp` `--npc-movement-policy-selftest` (new): builds a real
  `World` (floor + wall) and `NpcSystem`, spawns an NPC with
  `actorPresetId = counter_strike`, and drives `updateOneWithTarget` for real
  ticks. Asserts `Advance` movement, never Circle/RandomWalk/Strafe/ZigZag, real
  forward displacement, wall recovery via lateral correction without random
  wandering, and emergent percentage retreat via `pickNextState`.
- `--actor-preset-selftest` extended to print/assert the parsed policy fields.

## Evidence

### Source / config

- New policy owner, loader wiring, spawn plumbing, hot-path gating, and config
  block as listed above. `npc_behavior` parsed values confirmed by both the
  actor-preset selftest print and the unit test.

### Build

- `python build.py build-only` after deleting the changed objects:
  `Compiled: 8`, `[LINK] mimita.exe`, `BUILD SUCCESS` (exit 0).
- `python build_agent.py` acquired/released the lock; the pre-existing agent
  script does not capture `build.py` stdout, so its `[BUILD]` status line is
  unreliable — the direct `build.py` run above is the authoritative build proof.

### Automated

- `build/npc-movement-policy-test.exe`: `PASS (72 checks)`.
- `mimita.exe --npc-movement-policy-selftest`:
  `PASS` (`lowHealthRetreats=257/300 fullHealthRetreats=0 lateral=2.72`).
- `mimita.exe --actor-preset-selftest`: policy line correct
  (`configured=1 travel=forward ... noise=0.00 retreat=0.35`) but overall FAIL
  due to the pre-existing revolver-damage assertion (35 != 100). Not caused by
  this change.

### Runtime / human

Not performed. Live multiplayer movement, the `[NPC POLICY]` reload log
(requires the runtime `StructuredLogger`), and live reload behavior need a real
Counter-Strike session.

## Human acceptance checklist

1. NPCs move continuously after GO; do not randomly circle/zigzag/wander.
2. NPCs do not repeatedly press into walls; make small local corrections.
3. NPCs jump only for obstacles/traversal, never because they are merely stuck.
4. NPCs at low health retreat more often (percentage-based, same at any max HP).
5. Edit `config/actor-presets/counter_strike.json` `npc_behavior`, save, and
   confirm living NPCs change without a restart and the `[NPC POLICY]` line logs
   a new revision.
