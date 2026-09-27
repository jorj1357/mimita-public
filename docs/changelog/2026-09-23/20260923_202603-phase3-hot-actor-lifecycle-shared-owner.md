# Phase 3: shared hot actor lifecycle owner (players + NPCs) + spawn-protection schema fix

Date (UTC): 2026-09-23T20:26:03Z
Status: implemented; build and selftests verified; live two-client run pending

## Scope

Phase 3 of the hot server/networking migration: make the generic
`actor.lifecycle` envelope the ONE hot lifecycle owner for player initial spawn,
respawn, reconnect, and NPC respawn, so player and NPC lifecycle share one
policy. Also fixes a latent spawn-protection bug found while wiring it.

## Changes

1. **Player lifecycle through the generic envelope** (`server-players.cpp`
   `completeAuthoritativeSpawn`): after the existing spawn-placement policy, the
   actor lifecycle envelope is dispatched with stable identity, life generation,
   reason (initial join / respawn), position/yaw, health, and avatar name. A hot
   respawn veto (`respawnRequested == 0`) is honored. This is the single owner
   for initial spawn, respawn, and reconnect.

2. **Functional hot lifecycle handler** (`modules/actor-lifecycle-boundary.cpp`):
   preserves identity and life generation, keeps the actor alive for the new
   life, and arms generic spawn protection on the actor entity. Previously a
   no-op boundary proof.

3. **NPC respawn uses the same envelope** (`server-npcs.cpp` `respawnServerNpc`):
   the same `actor.lifecycle` dispatch is added alongside the existing placement
   policy, so players and NPCs share one lifecycle owner.

4. **Latent bug fixed — spawn-protection schema**: `SpawnProtection` was written
   through the generic dynamic-component store (`lifecycle-policy.cpp`,
   `rocket-behavior.cpp`, and now the lifecycle handler) but no schema was ever
   registered, so `DynamicComponentStore::write` rejected every write (size
   lookup fails) and spawn protection was silently inactive. Registered the
   schema in `actor-lifecycle-boundary.cpp`. This makes the existing
   `actor.lifecycle-policy` spawn protection actually apply.

5. **Self-test** (`--actor-lifecycle-selftest`, new): hot package active;
   envelope handled; identity + life generation preserved; actor alive for the
   new life; spawn protection armed on the actor entity; respawn through the same
   owner; determinism.

## Evidence

- Source changes: `server-players.cpp`, `server-npcs.cpp`,
  `modules/actor-lifecycle-boundary.cpp`, `actor-lifecycle-selftest.{h,cpp}`,
  `game-cli.cpp`.
- Build (source/build evidence):
  - Hot DLL: `python build_game_dll.py` -> `DLL build success`.
  - Cold EXE: `python build_agent.py` -> `Status: SUCCESS`
    (`mimita-20260923T162538.exe`). A running `mimita.exe` was not touched.
- Automated tests (test evidence):
  - `--actor-lifecycle-selftest` -> PASS (7 checks).
  - `--live-code-selftest`, `--server-spatial-authority-selftest`,
    `--generation-bootstrap-selftest`, `--lagcomp-history-selftest`,
    `--npc-entity-selftest`, `--npc-actor-state-selftest`,
    `--counterstrike-selftest`, `--match-policy-selftest` -> PASS.
- Runtime evidence: none. No live two-client session was observed.
- Human acceptance: pending.

## Not done

- The proposal's "thin adapters" cleanup (deleting redundant cold decisions in
  `server-players.cpp`/`server-damage.cpp`/`server-projectiles.cpp`) is not done;
  this phase adds the shared owner without removing the cold fallbacks.
- Hot-owned transient-state serialization into versioned POD migration buffers
  at activation is not implemented.
- `--hot-combat-selftest` animation/phase2 failures remain concurrent/unrelated.
