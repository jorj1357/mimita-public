# Unified NPC movement executor (Sandbox = Counter-Strike)

Date: 2026-10-04
EST timestamp: 2026-10-04 19:30:26 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, pure-test, and in-binary runtime evidence are proven. No live
Counter-Strike match was played this session; human acceptance remains required.

## Scope

Make the shared NPC movement path explicit and reusable so Counter-Strike uses
exactly the same movement/wall-avoidance/stuck-recovery executor as Sandbox,
while mode-specific code only chooses the target, objective, and goal. Rage2
remains combat/decision tuning only.

This builds on the uncommitted Attempt-16 targeting/wall-recovery work (which
was preserved, not reverted).

## Current-state finding

There was already one movement owner: `NpcSystem::updateOneNpc`
(`src/npc/npc.cpp`), reached by both `NpcSystem::update` (offline/Sandbox sim,
`src/sim/simulate-tick.cpp`) and `NpcSystem::updateOneWithTarget` (server path
used by Sandbox-online and Counter-Strike, `src/network/server-npcs.cpp`). The
work was to formalize the boundary, add a generic executor selector, consolidate
stuck recovery, and prove parity — not to add a second executor.

## Files changed

### New `src/npc/npc-movement-context.h` / `.cpp`
- `NpcMovementExecutor` (`sandbox_shared` default, `surface_navigation`,
  `direct`) + `npcMovementExecutorName` / strict `npcMovementExecutorFromString`.
- `NpcMovementContext` (mode-selected `target`, `objective`, optional
  `goalOverride`), the typed boundary between mode decisions and the shared
  executor.
- `npcMovementExecutorSelfTest`.

### `src/npc/npc-movement-policy.h` / `.cpp`
- `NpcMovementPolicy.movementExecutor` (default `sandbox_shared`); parsed
  strictly from `movement_executor` inside `npc_behavior`.

### `src/gamemode/match-roles.cpp`
- Top-level actor-preset `movement_executor` is parsed and overrides the
  `npc_behavior` value; unknown values reject the preset.

### `src/npc/npc.h`
- `Npc::lastMovementExecutor` (diagnostics/tests).
- Public shared entry `NpcSystem::updateOneNpc(Npc&, const World&, const NpcMovementContext&, float)`.

### `src/npc/npc.cpp`
- `activeMovementExecutor(npc)`: resolves the executor from the actor preset,
  independent of whether the movement policy is configured (hot-reloadable).
- `update` / `updateOneWithTarget` now build an `NpcMovementContext` and call the
  shared entry; the `Player&` parameter of `updateOneNpc` is replaced by the
  context.
- `updateOneNpc` applies `context.objective` only when `hasObjective` (so the
  existing TeamBrain producer is unchanged), honors `context.goalOverride`, and
  selects the navigator settings by executor:
  - `sandbox_shared` → shared navigation defaults (exact Sandbox behavior),
  - `surface_navigation` → the preset `navigation` block,
  - `direct` → direct steering.
- Consolidated the policy and legacy stuck-recovery branches into one shared
  recovery: choose the most open local direction, optionally jump (still policy
  gated), request a repath, and never keep pushing the blocked direction
  (`[NPC NAV] ... recovery=open_turn_repath`).

### `config/actor-presets/counter_strike.json`
- Added top-level `"movement_executor": "sandbox_shared"`.

### Tests
- New `tests/npc-movement-executor-test.cpp` (pure).
- New `--npc-movement-executor-selftest` in `src/game/game-cli.cpp`.

## Reasoning

The movement math already lived in one place; the missing piece was an explicit
typed input so mode code cannot accidentally own movement. Resolving the
executor from the actor preset keeps it generic and hot-reloadable, and
`sandbox_shared` guarantees Counter-Strike and Sandbox run identical movement
code and tuning. Objective/target selection stays mode-specific.

## Spec / regression alignment

- `docs/architecture/player-npc-systems/player-npc-systems.md`: players and NPCs
  share one exact movement path; no parallel implementation.
- `docs/specs/movement/movement.md`, `docs/specs/networking/networking.md`: never
  redefine movement; all paths call the same functions.
- `docs/specs/gamemodes/gamemodes.md`: modes are data/rules; shared runtime owns
  lifecycle and movement.
- `docs/regressions/2026-10-04/counterstrike-npc-targeting-movement-REG.md`: the
  CS nearest-hostile target selection and open-direction stuck recovery are
  preserved; the unified executor is the architectural follow-through.

## Validation

### Build

```text
python build.py build-only
BUILD SUCCESS  (background dev-loop also recompiled changed objects; mimita.exe relinked)
```

### Pure test

```text
build/npc-movement-executor-test.exe -> PASS (15 checks)
```

### In-binary selftests (all EXIT=0)

```text
--npc-movement-executor-selftest  PASS
  ok  counter_strike preset uses movement_executor=sandbox_shared
  ok  CS NPC runs the shared Sandbox executor
  ok  Sandbox NPC runs the shared Sandbox executor
  ok  both modes reach the same movement executor
  ok  CS NPC moved / Sandbox NPC moved
  ok  objective context reaches the shared utility context
  ok  objective context does not change the movement executor
--npc-targeting-selftest         PASS
--npc-navigation-selftest        PASS
--npc-search-behavior-selftest   PASS
--npc-movement-policy-selftest   PASS
--npc-radar-selftest             PASS
--npc-nav-request-selftest       PASS
--aim-fov-selftest               PASS
--spawn-tag-selftest             PASS
--cs-round-selftest              PASS
--gamemode-selftest              PASS
--counterstrike-acceptance-selftest PASS
```

- `--actor-preset-selftest`: FAIL only on the pre-existing weapon-value
  assertions (revolver 35/6/36, shotgun 20/6/32), unrelated to this change.

### Runtime / human

Not performed. The live Counter-Strike match below is still required.

## Human review still needed

On `dust2cyberiav4` with one CT human + CT NPCs vs T NPCs:
1. CT and T NPCs leave spawn.
2. NPCs attack the human and enemies on the other team; never shoot teammates.
3. NPCs do not repeatedly push walls or doorways; ramps and corners trigger the
   shared open-direction recovery.
4. A target known only through radar behind a wall does not force the NPC into
   the wall.
5. NPC behavior remains Rage2 combat tuning.

## Remaining issues from map collision geometry (not NPC logic)

None identified this session. If a doorway/ramp still traps NPCs during the live
test, capture the world triangle geometry at that spot; a blocked corridor with
no valid surface connection is a map-collision issue, not a movement-executor
issue.
