# Avatar movement command and live animation evidence

## Scope

Fixed the live `agent.move` path used by the dev-loop scenario. The command
was accepted and logged, but the modern multiplayer tick was reading keyboard
bindings directly and never consuming the scheduled movement. The fixed-step
simulation also needed to see the same scheduled axes so movement animation
could enter `walk`.

## Source evidence

- `src/main-systems.cpp` now schedules camera-relative side/forward axes rather
  than pre-rotated world coordinates.
- `src/input/input-poll.cpp/.h` exposes a peek/consume boundary. The fixed-step
  input frame peeks the axes for physics and animation; `engine-tick-net.cpp`
  consumes them once for the outgoing multiplayer input.
- `src/engine/engine-tick-net.cpp` emits bounded `agent.move.applied` events
  with applied axes, position, and velocity.
- The unrelated legacy `src/network/client.cpp` walking-bit experiment was
  removed; the live path is owned by `engine-tick-net.cpp` and
  `multiplayer-tick.cpp`.

## Build evidence

- Published through the persistent dev loop.
- Fresh executable: build `2238`.
- Dev-loop status: `stale=False`, run `20261013_234636`.
- The daemon remained running; it was not replaced by a competing manual
  executable.

## Runtime evidence

- Journal: `logs/2026-10-13/20261013_234636/events.jsonl`.
- Scenario: `monster_avatar_spawn_walk_offsets_v1` from
  `devscripts/scenarios/readmewhat.md`.
- `scenario.finished` reported `execution_complete=true` after all 12 steps.
- `agent.move.applied` contained `move_x=0`, `move_y=1` and nonzero velocity.
- Stabber movement snapshot: position changed from approximately
  `[-1256.586, 526.028, 381.532]` to `[-1252.114, 452.914, 391.962]`, with
  `active_animation=walk` and `current_animation=walk`.
- Boomer movement snapshot also reported `active_animation=walk` and
  `current_animation=walk`.
- Avatar snapshots retained `body_frame_policy=player_canonical` and
  `body_transform_source=model` for the monster avatars.

## Remaining acceptance boundary

The journal proves command delivery, applied movement, animation state, and
avatar transform sampling. It does not replace a human visual check of the
running window for the final arm/weapon appearance. The next visual check
should use the same fresh run and scenario rather than a manually launched
executable.
