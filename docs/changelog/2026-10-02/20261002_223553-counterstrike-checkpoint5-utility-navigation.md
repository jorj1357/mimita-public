# Counter-Strike Checkpoint 5 — utility goals and navigation request

Date: 2026-10-02
EST timestamp: 2026-10-02 22:35:53 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and scoring/classification runtime evidence are proven. Live NPC
goal-selection behavior and objective play are NOT performed and remain
required.

## Scope

Checkpoint 5 of `docs/specs/20261002plan.md`: reusable utility goals/actions
with hysteresis and the generalized navigation request (Stages 8, 9).

## Pre-existing / external edits (not mine)

Same runtime-written user settings as prior checkpoints
(`collision`/`crosshair`/`movement`/`ragdoll`/`weapons.json`). Untouched.

## Files changed

### `src/npc/npc-utility.h` / `.cpp` (new)

- `UtilityGoalKind`: KillTarget, Survive, HoldPosition, TakeCover,
  MoveToObjective, DefendSite, RotateToSite, PlantObjective, DefuseObjective,
  RetakeSite.
- `UtilityActionKind`: Approach, Retreat, HoldAngle, Peek, Flank, TakeCover,
  Reposition, Shoot, Reload, SwitchWeapon, ThrowAreaEffect, Interact.
- `UtilityContext` (perception, health, distance, LOS, enemy count, team alive,
  time remaining, objective state, weapon readiness, confidence).
- `UtilityGoalScore` keeping every term separate (relevance, distance,
  lineOfSight, health, enemyCount, timeRemaining, objectiveState,
  weaponReadiness, confidence, teamInformation).
- `scoreUtilityGoal`, `selectUtilityGoal` (hysteresis: minimum goal duration,
  switch margin, action cooldown), `actionForGoal`.
- `npcUtilitySelfTest`.

### `src/npc/npc-nav-request.h` / `.cpp` (new)

- `MovementCapabilities` (canWalk/Run/Jump/Drop/Fly/Climb; defaults walk/jump/
  drop on, run/fly/climb off).
- `NavigationRequest` (start, destination, goal kind, target, desired distance,
  tolerance, capabilities).
- `navigationRequestToGoal`: the single adapter to the existing `NpcGoal`.
- `NavSegmentType` + `classifySegment`: Walk/Jump/Drop/Gap/Unreachable under
  capabilities and max jump height.
- `npcNavRequestSelfTest`.

### `src/npc/npc.h`

Added `UtilityState utility` and `UtilityContext utilityContext`; included
`npc/npc-utility.h`.

### `src/npc/npc.cpp`

- `senseWorld` builds a `UtilityContext` from the belief, health, and weapon
  readiness, then calls `selectUtilityGoal`.
- `makeNavGoal` consults `npc.utility.currentGoal` first (engaging, surviving,
  objective goals) and falls back to the legacy state mapping.
- `buildInputState` now routes the NPC's tactical output through
  `ActorIntent`/`ActorIntentAdapter::toInputState`, so all input sources share
  one execution translation. Facing smoothing stays here.

### `src/game/game-cli.cpp`

Added `--npc-utility-selftest` and `--npc-nav-request-selftest`.

### `docs/features/gamemodes/counterstrike.md`

Appended Attempt 5.

## Reasoning

The plan requires utility-scored goals/actions without deleting the old state
machine before equivalence is proven. The utility layer therefore selects a
high-level goal and influences navigation, while `NpcStateMachine` stays the
tactical executor. The navigator is not replaced; `NavigationRequest` is an
adapter over the existing `NpcGoal` so there is exactly one pathfinding owner.
`ActorIntent` is now the live translation path for the NPC's input.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stages 8, 9; Checkpoint 5).
- Skill: `docs/skills/spec-behavior-review-v1.md` — no blocker findings.

## Validation

Build: `BUILD SUCCESS`, no warnings.

Runtime (`mimita.exe`):

```text
[NPC UTILITY SELFTEST]
hysteresis=ok
switch=ok
action_map=ok
PASS

[NPC NAV REQUEST SELFTEST] PASS
[NPC PERCEPTION SELFTEST] PASS
[GAMEMODE SELFTEST] PASS
[ACTOR PRESET SELFTEST] PASS
[CS ROUND SELFTEST] PASS
```

## Human review still needed

- In a live session confirm NPCs pick sensible goals, do not thrash between
  goals, and still navigate/strafe/fire as before.
- Confirm no regression from routing NPC input through the ActorIntent adapter.

## Explicitly not done yet

Objective-fed utility context (TeamBrain, Checkpoint 9), bomb item pickup/drop
(Checkpoint 6), and full action-execution migration off the legacy state
machine.
