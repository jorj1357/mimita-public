# Ragdoll live + network slice

- EST timestamp: 2026-09-12 16:07:09 EDT (UTC 2026-09-12T20:07:09Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW` (source staged; cold build deferred by a running game)

## What was implemented

### Persistent ragdoll entities/components
- `src/ragdoll/ragdoll-components.h`: `LimbComponent`, `JointComponent`,
  `GrabComponent`, `RagdollRootComponent` in `namespace Ragdoll`.
- `src/ecs/entity-types.h`: new `EntityDomain::RagdollLimb` (5) so each limb has
  a stable `EntityId`.
- `src/ragdoll/ragdoll-entities.*`: `bind` projects a `RagdollBody` into limb
  entities plus root/grab entities; `syncFromBody`, `setGrab`, `solveParams`,
  `setSolveParams`, `writeSnapshot`, `applySnapshot`, `limbEntity`,
  `limbCount`, `entityCount`.

### Hot solver policy
- `game-api.h`: `GAME_EVENT_RAGDOLL_SOLVE` + `RagdollPolicyV1`.
- `live-code/live-behavior.*`: `dispatchRagdollPolicy`.
- `modules/rocket-behavior.cpp`: handles the ragdoll policy (identity default;
  edit stiffness/damping/iterations/gravity live).

### Replication
- `packets.h`: `PACKET_RAGDOLL_STATE` (71), `RagdollLimbStatePacket`,
  `RagdollStatePacket` (owner actor, up to 16 limbs).
- `multiplayer-context.h` / `multiplayer-packets.cpp`:
  `mpSendRagdollSnapshot`.
- `multiplayer-tick.cpp`: binds/syncs the local ragdoll and sends a snapshot
  every 3 ticks; applies received snapshots.
- `server-packets.cpp`: relays snapshots to other peers.
- `sim/simulate-tick.cpp`: projects the offline ragdoll into entities (owner 1).

### Migrations applied
- Removed the singleton-only path by adding a persistent representation and a
  stable limb identity. `RagdollModeSystem::aliveBody()` accessor added.
- Solver policy now flows through the generic event/behavior seam instead of
  being cold-only.

## Evidence

- `python build_game_dll.py` -> success (4 hot sources).
- `-fsyntax-only` clean for `ragdoll-entities.cpp`, `ragdoll-slice-selftest.cpp`,
  `live-behavior.cpp`, `multiplayer-packets.cpp`, `multiplayer-tick.cpp`,
  `simulate-tick.cpp`, `server-packets.cpp`, `game-cli.cpp`.
- `--ragdoll-slice-selftest` staged (compiles); runs via the EXE after the cold
  build.
- Existing `--live-code-selftest` / `--hot-authoritative-selftest` /
  `--entity-slice-selftest` unchanged.

## Cold build (deferred)

The new packet, ragdoll entity/component components, hot ragdoll event, and
server relay are EXE-owned mechanisms and need one bootstrap cold build. It was
deferred because a `mimita.exe` process is running; the invariant forbids
killing it.

## Known limitations

- The legacy `RagdollModeSystem` still runs the actual solver; the components are
  the canonical persistent representation but the solver does not yet read them
  each substep. Full solver extraction to the high-rate `ragdoll.solver` domain
  is the next step.
- Received snapshots apply only where the owner ragdoll is bound locally; remote
  bodies are not yet reconstructed from snapshots.
- Corpse replication is not covered; only the alive local ragdoll replicates.
- `std::rand()` corpse spawn determinism is not yet fixed.

## Remaining ragdoll migration (next)

1. Drive the solver from `Sim::DomainScheduler` `ragdoll.solver` (e.g. 120-240 Hz)
   and read/write the limb/joint components directly.
2. Reconstruct remote ragdolls from snapshots (bind on first snapshot).
3. Replicate corpses with the same snapshot type.
4. Seed corpse spawn deterministically.
5. Migrate grab to entity-to-entity constraints.
6. Delete the dead fields (`joint_stiffness` wiring, `coneLimitDeg`,
   `configIndex`, `mNextCorpseSerial`) and stop writing `config/ragdolldeath.json`
   from network overrides.

## Files

New: `src/ragdoll/ragdoll-components.h`, `ragdoll-entities.h/.cpp`,
`ragdoll-slice-selftest.h/.cpp`,
`docs/architecture/live-development/ragdoll-live-network.md`.

Changed: `src/ecs/entity-types.h`, `src/hot-reload/game-api.h`,
`src/hot-reload/modules/rocket-behavior.cpp`, `src/live-code/live-behavior.h/.cpp`,
`src/ragdoll/ragdoll-mode.h`, `src/network/packets.h`,
`src/network/multiplayer-context.h`, `src/network/multiplayer-packets.cpp`,
`src/network/multiplayer-tick.cpp`, `src/network/server-packets.cpp`,
`src/sim/simulate-tick.cpp`, `src/game/game-cli.cpp`.

## Pre-existing edits preserved

All unrelated working-tree changes were preserved.
