# Generic NPC actor authority (fully-hot NPC migration Phases 1-3)

Time: 2026-09-24T22:11:56Z

## Scope

Phased live-safe migration of NPCs onto the generic ECS/replication/lifecycle
systems. This pass implements Phases 1-3: generic actor component authority,
generic identity/storage projection, and generic lifecycle/destruction. Phases
4-6 (generic replication, hot behavior, deleting the typed NPC path) remain.
Nothing was deleted; superseded code is marked LEGACY with a dated note.

## Changes

- `src/hot-reload/game-api.h`
  - `GameActorStateV1` POD envelope (identity, transform, velocity, health,
    lifecycle generation, origin, avatar hash, tool, team/role/profile, target;
    no STL/pointers) + `actor.state.read` / `actor.state.write` capabilities.
  - `GAME_CAP_ACTOR_DESTROY` + `GAME_CAP_ACTOR_DESTROY_*` reasons +
    `GAME_EVENT_ACTOR_DESTROY` generic destruction record.
- `src/network/actor-state.{h,cpp}`
  - New generic components: `ActorOriginStateV1`, `ActorLifecycleComponentV1`,
    `ActorAvatarStateV1` (all `GAME_NET_ALL`), plus read/write helpers and schema
    registration.
- `src/network/server-context.h`
  - `serverDestroyActor` generic destruction entry.
- `src/live-code/live-behavior.cpp`
  - Kernel capabilities `actor.state.read`, `actor.state.write` (project the
    generic components into `GameActorStateV1` and apply writes through the
    shared player/NPC helpers), and `actor.destroy`.
- `src/network/server-npcs.cpp`
  - `destroyNpcActor` (one generic destruction path, records facts, dispatches
    `GAME_EVENT_ACTOR_DESTROY`, removes exactly one entity) and
    `serverDestroyActor`.
  - Reconciliation reads origin from the generic component (`originOf`);
    automatic/manual distinction no longer depends on the mirror.
  - `rebuildServerNpcMap` is a derived projection that writes
    origin/lifecycle/avatar components; legacy mirror-origin seed fallback marked
    LEGACY (dated 2026-09-24 18:09 UTC).
  - Creation paths (startup/reconcile/adopt) seed `ActorOriginState` +
    lifecycle at birth.
- `src/network/server.cpp`, `src/network/server-packets.cpp`
  - Startup and manual `npc_spawn` write the generic origin/lifecycle components
    at creation.
- `src/network/server.h`
  - Dead `ServerNpc` fields documented as LEGACY with a dated delete note.
- `src/hot-reload/npc-generic-slice-selftest.{h,cpp}` (new) + `--npc-generic-slice-selftest`
  CLI registration in `src/game/game-cli.cpp`.

## Validation

Cold build (canonical `python build_agent.py`):

```text
Status: SUCCESS
Executable: mimita-20260924T181053.exe
```

Automated tests (test evidence):

```text
--npc-generic-slice-selftest PASS (17/17: identity, generic components, envelope,
                                 respawn generation, origin distinction, destroy)
--capability-selftest         PASS
--dynamic-lifecycle-selftest  PASS
--gamemode-hot-selftest       PASS
--production-loop-selftest    PASS
--live-code-selftest          known pre-existing journal FAILs (4)
--hot-authoritative-selftest  known pre-existing journal-evidence FAIL (1)
--hot-combat-selftest         25 known pre-existing animation/phase2 FAILs
```

Build evidence is separate from runtime evidence. The task's multiplayer and
live-reload acceptance checks (two clients agreeing on ids/health/generations/
avatars; hot-edit behavior live) require a running server and human observation
and were NOT performed here.

## Honest boundary

- Phases 4-6 are not done: NPC position/aim/name/avatar still replicate through
  the compact snapshot `ENTITY_NPC` branch; generic dynamic replication already
  carries health/team/role/profile/tool. NPC-specific packet types remain.
- Three NPC id allocators still coexist; unification is Phase 2's storage goal
  but the typed `NpcSystem` body still owns simulated ids.
- The typed `Npc`/`NpcSystem` simulation remains the per-tick engine; deleting it
  is the long pole and is explicitly deferred.
- `serverDestroyActor` currently destroys the generic entity; the mirror/body are
  reconciled on the next tick (documented), so a hot reentrant destroy is safe.
- Concurrent unrelated edits to `server-npcs.cpp`/`server.h`/config were
  preserved and merged around, not overwritten.
