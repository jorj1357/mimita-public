# NPC lifecycle behind one hot policy

Time: 2026-09-24T20:59:50Z

## Scope

Moved NPC startup, spawn, respawn, health, weapon selection, and lifecycle
decisions behind one hot NPC policy (`npc.lifecycle`). After one cold bridge
build, changing NPC C++ or `config/weapons.json` applies to the running server
without another cold build.

## Changes

- `src/hot-reload/game-api.h`
  - `GAME_CAP_NPC_LIFECYCLE` + `GAME_SIG_NPC_LIFECYCLE`.
  - `GameNpcLifecycleReasonV1`, `GameNpcOriginV1`, `NpcLifecyclePolicyV1`
    (POD, no STL/pointers) and `GameNpcLifecycleFn`.
- `src/hot-reload/hot-npc-lifecycle.h` (new)
  - Shared fallback policy used when no hot provider is registered; preserves
    the pre-migration enable/count/clamp behavior and adds reconcile/respawn.
- `src/hot-reload/modules/npc-lifecycle-policy.cpp` (new)
  - Reads `config/weapons.json` `npc_lifecycle` live (malformed -> last-good);
    owns startup count, placement intent, health, starting weapon/loadout,
    difficulty, respawn enable, and reconciliation.
- `src/live-code/live-behavior.cpp`
  - Kernel-registered `npc.lifecycle` fallback so the capability always
    resolves; a hot package overrides it.
- `src/network/server.h`
  - `ServerNpc.origin` (`GameNpcOriginV1`) and `ServerNpc.startingWeapon`.
  - `ServerGameOverrides.startupNpcsEnabled/startupNpcCount` as live facts.
- `src/network/server.cpp`
  - Startup NPC plan now comes from the hot policy; launch options are recorded
    as live facts for reconciliation instead of permanently deciding the count.
  - Applies policy health and starting weapon.
- `src/network/server-npcs.cpp`
  - `reconcileAutomaticNpcs` (origin-aware) runs every fixed tick: creates
    missing automatic NPCs, removes only stale automatic NPCs, preserves manual
    and gamemode NPCs.
  - `adoptNewServerNpcs` applies policy health + starting weapon before the
    shared `finalizeServerNpcSpawn` init.
- `src/network/server-packets.cpp`
  - `npc_spawn` tagged origin MANUAL.
- `src/hot-reload/hot-modules.json`
  - `hot-npc-lifecycle.h` added to `headers`.
- `config/weapons.json`
  - New `npc_lifecycle` block (startup disabled by default, respawn, loadout,
    health).
- `src/hot-reload/capability-selftest.cpp`
  - P7 lifecycle-fallback contract checks (reconcile creates/removes automatic;
  respawn preserves the same id).

## Validation

Cold build (canonical `python build_agent.py`):

```text
Status: SUCCESS
Executable: mimita-20260924T165449.exe
```

Automated tests (test evidence):

```text
--capability-selftest        PASS (incl. new P7 NPC lifecycle checks)
--gamemode-hot-selftest      PASS
--dynamic-lifecycle-selftest PASS
--production-loop-selftest   PASS
--hot-authoritative-selftest known pre-existing journal-evidence FAIL (1)
--live-code-selftest         known pre-existing journal FAILs (4)
--hot-combat-selftest        25 known pre-existing animation/phase2 FAILs
```

Startup log confirms the hot provider resolves:

```text
[CAPABILITY_RESOLVED] provider=npc.lifecycle id=5188915802548897776
```

Build evidence is separate from runtime evidence. The task's live checks
(disable startup live, `npc_spawn`, change count live, generation transition)
require a running server and human observation and were NOT performed here.

## Honest boundary

- Health-bar presentation is excluded from this migration as requested.
- The respawn delay remains owned by `net.respawn`; `npc.lifecycle` owns only
  whether an NPC respawns, so there is one delay owner.
- Concurrent unrelated work: `src/hot-reload/modules/ragdoll-solve.cpp` had an
  invalid negative `uint32_t` priority literal that broke the build; it was
  changed to `0u` to unblock validation (flagged, not my feature).
- Live-edit and generation-transition acceptance remain pending human review.
