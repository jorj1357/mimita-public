# Ragdoll solver migration: component-driven domain, grabs, deterministic corpses

- EST timestamp: 2026-09-12 16:37:00 EDT (UTC 2026-09-12T20:37:00Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW` (source + syntax verification; cold build deferred by a running game)

## What was implemented

### Component-driven solver + editable-rate domain
- `src/ragdoll/ragdoll-solver.h/.cpp` (new): `Ragdoll::Solver::solveSubstep`
  reads the canonical limb components into a `RagdollBody` workspace, runs
  integrate -> joints/rotation limits -> grabs -> world/self collision ->
  depenetrate -> settle, and writes the dynamic state back. `solveJoints`,
  `solveRotationLimits`, `solveGrabs`, `selfCollision` are now free functions;
  the duplicate bodies in `RagdollModeSystem` were deleted.
- `RagdollModeSystem::update` applies input/look motors once per 60 Hz tick,
  publishes components, then advances the `ragdoll.solver`
  `Sim::DomainScheduler` domain and syncs back; the 60 Hz gameplay tick is
  unchanged.
- `Ragdoll::SolveParams` moved to `ragdoll-components.h`; `RagdollEntities`
  aliases it (one owner). Stiffness scales joint beta, damping relaxes limb
  velocity per substep, iterations/gravity feed the solver.
- Editable rate primitives in `config/ragdoll.json` /
  `RagdollModeConfigData`: `solver_hz` (120), `snapshot_send_interval_ticks`
  (3), `replicate_corpses` (true), all hot-reloadable.

### Entity layer
- `RagdollEntities::syncToBody` (inverse of `syncFromBody`),
  `bindLimbCount`, and `applySnapshot` reconstructing an unknown owner from the
  first snapshot (`reconstructFromSnapshot`) with stable ids on later frames.

### Grabs as entity constraints
- `GrabComponent` gained `limbEntity`, `handLocalAnchor`, `targetEntity`,
  `strength`; `RagdollGrabState` gained `targetPart`, `targetLocalAnchor`,
  `strength`. `Solver::solveGrabs` resolves a same-body target as a two-body
  point constraint, else pins to the world point.
- `RagdollModeSystem::grabLimb/releaseGrab/grabTargetPart` expose
  entity-to-entity grabbing. `RagdollGrabStatePacket` is serialized inside
  `RagdollStatePacket` and applied on receive.

### Deterministic corpses + replication
- Corpse seed `splitmix64(FNV1a(worldSeed, actorId, ownerId, deathTick,
  deathEventId))` replaces `std::rand()`; the tumble streams from the seed.
- `PACKET_CORPSE_SPAWN` (72) + `CorpseSpawnPacket`; `mpSendCorpseSpawn` sends on
  the local corpse serial, the server relays, and peers
  `noteNetworkDeath`/`consumeNetworkDeath` so the remote `spawnCorpse` uses the
  shared identity.

### Config cleanup
- Removed the runtime `config/ragdolldeath.json` rewrite in
  `community-match-client.cpp`; the gamemode override is now the in-memory
  `RagdollDeathConfig::setEnabledOverride/clearEnabledOverride`, cleared on
  `CommunityMatchClient::reset`.

### Telemetry + inspector
- `MIMITA_TELEMETRY_SCOPE("RagdollSolverSubstep")` and per-owner
  `EntityCounters` per substep; replicated snapshots add network counters.
- `CreationMode::describe` prints limb/joint/grab/root ragdoll fields.

## Evidence

- `python build_game_dll.py --output build/verify-ragdoll-game.dll` -> success
  (rebuilds the 4 hot sources; no hot module was otherwise changed).
- `-fsyntax-only` clean (mingw64 g++, `-std=c++17`, pch): `ragdoll-mode.cpp`,
  `ragdoll-solver.cpp`, `ragdoll-entities.cpp`, `ragdoll-mode-config.cpp`,
  `ragdoll-slice-selftest.cpp`, `multiplayer-packets.cpp`, `multiplayer-tick.cpp`,
  `server-packets.cpp`, `server.cpp`, `multiplayer-interpolation.cpp`,
  `multiplayer-shots.cpp`, `simulate-tick.cpp`, `community-match-client.cpp`,
  `creation-mode.cpp`, `ragdoll-death-config.cpp`.
- `static_assert`s for `RagdollStatePacket` (<=800) and `CorpseSpawnPacket`
  (<=96) hold (validated by the TUs that include `packets.h`).
- `--ragdoll-slice-selftest` extended (source compiles) with syncToBody,
  entity-to-entity grab + snapshot codec, remote reconstruction with stable ids,
  and network-death identity.

## Cold build (deferred)

The solver domain wiring, new packet, runtime override, telemetry call sites,
and inspector fields are EXE-owned mechanisms and need one bootstrap cold build.
It was deferred because a `mimita.exe` process is running (pid 9724); the
invariant forbids killing it. Live/hot edits remain available after that build.

## Runtime proof (pending, not claimed)

- `mimita.exe --ragdoll-slice-selftest` run and result.
- In-game: toggle ragdoll, `config/ragdoll.json` edit changes `solver_hz` live,
  two-client limb/grab replication, two clients derive the same corpse from one
  death, `PACKET_CORPSE_SPAWN`/`PACKET_RAGDOLL_STATE` seen on the wire.

## Known limitations

- Entity-to-entity grabs are solved within one body; cross-actor grabs are
  carried as the replicated moving world anchor (no cross-body two-body solve),
  as designed for this slice.
- Remote ragdoll interpolation is **not** wired in this pass: received snapshots
  write the limb components directly, and remote rendering still reads the
  network player skeleton rather than the components, so there is no delayed
  render buffer to feed yet. Reusing `multiplayer-interpolation.cpp` is the next
  step once remote ragdolls render from components.
- Cross-client death identity depends on the `PACKET_CORPSE_SPAWN` identity;
  local-only deaths still seed from local tick/event (0 if unavailable).
- Corpse entity/component binding reuses the alive binding path at
  `solver_hz/60` substeps; corpses do not yet back onto their own root/limb
  component set for inspection.
- `joint_stiffness`/`coneLimitDeg`/`configIndex`/`mNextCorpseSerial` dead config
  was not deleted in this pass (out of scope to avoid touching unrelated edits).

## Files

New: `src/ragdoll/ragdoll-solver.h`, `src/ragdoll/ragdoll-solver.cpp`,
`docs/changelog/2026-09-12/20260912_203700-ragdoll-solver-migration.md`.

Changed: `src/ragdoll/ragdoll-components.h`, `src/ragdoll/ragdoll-entities.h/.cpp`,
`src/ragdoll/ragdoll-mode.h/.cpp`, `src/ragdoll/ragdoll-mode-config.h/.cpp`,
`src/ragdoll/ragdoll-slice-selftest.cpp`, `src/config/ragdoll-death-config.h`,
`src/network/packets.h`, `src/network/multiplayer-context.h`,
`src/network/multiplayer-packets.cpp`, `src/network/multiplayer-tick.cpp`,
`src/network/server-packets.cpp`, `src/network/server.cpp`,
`src/network/multiplayer-interpolation.cpp`, `src/network/multiplayer-shots.cpp`,
`src/network/community-match-client.cpp`, `src/editor/creation-mode.cpp`,
`config/ragdoll.json`, `docs/architecture/live-development/ragdoll-live-network.md`.

## Pre-existing edits preserved

All unrelated working-tree changes were preserved; only ragdoll/network/config
files above were edited.
