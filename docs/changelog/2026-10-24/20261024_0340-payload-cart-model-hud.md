// 2026-10-24T03:40:00Z
/* purpose
* Record the Payload vertical-slice cart model, route/checkpoint, NPC objective,
* and HUD work from this AI session.
* Separate source/build/test/runtime evidence from human-visible acceptance.
* Do not claim pre-existing uncommitted changes as this session's work.
* Do not rewrite historical changelogs.
*/

# Task

- Task ID: payload-cart-model-hud
- Summary: Payload cart GLB visuals, 3x3x2 gameplay footprint, editable checkpoint route, NPC cart objective, and payload HUD (intermission/countdown/teams/timer/overtime).
- Status: Implemented and built; runtime data-path verified; final human-visible HUD/model check pending.
- Date, time, timezone: 2026-10-24T03:40:00Z
- Branch: 20261011zt
- Base commit: 2103321b
- Final commit: uncommitted working tree

# Pre-existing changes

- The checkout already contained many modified and untracked files before this session.
- This changelog claims only the Payload-related files listed under implementation.
- `git status --short` was inspected before writing this record; unrelated edits were preserved.

# Requested behavior

- Use `assets/objects/things/gamemode-objects/payload-payload.glb` for the cart.
- Scale the cart to about 2 m tall, 3 m wide, and 3 m long.
- Let the cart origin move toward authored checkpoints that content authors can place/edit in map JSON.
- Show intermission, 3-2-1-GO, red-vs-blue team information, a top timer, and overtime as seconds.milliseconds.
- Keep overtime authoritative in server ticks and format it for display.
- Follow the Payload specification.

# Specification alignment

- Specification: `docs/specs/gamemodes/payload.md`.
- Cart origin/interaction box: cart origin is the anchor; interaction box is a configured transformed offset.
- Overtime: server stores integer ticks; HUD shows seconds.
- HUD: timer at top, intermission/countdown, team scoreboard, objective status.
- The authored route is 2,213 world units. At 0.2-1.2 units/s plus two 300 s checkpoints, six attackers cannot finish in one round. The user chose to leave this as a tuning note rather than change pacing in this task.

# Exact implementation changes

## `src/network/server-gamemode.cpp`

- Added the cart GLB path and `renderModel`/`collisionBox` flags to the spawned cart entity.
- Replicated cart HUD fields in `DuelStatePacket`: overweight/blocked/rolling-back state, nearby attacker/defender counts, overtime ticks, distance/progress, and cart position.
- Added Player and NPC cart collision/carry resolution each authoritative Payload tick.
- Added Payload NPC objective context: both teams receive the cart position, with a small target offset so NPCs enter the interaction box.
- Kept cart movement, healing, checkpoints, overtime, and win/loss in the server owner.

## `src/gamemode/payload-runtime.h/.cpp`

- Added previous cart transform and cart delta for full-delta carry.
- Added capsule-vs-box support/push resolution, including carrying actors standing on the cart.
- Added selftest coverage for support, carry, side push, and far-actor rejection.
- Route/checkpoint movement remains data-driven from map JSON.

## `src/physics/physical-entity.h/.cpp`

- Added `renderModel` and `collisionBox` flags.
- Added a cached GLB prop renderer that scales the authored model to the entity's gameplay extents.
- Payload cart renders the GLB; its collision remains the configured gameplay box.

## `src/network/physical-entity-replication.cpp`

- Added late-join replay of physical entity spawns/cuts/despawns per player so the cart reaches clients that join after the entity exists.
- Carries the render-model and box-collision flags in the spawn packet reserved bits.

## `src/network/multiplayer-physical-entities.cpp`

- Applies model path, render-model, and box-collision flags.
- Emits `physical_entity.mirror_spawned` diagnostics for replicated props.

## `src/network/packets.h`

- Protocol 45 appends Payload HUD fields to `DuelStatePacket`.

## `src/network/community-match-client.h/.cpp`

- Replicates and exposes Payload HUD state to the client.

## `src/engine/engine-tick-ui-overlays.cpp`

- Added Payload red/blue team labels with local-team highlighting.
- Added Payload cart progress/status and overtime `seconds.milliseconds` display.
- Colored team tags red/blue in the Tab scoreboard.

## `config/gui/gamemode-meta-gui.json`

- Added the `payload` HUD section: intermission, PAYLOAD title, top timer, score line, red/blue team labels, overtime, cart progress/status, countdown/GO, and result text.

## `config/maps/payload1.json`

- Cart size changed to 3x3x2.
- Interaction box changed to 4.5x4.5x3.0 (150% of the 3x3x2 cart).
- Route/checkpoint positions remain editable JSON data.

## `src/network/server-ice.cpp`

- Re-resolves player references after packet handlers to avoid iterator/player-reference invalidation when a player joins or is defer-removed.

## `src/terminal/crate-commands.cpp`

- Added `physical_entity_list` for inspecting server and replicated physical entities.

# Diagnostics

- `payload.configured`: route, checkpoints, distance, speed, overtime.
- `payload.cart-spawned`: entity id and gameplay size.
- `payload.cart-state`: attacker/defender counts, distance, overtime, movement state.
- `payload.checkpoint`, `payload.overtime-start`, `payload.overtime-expired`, `payload.finished`.
- `physical_entity.mirror_spawned`: client mirror id, network id, source kind, extents, position.
- These are change-edge or one-time events, not per-tick spam.

# Validation

- Build: dev-loop build 3010 success (debug).
- Selftests:
  - `mimita.exe --payload-selftest` PASS.
  - `mimita.exe --team-brain-selftest` PASS.
  - `mimita.exe --npc-utility-selftest` PASS.
  - `mimita.exe --destruction-replication-selftest` PASS.
- Runtime evidence (build 3007, run `20261024_032221`):
  - `payload.cart-spawned` size `[3,3,2]`.
  - `physical_entity.mirror_spawned` with `source_kind=1` (GLB) at the cart origin.
  - `round.active` reached; cart push produced `payload.cart-state` advancing events.
  - Server and client stayed alive while the GLB cart entity was present.
- Earlier runtime evidence (builds 2937/2951/2964):
  - Payload config, cart spawn, push, and mirror replication observed.
- Human-visible acceptance: not claimed. The final Payload HUD and GLB appearance still require direct visual review.
- Latest-run note: the dev-loop client intermittently failed to join the coordinator room after rebuilds, so the last build's visible HUD pass was not captured.

# Open decisions

- Route pacing remains a tuning note per user direction: 2,213 units is too long for the spec speed and current round/checkpoint bonuses.
- Checkpoint authoring is JSON-only (map `payload.route`); no in-game placement command was added.
