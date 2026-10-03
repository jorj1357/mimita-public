# Counter-Strike Checkpoint 4 — ActorIntent boundary and human-like perception

Date: 2026-10-02
EST timestamp: 2026-10-02 22:27:12 EST
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and perception-math evidence are proven. Live in-game
FOV/LOS/memory behavior is NOT performed and remains required.

## Scope

Checkpoint 4 of `docs/specs/20261002plan.md`: generic `ActorIntent` and
human-like NPC perception/memory (Stages 6, 7).

## Pre-existing / external edits (not mine)

The working tree also shows runtime-written user settings changed during the
session: `config/collision.json`, `config/crosshair.json`,
`config/movement.json`, `config/ragdoll.json`, and `config/weapons.json`
(`beam_thickness` 2.0 -> 0.01). None of these were written by this session's
tools. The `weapons.json` change in particular is flagged in the feature record:
the actor-preset override forces the in-memory value, so Counter-Strike runtime
behavior is unaffected, and it is left untouched because it belongs to another
writer.

## Files changed

### `src/actor/actor-intent.h` / `.cpp` (new)

`ActorIntent` (move, lookDirection, aimPoint, jump/dash/crouch/attack/reload/
interact, weaponId, targetActorId) and
`ActorIntentAdapter::toInputState(intent, movementPressed, fallbackFacing)` as
the single intent-to-`InputState` translation owner.

### `src/npc/npc-perception.h` / `.cpp` (new)

- `PerceptionSnapshot`, `MemoryRecord`, `BeliefState`, `PerceptionTuning`.
- `perceive(...)`: FOV + range + LOS gating (LOS supplied by the caller).
- `withinSightCone(...)`: pure planar cone test shared with tests.
- `updateMemory(...)`: refresh on visible; confidence decay + uncertainty growth
  while unseen; expires at `memoryTicks`.
- `buildBelief(...)`: visible target, else decaying memory pseudo-target, else
  no target.
- `predictTargetPosition(...)`: extrapolation plus deterministic per-NPC error.
- `npcPerceptionSelfTest(...)`.

### `src/npc/npc.h`

Added `PerceptionSnapshot perception`, `BeliefState belief`,
`MemoryRecord targetMemory`; included `npc/npc-perception.h`.

### `src/npc/npc.cpp`

- `senseWorld` now takes `world` and drives acquisition through the perception
  module: candidate is hostile + alive, then FOV + range + LOS gate visibility.
  Memory gives an uncertain pseudo-target for the search phase.
- LOS is rate-limited every 5 ticks through the existing `rayTraverseGridCells`
  and `cachedLoSBlocked` is kept in sync from perception (one LOS owner).
- Removed the duplicated LOS block later in `updateOneNpc` and the now-unused
  `SEARCH_TIMEOUT` in this file.
- Reaction delay now falls back to `perceptionReactionTicks` (default 8 ticks)
  when no behavior profile sets one.

### `src/npc/npc-difficulty-config.h` / `.cpp` and `config/npc-difficulty.json`

Added and parsed: `perceptionFovDegrees` (100), `perceptionSightRange` (100),
`perceptionHearingRange` (40), `perceptionReactionTicks` (8),
`perceptionMemoryTicks` (180), `perceptionPredictionSeconds` (0.15),
`perceptionPredictionErrorMeters` (0.4).

### `src/game/game-cli.cpp`

Added `--npc-perception-selftest`.

### `docs/features/gamemodes/counterstrike.md`

Appended Attempt 4.

## Reasoning

Acquisition previously set `hasTarget` from "alive" alone and used LOS only for
firing, so a wall-blocked actor stayed fully known. Perception now owns the
gates and the memory, and the shared ray remains the single LOS implementation.
The `ActorIntent` boundary is added additively; the NPC brain still builds
`InputState` directly and will adopt the adapter in Checkpoint 5 when utility
goals produce intent.

## Documents and skills

- Spec: `docs/specs/20261002plan.md` (Stages 6, 7; Checkpoint 4).
- Skill: `docs/skills/spec-behavior-review-v1.md` — no blocker findings.

## Validation

Build: `BUILD SUCCESS` (new files compiled; `mimita.exe` relinked).

Runtime (`mimita.exe`):

```text
[NPC PERCEPTION SELFTEST]
fov_gate=ok
memory_decay=ok
belief=ok
prediction=ok
PASS

[GAMEMODE SELFTEST] PASS
[ACTOR PRESET SELFTEST] PASS
[CS ROUND SELFTEST] PASS
```

## Human review still needed

- In a live session confirm an NPC cannot see through a wall, respects its FOV
  and 100 m range, remembers a last-known position for ~3 s, applies a reaction
  delay, and does not perfectly track a moving target.
- Confirm no regression in NPC combat/navigation feel.

## Explicitly not done yet

Utility-scored goals/actions with hysteresis, `NavigationRequest`/
`MovementCapabilities`, TeamBrain, and adopting ActorIntent as the live NPC
execution path (Checkpoint 5).
