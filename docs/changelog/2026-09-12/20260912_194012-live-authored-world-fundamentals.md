# Live authored world fundamentals: held-fire intent, creation mode, telemetry

- EST timestamp: 2026-09-12 15:40:12 EDT (UTC 2026-09-12T19:40:12Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW` (source staged; one cold build deferred by a running game)

## What was implemented

### 1. Batched held-fire intent (full-auto rocket)
- `packets.h`: `PACKET_FIRE_INTENT_REQUEST = 70`, `FireIntentAction`
  (START/STOP/HEARTBEAT), `FireIntentPacket` (intentId, weapon, action, start/end
  tick, count, seed, aim).
- `server.h`: `HeldFireState` per player; declarations for
  `handleFireIntentPacket` / `tickHeldFireIntents`.
- `server-attack.cpp`: the handler opens/updates/closes the window, and the
  per-tick driver emits **one authoritative projectile per gameplay tick** via
  the existing `handleGenericProjectileAttack` (distinct id/fireSerial per
  rocket). Ammo is server-authoritative; the window closes at zero.
- Hot hook: `GAME_EVENT_FIRE_INTENT` + `FireIntentPolicyV1` in `game-api.h`;
  `LiveBehavior::dispatchFireIntent`; `modules/rocket-behavior.cpp` handles it
  (edit to change the schedule live).
- Client `engine-tick-combat.cpp`: automatic projectile weapons send START,
  heartbeat every ~6 ticks, STOP on release; no per-rocket packet.
- `config/weapons.json`: rocket launcher `fire_mode` set to `automatic`
  (damage value left as the developer's `1111`).

### 2. Creation mode + local authoring fork
- `src/editor/creation-mode.*`: `modecreate` state, raycast picking via existing
  `selectWorldTriangle`, `WorldObject` entities, terminal-first inspection text,
  and non-destructive patch ops (duplicate/transform/delete) with a deterministic
  `forkHashFor(baseHash, patch)`.
- `src/terminal/creation-commands.*`: `modecreate`, `create_inspect`,
  `create_copy`, `create_paste`, `create_delete`, `create_move`,
  `create_rotate`, `create_scale`, `create_patch`.
- Journal events with base/fork hashes and process identity.

### 3. Dedicated telemetry
- `src/telemetry/telemetry.*`: rolling per-scope aggregates (calls, calls/sec,
  inclusive/self/avg/max ns, last tick, generation, hash) and generic per-entity
  counters. `MIMITA_TELEMETRY_SCOPE` sampling primitive.
- `src/terminal/telemetry-commands.*`: `telemetry [top n]`, `telemetry_entity`.
- `src/sim/domain-scheduler.*` (from the previous round) remains the home for
  additional simulation domains.

## Evidence

- `python build_game_dll.py` -> success (4 hot sources), `mimita.exe` untouched.
- `-fsyntax-only` clean for `telemetry.*`, `telemetry-selftest.cpp`,
  `telemetry-commands.cpp`, `live-behavior.cpp`, `server-attack.cpp`,
  `server-packets.cpp`, `multiplayer-packets.cpp`, `engine-tick-combat.cpp`,
  `creation-mode.cpp`, `creation-selftest.cpp`, `creation-commands.cpp`,
  `main-systems.cpp`, `game-cli.cpp`.
- `--telemetry-selftest` -> `[TELEMETRY SELFTEST] PASS` (standalone).
- `--project-selftest` and `--phase456-selftest` unchanged and passing.
- `--hot-authoritative-selftest` now also checks the hot fire-intent dispatch.
- `--creation-selftest` compiles; standalone execution is deferred (its TU pulls
  the hot-reload system), so it runs via the EXE after the cold build.

## Cold build (deferred)

The new packet, server held-fire driver, hot fire-intent event, creation mode,
and telemetry are EXE-owned mechanisms and need one bootstrap cold build. It was
deferred because two `mimita.exe` processes are running; the invariant forbids
killing them.

## Known limitations

- Client prediction for held fire is server-authoritative-only in v1 (no
  per-rocket prediction/reconciliation yet).
- Map edits are data/fork operations; visible GLB geometry movement is not
  re-baked in this pass. Base objects can be inspected, duplicated, hidden, and
  deleted in the fork; moving baked sub-meshes is deferred.
- Capabilities and telemetry are data-first; enforcement/visualization later.

## Remaining work for component editing

`ComponentSchemaRegistry`-driven edit API; per-component fork patch types
(set/add/remove); `setentity <id> <component>.<field> <value>` commands; undo/redo
through the patch chain; GUI gizmo consuming `WorldInspector`/fork; copyable
map-object components (`MeshRef`/`MaterialRef`/`ColliderRef`).

## Docs

- New `docs/architecture/live-development/live-authored-world.md`.
- Changelog: this file.

## Pre-existing edits preserved

The developer's `rocketDirectDamage: 1111` and all unrelated working-tree changes
were preserved.
