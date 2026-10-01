# Cold Build Required

Time created: 2026-09-20T15:11:13Z
Time last updated: 2026-09-29T16:21:00Z

Status: COLD-BUILD DEBT

This persistent record tracks intentional cold builds required because a
changed owner cannot yet cross the live-reload boundary. A cold build is not
automatically proof of a user-visible behavior regression.

Related specification:
`docs/features/live-code-development/live-code-development.md`

Related workflow:
`docs/operations/build-and-exe/build-and-exe.md`

Related changelog:
`docs/changelog/2026-09-20/20260920_152300-live-collaboration-foundation-and-commands.md`

---

## Cold-build occurrence 1

Time:
`2026-09-20T15:13:04Z`

Related changelog:
`docs/changelog/2026-09-20/20260920_152300-live-collaboration-foundation-and-commands.md`

### Reason the cold build was required

The v2.1.0 live-collaboration foundation added revision storage, server
arbitration, packet definitions, terminal commands, and JSONL probe support.
The requested result was a complete executable containing those new owners so
the code could be compiled and the existing live-code self-test could run.

### Exact cold owner or boundary

The changed owners were compiled into the EXE by the canonical build:

```text
src/network/server-live-collaboration.cpp
src/network/live-collaboration-client.cpp
src/network/multiplayer-packets.cpp
src/network/multiplayer-tick.cpp
src/network/server-packets.cpp
src/network/packets.h
src/terminal/live-code-commands.cpp
src/debug/structured-log.cpp
src/debug/structured-log.h
```

The hot manifest currently builds replaceable modules from
`src/hot-reload/modules/` and `src/hot-reload/packages/`. It does not make the
network transport, packet dispatch, terminal registration, or logger mechanism
replaceable. Therefore `python devscripts/live-build.py` could not install all
of this change into the running EXE.

### Why this is still cold

The EXE owns the socket transport, packet decoding/dispatch, terminal command
registry, logger file handle, and the stable hot-reload bridge. These are
mechanisms and are currently initialized by the EXE. A live DLL can emit a
generic event only where an existing bridge already exposes that event; it
cannot add a new packet receive case or replace the terminal registry today.

### Result obtained from the cold build

```text
Executable: C:\mimita-priv-v8\mimita-20260920T111304.exe
Build: SUCCESS
Self-test: mimita-20260920T111304.exe --live-code-selftest -> PASS
```

The build compiled 315 files and skipped 355 unchanged files. No running
MiMITA process was present, so no active session was closed or replaced.

### Smallest hot-boundary change needed

Keep the following cold mechanisms:

- socket I/O and packet framing;
- packet byte serialization/deserialization;
- the JSONL file writer and logger thread safety;
- terminal input parsing and command registration.

Move the editable policy behind existing generic seams:

1. Add a generic `live.operation` event/capability whose POD payload contains
   the resource, revision, operation, and content hash.
2. Let hot code validate and decide revision policy through that event.
3. Keep the EXE as a thin packet-to-event and event-to-packet bridge.
4. Let hot code request queue inspection, rollback, and activation through the
   same generic operation surface.
5. Keep `LiveProbe` calls in hot C++ and keep probe configuration in the
   existing hot-reloaded logger configuration path.

After that migration, changing revision rules, queue policy, rollback policy,
or which values a hot gameplay function exposes should require only a DLL
generation. The EXE should not gain resource-specific packet cases.

### How to avoid this cold build next time

Before editing, run:

```text
hotreload classify
```

If a required file is `COLD`, do not edit it as the first implementation step.
Instead:

1. Find an existing generic event, capability, command, resource provider, or
   operation queue.
2. Put the frequently edited rule in a hot module.
3. Keep only the stable mechanism in the EXE.
4. Add a POD payload and a generic bridge if the mechanism has no doorway.
5. Build with `python devscripts/live-build.py`.
6. Confirm the same EXE PID, world, entities, session, and old generation
   remain alive while the new generation activates.
7. Read `events.jsonl` for `compile_started`, `candidate_loaded`, validation,
   activation, generation, and result records.

Use a cold build only when the work changes a genuine kernel mechanism, ABI,
packet framing contract, OS resource, or initial installation boundary. Record
that exact reason here instead of treating the cold build as a normal fix.

### Migration/falsification step

Create a hot `live.operation` self-test that submits two revisions against the
same resource, proves stale-base draft preservation, activates one revision,
and rolls back without deleting the other. Then run a two-process test where a
client proposes the operation through the real packet bridge and both peers
report the same active revision in JSONL. If that passes, the revision-policy
files can be classified HOT; packet transport remains COLD mechanism only.

### Build result

`SUCCESS`

### Human review

The compile and existing live-code self-test passed. Full two-client revision
transport, simultaneous-edit preservation, packet activation, and live
rollback still require runtime/human acceptance.

---

## Cold-build occurrence 2

UTC time: 2026-09-23T03:23:49Z

Related changelog:
`docs/changelog/2026-09-23/20260923_032900-dash-down-dash-no-buffer-hot-edge.md`

### Why the cold build was required

The dash / down-dash press edge lived in the input layer (`src/input/*`,
`src/sim/simulate-tick.cpp`, `src/engine/engine-tick-net.cpp`), which is EXE
code. Removing the 150 ms press buffer and feeding the raw key-down state into
the movement intent changed those EXE files. A hot build alone could not apply
it.

### Exact cold source / boundary

- `src/input/input-frame.h` (added `dashHeld`, `downDashHeld`)
- `src/input/input-poll.cpp` (raw held + raw pressed, no buffer)
- `src/input/input-commands.cpp` (`isDashPressed`/`isDownDashPressed` no buffer)
- `src/sim/simulate-tick.cpp` (intent uses held fields)
- `src/engine/engine-tick-net.cpp` (network uses raw pressed)

### Result needed from the new executable

Rapid Q / Shift presses each produce a fresh edge with no 150 ms merge.

### Why it could not be applied through the live path

The input sampling and the intent write are in the EXE, not in the hot module.

### Smallest change that would make this hot

The hot movement already owns the edge. Only the raw key sampling is EXE-side.
The remaining cold surface is small and stable: the frame's held booleans and the
intent write. A future option is a generic `input.read` raw-state capability so
hot code can compute edges without any EXE edit; the frame fields themselves are
stable and should not need further edits.

### Build result

`SUCCESS` -> `mimita-20260922T232349.exe` (an earlier `mimita-20260922T231811.exe`
was the one-shot buffer variant).

### Human review

Pending. Rapid-tap behavior needs a human playtest.

---

## Cold-build occurrence 3

UTC time: 2026-09-24T03:15:13Z

Related changelog:
`docs/changelog/2026-09-23/20260923_231500-network-hot-tick-damage-projectile-connection.md`

### Why the cold build was required

The networking gameplay migration added new versioned POD envelopes and generic
capabilities that the EXE itself must call for the first time: `network.tick`
(`GameServerTickV1`), `connection.transition` (`GameConnectionTransitionV1`),
`net.projectile-cancel` (`GameProjectileCancelV1`), and append-only extensions to
`ProjectileImpactPolicyV1` and `GameDamageApplicationV1`. Installing a new cold
call site and a new struct layout cannot happen through a live DLL swap; the EXE
must be relinked once. After this build, further edits to the hot headers and hot
modules activate without restarting the process.

### Exact cold source / boundary

- `src/hot-reload/game-api.h` (new/changed POD envelopes)
- `src/hot-reload/hot-damage-application.h` (append-only damage + actor-death result)
- `src/network/server.cpp` (server tick policy path + `log.event` diagnostics)
- `src/network/server-damage.cpp` (suicide/outcome + actor events)
- `src/network/server-projectiles.cpp` (hot cancellation + legacy markers)
- `src/network/multiplayer-packets.cpp` (connection transition + `log.event`)
- `src/network/multiplayer-context.h` (join stage fact)
- `src/network/server.h` (`ServerDamageResult` suicide/score fields)

### Result needed from the new executable

The new envelopes resolve as hot providers, both tick bodies use the shared
`network.tick` policy path, damage reports an explicit suicide with no score, and
the new capability self-tests pass.

### Why it could not be applied through the live path

A running process cannot gain a new call site or read a new struct field. The new
capabilities had to be registered and called by the EXE before the live path
could own the policy.

### Smallest change that would make this hot

None for the installation itself; this is the one-time ABI/installation boundary.
Going forward the policy edits live in `hot-server-tick.h`,
`hot-connection-transition.h`, `hot-damage-application.h`,
`hot-projectile-splash.h`, `hot-projectile-cancel.h`, and the matching
`src/hot-reload/modules/*.cpp`, all of which are hot.

### Build result

`SUCCESS` -> `mimita-20260923T231639.exe` (final; an earlier
`mimita-20260923T231343.exe` built the same tree).

Automated tests (test evidence):

```text
--live-code-selftest        PASS (incl. new server-tick / connection-transition /
                                 projectile-cancel / damage-application checks)
--server-journal-selftest   PASS
--capability-selftest       PASS
--gamemode-hot-selftest     PASS
--hot-authoritative-selftest PASS
--match-policy-selftest     PASS
--hot-combat-selftest       only the 25 known pre-existing animation/phase2 FAILs
```

### Human review

Pending. Live-edit, rocket self-kill, falloff edit, and join retry edit still
require human acceptance.

## Cold-build occurrence 4

UTC time: 2026-09-24T15:26:22Z

Related changelog:
`docs/changelog/2026-09-24/20260924_152622-hot-logging-abi-bootstrap.md`

### Why the cold build was required

Phase 0 of the hot-logging migration adds a new stable logging ABI and a new
generic mechanism that the EXE itself must provide before any hot logging policy
can activate: append-only `GameLogEventV1` fields plus `GameLogFieldV1` /
`GameLogRecordV1`, the `log.append` capability, the `overridable` kernel
capability flag with `overrideCapability`, and the `log.event` bridge that
consults a package override. A new call site, a new capability id, and a new
struct layout cannot be installed through a live DLL swap; the EXE must be
relinked once.

### Exact cold source / boundary

- `src/hot-reload/game-api.h` (append-only logging envelope + field payload)
- `src/hot-reload/generic-runtime.{h,cpp}` (overridable kernel capability)
- `src/live-code/live-behavior.cpp` (`log.event` override bridge, `log.append`)
- `src/debug/structured-log.{h,cpp}` (atomic append + provider record surface)
- `src/hot-reload/capability-selftest.cpp` (P6 overridable checks)

### Result needed from the new executable

`log.event` resolves as before, a package provider for an overridable kernel id
is reachable through `overrideCapability`, the kernel entry stays stable for all
callers, and `log.append` exists as the single safe append mechanism.

### Why it could not be applied through the live path

A running process cannot gain a new capability id, a new bridge branch, or a new
trailing struct field. The ABI and the override mechanism had to be registered
and called by the EXE before a hot provider can own logging policy.

### Smallest change that would make this hot

None for the installation itself; this is the one-time ABI/installation boundary.
After this build, logging policy edits live in the Phase 2 hot provider
(`src/hot-reload/modules/logging-provider.cpp` + `src/hot-reload/hot-logging.h`),
which register through the same `log.event` id and need no cold relink.

### Build result

`SUCCESS` -> `mimita-20260924T112457.exe`.

Automated tests (test evidence):

```text
--capability-selftest   PASS (incl. new P6 overridable checks)
--live-code-selftest    log.event capability resolves; 4 pre-existing "journal"
                        checks FAIL identically on the pre-change build
                        mimita-20260924T100751.exe (not caused by this build)
```

### Human review

Pending. Live provider activation and live logging-policy edits still require
human acceptance.

## Cold-build occurrence 5

UTC time: 2026-09-24T15:43:06Z

Related changelog:
`docs/changelog/2026-09-24/20260924_154306-hot-logging-provider.md`

### Why the cold build was required

Phases 2-3 add the hot logging provider and route the cold `debug::logEvent` API
through it. The provider itself is a hot module, but installing its `logging.flush`
system and the new cold-side bridge (`emitViaProvider`, `appendBody`,
destination-aware `capLogEvent`) requires the EXE to gain those call sites and
the `hot-logging.h` manifest entry once.

### Exact cold source / boundary

- `src/hot-reload/hot-logging.h` (new hot header; must be in `headers`)
- `src/hot-reload/modules/logging-provider.cpp` (new hot module)
- `src/debug/structured-log.cpp` (provider bridge + `appendBody`)
- `src/live-code/live-behavior.cpp` (destination-aware `log.event`, wrapped append)
- `src/hot-reload/hot-modules.json` (header registration)

### Result needed from the new executable

The `log.event` capability resolves to the hot `logging.provider`, `logging.flush`
registers and runs, cold `debug::logEvent` routes through the provider, and
aggregation still collapses repeats while errors stay unaggregated.

### Why it could not be applied through the live path

A running process cannot gain the new cold bridge call sites in
`structured-log.cpp` / `live-behavior.cpp`, nor discover a brand-new hot header
before it is listed in the manifest. The bridge and manifest are cold.

### Smallest change that would make this hot

None for the bridge installation. After this build, logging policy edits live in
`hot-logging.h` and `modules/logging-provider.cpp` and activate live.

### Build result

`SUCCESS` -> `mimita-20260924T114203.exe`.

Automated tests (test evidence):

```text
--capability-selftest        PASS
--live-code-selftest         PASS except 4 pre-existing journal checks (also fail
                             on mimita-20260924T100751.exe); new provider-route PASS
--gamemode-hot-selftest      PASS
--dynamic-lifecycle-selftest PASS
--production-loop-selftest   PASS
--hot-combat-selftest        25 known pre-existing animation/phase2 FAILs
--hot-authoritative-selftest known pre-existing journal-evidence FAIL
```

### Human review

Pending. Live logging-policy edits (field add/remove, destination routing,
throttling, malformed-config last-good) still require human acceptance.

## Cold-build occurrence 6

UTC time: 2026-09-24T19:31:20Z

Related changelog:
`docs/changelog/2026-09-24/20260924_193120-rocket-behavior-hot.md`

### Why the cold build was required

The rocket migration adds hot rocket policy, but the player and NPC fire paths
are cold `weapon-system.cpp` / `npc-combat.cpp` call sites that previously called
`WeaponRocketLauncher::fire` directly. Installing the cold bridge (dispatch a
generic `ToolUsePolicyV1` first, fall back to the legacy launcher only when the
hot router declines) changes those cold call sites, which cannot be activated
through a live DLL swap. The hot rocket owner itself is already hot.

### Exact cold source / boundary

- `src/combat/weapon-system.cpp` (player rocket tool-use bridge)
- `src/npc/npc-combat.cpp` (NPC rocket tool-use bridge)
- `src/hot-reload/hot-projectile.h` (append-only state fields; hot header)
- `src/hot-reload/hot-projectile-event.h` (explode exclude-origin flag; hot)

### Result needed from the new executable

The player and NPC rocket fire paths offer a `ToolUsePolicyV1` to the hot tool
dispatcher before the cold launcher, so the canonical hot rocket owner runs while
the legacy launcher stays only as a fallback.

### Why it could not be applied through the live path

A running process cannot gain a new branch/call site in `weapon-system.cpp` or
`npc-combat.cpp`. The bridge had to be compiled and linked once.

### Smallest change that would make this hot

None for the bridge installation; this is the one-time boundary. After this
build, rocket fire/spawn/movement/collision/explosion/damage/effects/sound/log
edits live in the hot `rocket-tool.cpp` / `hot-projectiles.cpp` and activate
without a cold relink. Future cold builds should only be needed when a brand-new
cold call site is genuinely required; the goal is to keep the interval as large
as possible.

### Build result

`SUCCESS` -> `mimita-20260924T152930.exe`.

Automated tests (test evidence):

```text
--capability-selftest        PASS
--hot-combat-selftest        rocket checks PASS; 25 known pre-existing FAILs
--live-code-selftest         4 pre-existing journal FAILs only
--gamemode-hot-selftest      PASS
--dynamic-lifecycle-selftest PASS
--production-loop-selftest   PASS
--hot-authoritative-selftest known pre-existing journal-evidence FAIL
```

### Human review

Pending. Live-edit and single-sound/destroy acceptance checks still require
human observation.

## Cold-build occurrence 7

UTC time: 2026-09-24T20:59:50Z

Related changelog:
`docs/changelog/2026-09-24/20260924_205950-npc-lifecycle-hot.md`

### Why the cold build was required

The NPC lifecycle migration installs a new generic capability
(`npc.lifecycle`), a new POD envelope (`NpcLifecyclePolicyV1`), origin tagging
on `ServerNpc`, and cold reconciliation/startup call sites. Installing the new
capability id, the per-tick reconciliation branch, and the origin field changes
the EXE and cannot be activated through a live DLL swap.

### Exact cold source / boundary

- `src/hot-reload/game-api.h` (new capability id + POD envelope)
- `src/live-code/live-behavior.cpp` (kernel fallback registration)
- `src/network/server.h`, `server.cpp`, `server-npcs.cpp`, `server-packets.cpp`
  (origin field + reconciliation/startup/apply call sites)

### Result needed from the new executable

The `npc.lifecycle` capability resolves (hot provider or kernel fallback),
startup asks the policy for the plan, the per-tick reconciliation aligns the
automatic NPC set while preserving manual NPCs, and NPC init uses the shared
lifecycle.

### Why it could not be applied through the live path

A running process cannot gain a new capability id, a new per-tick reconciliation
branch, or a new `ServerNpc` field. The bridge had to be linked once.

### Smallest change that would make this hot

None for the bridge installation. After this build, NPC policy edits live in
`modules/npc-lifecycle-policy.cpp` + `hot-npc-lifecycle.h` and `config/weapons.json`
and apply through the next fixed tick without a cold relink.

### Build result

`SUCCESS` -> `mimita-20260924T165449.exe`.

Automated tests (test evidence):

```text
--capability-selftest        PASS (incl. P7 lifecycle checks)
--gamemode-hot-selftest      PASS
--dynamic-lifecycle-selftest PASS
--production-loop-selftest   PASS
--hot-authoritative-selftest known pre-existing journal-evidence FAIL
--live-code-selftest         4 pre-existing journal FAILs
--hot-combat-selftest        25 pre-existing animation/phase2 FAILs
```

### Human review

Pending. Live startup toggle, `npc_spawn` preservation, live count change, and
generation transition still require human observation.

## Cold-build occurrence 8

UTC time: 2026-09-24T22:11:56Z

Related changelog:
`docs/changelog/2026-09-24/20260924_221156-npc-generic-actor-phases-1-3.md`

### Why the cold build was required

The fully-hot NPC migration installs new generic actor components
(`ActorOriginState`, `ActorLifecycleComponent`, `ActorAvatarState`), a new POD
actor-state envelope, three new kernel capability ids (`actor.state.read`,
`actor.state.write`, `actor.destroy`), a new generic destruction event, and
bridge call sites in `server-npcs.cpp`/`server.cpp`/`server-packets.cpp`. New
capability ids, a new struct layout, and new call sites cannot activate through a
live DLL swap, so one cold relink was required.

### Exact cold source / boundary

- `src/hot-reload/game-api.h` (new capabilities + envelopes)
- `src/network/actor-state.{h,cpp}`, `server-context.h`
- `src/live-code/live-behavior.cpp` (kernel capability registration)
- `src/network/server-npcs.cpp`, `server.cpp`, `server-packets.cpp`, `server.h`

### Result needed from the new executable

The generic actor components are authoritative for NPC origin/lifecycle/avatar,
the actor-state envelope resolves for hot code, and the one generic destruction
path records and removes exactly one entity.

### Why it could not be applied through the live path

A running process cannot gain a new capability id, a new component schema, or a
new bridge call site. After this build, NPC generic-state and destruction edits
are hot.

### Smallest change that would make this hot

None for the bridge installation. Going forward, moving NPC position/aim/name/
avatar into generic replication (Phase 4) and NPC behavior into hot systems
(Phase 5) are the remaining cold boundaries; each is a one-time install.

### Build result

`SUCCESS` -> `mimita-20260924T181053.exe`.

Automated tests (test evidence):

```text
--npc-generic-slice-selftest PASS (17/17)
--capability-selftest         PASS
--dynamic-lifecycle-selftest  PASS
--gamemode-hot-selftest       PASS
--production-loop-selftest    PASS
--live-code-selftest          4 pre-existing journal FAILs
--hot-authoritative-selftest  1 pre-existing journal-evidence FAIL
--hot-combat-selftest         25 pre-existing animation/phase2 FAILs
```

### Human review

Pending. Two-client agreement and live behavior edits still require human
observation.

## Cold-build occurrence 9

UTC time: 2026-09-27T15:26:13Z

Related changelog:
`docs/changelog/2026-09-27/20260927_112700-linker-stale-objects.md`

### Why the cold build was required

The reported linker failure was in the executable link and could not be
verified by the live DLL path. A cold build was required to regenerate the
affected server translation unit and prove that the executable links.

### Exact cold source / boundary

- `src/network/server-packet-chat.cpp` — server chat, NPC damage request,
  server command, timeout, and void-death map-based definitions.
- `build.py` — failed compiler cleanup for partial object/dependency artifacts.

### Result needed from the new executable

The server packet and server-loop references must resolve at link time and the
game executable must be produced successfully.

### Why it could not be applied through the live path

The failure was in the cold executable link and the affected symbols are part
of the process-wide server network loop, not a replaceable DLL-only change.

### Smallest change that would make this hot

Keep these server-kernel symbols in the cold executable, but move future
server-policy changes behind the existing hot boundary. The build-system fix
prevents a failed compiler from leaving a partial object that can poison a
later incremental link.

### Build result

`SUCCESS` on 2026-09-27 after recompiling `server-packet-chat.cpp`.

### Human review

Pending. Runtime server startup and chat/NPC command behavior still require
human observation.

## Cold-build occurrence 10

UTC time: 2026-09-27T15:40:39Z

Related changelog:
`docs/changelog/2026-09-27/20260927_114039-physical-hitmarker-aim-point.md`

### Why the cold build was required

The hitmarker draw-call signature and HUD ownership changed in the cold
executable's UI translation units. A canonical build was required to compile
the changed C++ and verify that the executable link still succeeds.

### Exact cold source / boundary

- `src/ui/hitmarker.{h,cpp}` — shared hitmarker presentation API and renderer.
- `src/engine/engine-tick-ui-game-hud.cpp` — gameplay HUD call site.
- `src/engine/engine-tick-ui-replay-hud.cpp` — removed duplicate replay-HUD call.

### Result needed from the new executable

The changed hitmarker API must compile and the game executable must link
without duplicate or unresolved UI symbols.

### Why it could not be applied through the live path

This pass changes a C++ function signature and the cold executable's UI call
graph. The active process uses the existing `.dev/builds/0023/mimita.exe`; a
live DLL reload was not used to activate this cold UI boundary.

### Smallest change that would make this hot

Move the shared hitmarker draw API and gameplay HUD call graph behind the
replaceable UI module boundary, including its exported ABI contract.

### Build result

`SUCCESS` on 2026-09-27. One changed translation unit compiled and the link
completed successfully; active MiMITA processes were left running.

### Human review

Pending. Fire at a world target in physical aim mode and confirm the normal
hitmarker appears at the world-projected crosshair rather than screen center.

## Cold-build occurrence 11

UTC time: 2026-09-27T21:41:58Z

Related changelog:
`docs/changelog/2026-09-27/20260927_214339-canonical-contact-stage-a.md`

### Why the cold build was required

Stage A changes a widely included C++ header (`movement-types.h`), a combat
translation unit (`weapon-viewmodel.cpp`), and several collision translation
units. A canonical build was required to compile the changed C++ and verify the
executable link.

### Exact cold source / boundary

- `src/physics/movement/movement-types.h` — canonical contact fields.
- `src/physics/movement/physics-collision.{h}` and
  `physics-collision-core.cpp` — adapters.
- `src/physics/movement/physics-collision-glb-body.cpp` — producer.
- `src/combat/weapon-viewmodel.cpp` — local-player weapon mesh loader call.
- `src/game/game-cli.cpp` — `--canonical-contact-selftest`.

### Result needed from the new executable

The new canonical-contact selftest must compile and run, and the existing
collision self-tests must remain green with identical final positions/velocities.

### Why it could not be applied through the live path

The change is C++ in cold collision/combat translation units; the running
process uses the existing executable and the collision system is not behind the
replaceable module boundary.

### Smallest change that would make this hot

Move the collision contact vocabulary and producer call graph behind the
replaceable gameplay-module boundary with a stable exported ABI.

### Build result

`SUCCESS` on 2026-09-27. `mimita.exe` linked; `python build.py build-only` then
reported `Nothing changed`.

### Human review

Pending. No default behavior changed; human review is needed once the opt-in
`actorTriangleSolver` path is enabled.

## Cold-build occurrence 12

UTC time: 2026-09-27T21:57:25Z

Related changelog:
`docs/changelog/2026-09-27/20260927_215800-moving-entity-stage-c.md`

### Why the cold build was required

Stage C adds new C++ translation units (`physical-entity.{h,cpp}`), changes the
public solver signature, and changes the player collision struct. A canonical
build was required to compile the new sources and verify the link.

### Exact cold source / boundary

- `src/physics/physical-entity.{h,cpp}` — new entity type and system.
- `src/physics/movement/actor-triangle-solver.{h,cpp}` — entity parameter and
  moving-support carry.
- `src/physics/movement/physics-collision.h` — contact entity/surface fields.
- `src/entities/player.h` — `CollisionState::supportEntityId` /
  `supportVelocity`.
- `src/game/game-cli.cpp` — `--moving-crate-selftest`.

### Result needed from the new executable

The new `--moving-crate-selftest` must run and pass, and all existing collision
self-tests must remain green.

### Why it could not be applied through the live path

The change is C++ in cold collision translation units and the executable's link
script (new object file); the running process cannot load new object code.

### Smallest change that would make this hot

Move the physical-entity type and the actor-triangle solver behind the
replaceable gameplay-module boundary with a stable exported ABI.

### Build result

`SUCCESS` on 2026-09-27. `mimita.exe` linked.

### Human review

Pending. Entities are not yet spawned by a game mode; the proof is deterministic
only.

## Cold-build occurrence 13

UTC time: 2026-09-27T22:49:49Z

Related changelog:
`docs/changelog/2026-09-27/20260927_215800-moving-entity-stage-c.md`

### Why the cold build was required

The `crate_spawn` / `crate_clear` terminal commands add a new translation unit
and register it from `main-systems.cpp`; kinematic advance and entity rendering
were added to the engine tick files. A canonical build was required to compile
and link the new command and call sites.

### Exact cold source / boundary

- `src/terminal/crate-commands.{h,cpp}` — new command registration.
- `src/main-systems.cpp` — registers the commands.
- `src/engine/engine-tick-combat.cpp` — `advanceKinematics(dt)`.
- `src/engine/engine-tick-render.cpp` — `drawPhysicalEntities(camera)`.

### Result needed from the new executable

`crate_spawn` must exist in the terminal and `mimita.exe` must link; the moving
crate self-test must still pass.

### Why it could not be applied through the live path

New object code and a new terminal command registration are in the cold
executable; the running process cannot load them.

### Smallest change that would make this hot

Move terminal command registration and the entity draw/update call sites behind
the replaceable gameplay-module boundary.

### Build result

`SUCCESS` on 2026-09-27. `mimita.exe` linked at 18:49:49 local; the binary
contains the `crate_spawn` and `--moving-crate-selftest` strings.

### Human review

Pending. Spawn a crate in-game and verify it is visible and can be stood on with
`actorTriangleSolver` enabled in `config/collision.json`.

## Cold-build occurrence 14

UTC time: 2026-09-29T01:09:21Z

Related changelog:
`docs/changelog/2026-09-29/20260929_011000-force-punch-knockback-tuning.md`

### Why the cold build was required

The requested Force Punch timing and movement knockback-resistance behavior
change C++ movement and weapon execution owners. A cold executable was needed
to prove the changed fixed-tick code could compile and link.

### Exact cold source / boundary

- `src/combat/weapon-quick-hit.{h,cpp}` — client QuickHit startup gating.
- `src/network/server-attack.cpp` — authoritative QuickHit timeline values.
- `src/physics/movement/movement-types.h` — movement preset field.
- `src/config/movement-config.cpp` — movement JSON parsing/default.
- `src/physics/movement/movement-step.cpp` — impulse consumption scaling.

### Result needed from the new executable

Force Punch should wait for the JSON-configured startup ticks before its
physical contact becomes active, and the selected movement preset should scale
incoming external impulse by the configured resistance multiplier.

### Why it could not be applied through the live path

These owners are currently compiled into the executable rather than exposed as
replaceable hot gameplay-module functions.

### Build result

The changed translation units compiled successfully. The final link failed on
unrelated existing unresolved terminal/config symbols including
`registerWeaponCommands`, `registerDebugCommands`, and
`pollWorldCrosshairConfig`; no running process was closed or replaced. A retry
returned `SUCCESS / Nothing changed`, but did not relink a new executable.

### Smallest change that would make this hot

Expose QuickHit timing policy and movement impulse shaping behind the existing
hot movement/gameplay module boundary, while keeping fixed-tick state ownership
and network authority in the executable.

### Human review

Pending. A successful link and in-game test are still required.

## Cold-build occurrence 15

UTC time: 2026-09-29T12:56:53Z

Related changelog:
`docs/changelog/2026-09-29/20260929_125653-slope-edge-jsonl-tracking.md`

### Why the cold build was required

The canonical JSONL writer and actor-triangle collision event call sites are
compiled C++ owners. A new executable was required to verify that the logger
and collision instrumentation compile, link, and coexist with the existing
collision self-tests.

### Exact cold source / boundary

- `src/debug/structured-log.{h,cpp}` — canonical `events.jsonl` writer,
  universal fields, run directory, and shared-file environment path.
- `src/physics/movement/actor-triangle-solver.cpp` — fixed-tick contact
  before/after response and solve-summary events.

### Result needed from the new executable

The new executable needed to run `--collision-selftest` successfully and be
capable of producing the collision event sequence during a live reproduction.

### Why it could not be applied through the live path

These logger and collision call sites are part of the cold executable. The
currently available live module path does not replace the v9 central logger or
the active actor-triangle solver call sites.

### Build result

The first build caught and was corrected for an instrumentation-only field
name error. The corrected build completed with `Status: SUCCESS`; the produced
`.dev/builds/0381/mimita.exe --collision-selftest` returned
`[COLLISION SELFTEST] PASS`.

### Smallest change that would make this hot

Expose the stable JSONL logging capability and collision event emission through
the existing replaceable gameplay/logger boundary, while keeping the logger
file handle and fixed-tick state cold.

### Human review

Pending. Launch the produced build, reproduce the slope snag, verify the active
`events.jsonl` path, and inspect the correlated before/after/summary records.

## Cold-build occurrence 16

UTC time:
`2026-09-29T13:13:49Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_131349-jsonl-root-path-fix.md`

### Why the cold build was required

The canonical JSONL logger path owner changed so timestamped development
executables write into the v9 repository log root instead of creating a second
log tree under `.dev/builds/<id>/logs`.

### Exact cold source / boundary

- `src/debug/structured-log.cpp` — default log-directory resolution and
  `events.jsonl` destination.

### Result needed from the new executable

The produced executable needed to compile with the corrected path owner and
pass the collision self-test before live slope reproduction.

### Why it could not be applied through the live path

The destination is selected during cold logger initialization in the
executable. The existing live path cannot replace that initialization owner.

### Build result

The build completed with `Status: SUCCESS`. The produced
`.dev/builds/0382/mimita.exe --collision-selftest` returned
`[COLLISION SELFTEST] PASS`.

### Human review

Pending. Launch the new executable from `C:\mimita-v9`, reproduce the slope
snag, and verify that `logger.started` and collision events appear under
`C:\mimita-v9\logs\MM-DD-YYYY\<run>\events.jsonl`.

## Cold-build occurrence 17

UTC time:
`2026-09-29T13:34:20Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_133420-bookmark-command.md`

### Why the cold build was required

The bookmark command and backslash shortcut are cold executable input and
terminal owners. The feature must be present in the launched executable before
the collision reproduction can capture a live bookmark.

### Exact cold source / boundary

- `src/devtools/terminal.cpp` — bookmark command, tick capture, JSONL events,
  optional detail prompt, and confirmation notification.
- `src/engine/engine-tick-state.cpp` — backslash edge-triggered shortcut.

### Build result

The existing `.dev/builds/0394/mimita.exe` was built successfully and passed
the collision self-test. A later full build attempt is blocked by unrelated
pre-existing declaration mismatches in `src/impact/destructible-geometry.*`.

### Human review

Pending. Run the new executable, press `\\` during a surface catch, press
Enter at the prompt, and verify one `bookmark.created` record with the exact
client/server ticks in `events.jsonl`.

## Cold-build occurrence 18

UTC time:
`2026-09-29T15:47:50Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_154750-versioninfo-and-runtime-ownership.md`

### Why the cold build was required

The v9 terminal command registry is owned by the cold executable. Adding
`versioninfo` required a new executable before the running process could report
its own executable and authoritative JSONL path.

### Exact cold source / boundary

- `src/devtools/terminal.cpp` — `versioninfo` registration, process identity,
  tick snapshot, hot state, and JSONL event.

### Build result

Build `0403` completed with `Status: SUCCESS`; its collision self-test passed.

### Human review

Pending. Run `versioninfo` in the active client terminal and paste its output
when reporting the next collision reproduction.

## Cold-build occurrence 17

UTC time:
`2026-09-29T13:24:29Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_132503-aimbody-physical-mode.md`

### Why the cold build was required

A new always-on normal-play physical aim body was added to
`RagdollModeSystem` and hooked into the fixed tick in `simulate-tick.cpp`.
These are cold sources compiled into the executable, so an executable relink was
needed to inspect the behavior.

### Exact cold source / boundary

- `src/ragdoll/ragdoll-mode.cpp` — new aim-body solve path.
- `src/ragdoll/ragdoll-mode-config.cpp` — new physical config parsing.
- `src/sim/simulate-tick.cpp` — activation of the aim body.
- `src/entities/aimbody-config.cpp` — new physical mode.
- `src/ragdoll/physical-aim.h` — new controller header.

### Result needed from the new executable

Compile the new aim-body owner and let a human toggle
`config/aimbody.json` mode to `physical` in-game to observe momentum, sway,
and hitbox alignment.

### Why it could not be applied through the live path

The aim body activation and solve order live in the fixed-tick and ragdoll
owner; changing them requires a relink of the executable, not a hot module.

### Build result

`python build_agent.py` compiled the four changed translation units and
returned `Status: SUCCESS`, return code 0. Produced `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Launch the build, set `"mode": "physical"` in
`config/aimbody.json`, and verify the behavior listed in the changelog.

### Next migration/falsification step

Move the physical-aim controller/config into the live-editable ownership so the
goal, weights, and damping can be tuned without a relink.

### Cold-build occurrence 19

Time:
`2026-09-29T16:21:00Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_162100-collision-cleanup-pass-1-2.md`

The collision cleanup removed compiled C++ translation units, declarations,
and an active source file. This crosses the cold executable boundary because
the live reload layer cannot remove those compiled owners.

### Build evidence

`python build_agent.py` returned `Status: SUCCESS`, return code 0. Build 0409
ran `mimita.exe --collision-selftest` and returned `COLLISION SELFTEST PASS`.

### Human review

Pending. The cleanup did not claim that the slope/corner snag is fixed; launch
the new executable and reproduce the bookmarked slope contact before making
the next collision-owner deletion.

---

### Cold-build occurrence 20

Time:
`2026-09-29T18:17:05Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_141705-config-header-prune.md`

The session removed six unreferenced inline globals from `src/config.h`. The
header is compiled into the cold executable, and the requested build
checkpoint required the canonical executable build.

### Result needed from the new executable

Verify that the repository still compiles and links after the header cleanup.

### Why it could not be applied through the live path

The removed definitions are part of the cold C++ translation-unit boundary;
the live reload path cannot remove C++ declarations from the executable.

### Build result

`python build_agent.py` returned `Status: SUCCESS`, return code 0; 216
translation units compiled and 281 were skipped. Produced executable:
`C:\mimita-v9\mimita.exe`.

### Human review

Not required for the removed, unreferenced debug globals; no gameplay behavior
was intentionally changed. Runtime acceptance of the broader cleanup remains
pending.

### Next migration/falsification step

Repeat the same reference audit for the next candidate only after confirming
that no terminal/configuration surface depends on it; do not remove live
collision owners until their callers are migrated to the shared contract.

---

### Cold-build occurrence 21

Time:
`2026-09-29T19:01:50Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_190150-slope-edge-support-response.md`

The actor-triangle solver response policy changed in cold C++ code. The live
reload path cannot replace this collision owner, so the fix required a new
executable.

### Build result

`python build_agent.py` returned `Status: SUCCESS`, return code 0. Build 0435
ran `mimita.exe --collision-selftest` and returned `COLLISION SELFTEST PASS`.

### Human review

Pending. Reproduce the slope boundary and flat-surface down-dash case with the
new executable. Inspect the bookmark run for `support_edge_promoted` and
`landing_bounce_suppressed` before making another response change.

---

### Cold-build occurrence 22

Time:
`2026-09-29T19:19:05Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_191800-revert-slope-response-attempt-and-fix-bookmark-events.md`

The failed support-response policy was reverted and the bookmark event
category was changed in cold C++ code. A new executable was required; the
existing JSONL logger remains the diagnostic path.

### Build result

`python build_agent.py` returned `Status: SUCCESS`, return code 0. Build 0441
ran `mimita.exe --collision-selftest` and returned `COLLISION SELFTEST PASS`.

### Human review

Pending. Confirm that JSON-enabled bounce remains active and that the next
bookmark appears in the current run's `events.jsonl` as `bookmark.created`.

---

### Follow-up builds (same cold boundary, same session)

Two further python build_agent.py runs were needed while iterating the same
aim-body owner: one failed to compile agdoll-mode.cpp (missing forward
declaration of `quatToRotationVector`) and the next returned `Status:
SUCCESS`, return code 0. Same boundary as this occurrence, no new cold source.

## Cold-build occurrence 18

UTC time:
`2026-09-29T19:40:01Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_132503-aimbody-physical-mode.md`

### Why the cold build was required

A new limb/ragdoll replication packet, server relay handler, and remote
presentation were added. These live in cold network translation units and the
ragdoll owner, so a relink was needed to exercise the path in-game.

### Exact cold source / boundary

- `src/network/packets.h`, `server.cpp`, `server-packets.cpp`,
  `server-packet-handlers.cpp`, `server.h` — new packet and relay.
- `src/network/multiplayer-tick.cpp`, `multiplayer-interpolation.cpp`,
  `multiplayer-packets.cpp`, `multiplayer-context.h` — send/receive/apply.
- `src/ragdoll/ragdoll-mode.cpp`, `src/sim/simulate-tick.cpp`.

### Result needed from the new executable

Compile the new packet path and let a human confirm that a remote client sees
the owner's physical/hybrid/ragdoll limbs, interpolated, at render and hitbox
positions.

### Why it could not be applied through the live path

The packet table, server dispatch, and presentation order are cold owners; a hot
module cannot add a packet type or a fixed-tick apply step.

### Build result

`python build_agent.py` compiled the eight changed translation units and
returned `Status: SUCCESS`, return code 0. Produced `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Two clients: one enters physical/hybrid (or toggles ragdoll) and moves;
the other must see the limbs move with it, interpolated, with no popping.

### Next migration/falsification step

Decide server authority: either keep owner-authoritative limbs (current) or have
the server simulate and validate the body, then feed server hitbox validation
(`server-body-template` / `server-attack`) from the replicated pose.

## Cold-build occurrence 19

UTC time:
`2026-09-29T19:50:04Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_132503-aimbody-physical-mode.md`

### Why the cold build was required

Server-side hit validation was changed to use the replicated limb hitboxes and
the packet grew two new fields. These live in cold network/ragdoll translation
units, so a relink was needed to validate shots against the physical pose.

### Exact cold source / boundary

- `src/ragdoll/ragdoll-replication.h`, `src/ragdoll/ragdoll-mode.cpp`.
- `src/network/packets.h`, `multiplayer-tick.cpp`, `server.h`,
  `server-players.cpp`, `server-packet-handlers.cpp`, `server-attack.cpp`.

### Result needed from the new executable

Compile the pose-carrying packet and validation path, then confirm in a live
two-client match that shooting a physically-posed limb registers damage on that
limb for both the re-trace and the client claim.

### Why it could not be applied through the live path

The packet layout, server validation, and rewind history are cold owners; a hot
module cannot change the wire struct or the server trace.

### Build result

`python build_agent.py` compiled the eight changed translation units and
returned `Status: SUCCESS`, return code 0. Produced `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Two clients: one enters ragdoll/physical/hybrid; the other shoots an
outstretched limb and must see damage on that limb, at the rendered position.

### Next migration/falsification step

Decide whether the server should eventually simulate the body itself (true
authority) rather than trust owner hitboxes, and extend the same replicated
hitboxes to melee/physical-contact and explosion splash paths.

---

### Cold-build occurrence 23

Time:
`2026-09-29T19:50:19Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_195019-restore-angled-slope-bounce.md`

The actor-triangle solver's surface-normal response and collision stress test
changed in cold C++ translation units. A new executable was required to prove
the linked self-test includes the restoration.

### Build result

The first build attempt hit a linker-state failure while the changed collision
objects were being refreshed. A rerun returned `Status: SUCCESS` and produced
the current executable under `.dev/builds/0462/mimita.exe`. Running that
executable with `--collision-selftest` returned exit code 0 and
`[COLLISION SELFTEST] PASS`, including
`angled surface keeps lateral bounce`.

### Human review

Pending. Reproduce a downward dash into a slope and an angled edge in-game.
Confirm that JSON bounce remains enabled, the player rebounds relative to the
surface, and the bookmark-correlated `events.jsonl` records the contact's
`surface_normal`, `response_normal`, and before/after velocity.

### Next migration step

Move this collision response owner into the live-reloadable movement module if
the project requires changing the behavior without a cold executable relink.

## Cold-build occurrence 20

UTC time:
`2026-09-29T20:08:48Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_132503-aimbody-physical-mode.md`

### Why the cold build was required

The server movement authority changed: while a player reports an active ragdoll
pose, `simulatePlayer` now accepts the client root instead of simulating
movement. This is a cold server owner and needs a relink to test.

### Exact cold source / boundary

- `src/network/server-players.cpp` — `simulatePlayer` ragdoll branch.
- `src/ragdoll/ragdoll-replication.h`, `src/network/packets.h`,
  `multiplayer-tick.cpp`, `server-packet-handlers.cpp` — root on the wire.

### Result needed from the new executable

Compile the client-authoritative root path and let a human confirm that a
ragdolled player's server position no longer drifts and no longer resets on
death.

### Why it could not be applied through the live path

The server tick's movement authority and the wire packet layout are cold
owners.

### Build result

`python build_agent.py` compiled the four changed translation units and
returned `Status: SUCCESS`, return code 0. Produced `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Two clients: enter ragdoll, move far, and confirm no position reset /
drift, and that others see the body at the right place.

### Next migration/falsification step

Decide whether the always-on physical/hybrid body should also be
client-authoritative (current) or keep server movement simulation; and smooth
the 15 Hz root on the server so the broadcast root does not step.

## Cold-build occurrence 21

UTC time:
`2026-09-29T20:22:41Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_132503-aimbody-physical-mode.md`

### Why the cold build was required

The client hard-snap threshold default and the reconciliation config wiring
changed, plus the server's accepted-client state on ragdoll exit. These are cold
owners and need a relink to exercise.

### Exact cold source / boundary

- `src/network/movement-validation.h` — `majorCorrectionDistance` default.
- `src/network/multiplayer-reconcile.cpp` — config wiring.
- `src/network/server-players.cpp` — ragdoll-exit accepted state.
- `src/config/networking-config.h` + `config/networkingconfig.json`.

### Result needed from the new executable

Confirm a ragdolled player no longer hard-snaps on leaving ragdoll and that the
snap only occurs past 999 units.

### Why it could not be applied through the live path

The threshold is a compiled default plus a cold config parse read at load.

### Build result

`python build_agent.py` compiled the changed translation units and returned
`Status: SUCCESS`, return code 0. Produced `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Enter and leave ragdoll while moving; confirm no position snap.

### Next migration/falsification step

Decide whether `remote_players.broadcast_source` should move from
`server_sim` to a client-transform mode so other clients render the
client-authoritative root as well.

## Cold-build occurrence 24

UTC time:
`2026-09-29T20:54:55Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_205455-aimbody-lifecycle-devserver.md`

### Why the cold build was required

The hybrid aimbody startup gating, the shared authoritative-transform owner, and
the aim-body rebind are C++ changes in cold translation units
(`simulate-tick.cpp`, `multiplayer-tick.cpp`, `multiplayer-reconcile.cpp`,
`engine-tick-net.cpp`, `ragdoll-mode.cpp`) that require a relink to test the
client spawn lifecycle and hybrid physics against one executable.

### Exact cold source / boundary

- `src/network/local-gameplay-readiness.h` (new).
- `src/network/multiplayer-context.h` readiness declarations.
- `src/network/multiplayer-tick.cpp`, `src/network/multiplayer-reconcile.cpp`,
  `src/engine/engine-tick-net.cpp`, `src/sim/simulate-tick.cpp`,
  `src/ragdoll/ragdoll-mode.{h,cpp}`.

### Result needed from the new executable

With `config/aimbody.json` `"mode": "hybrid"`, confirm no visible fall from the
`(1,5,60)` fallback, the first active position is the authoritative spawn, the
hybrid body aligns immediately, and respawn/teleport do not leave a stale body.

### Why it could not be applied through the live path

The spawn lifecycle gate and the aim-body rebind are compiled into the client
executable and are not part of the live-reloadable module surface.

### Build result

`python build_agent.py` first failed to link with a stale incremental object for
`ragdoll-mode.cpp` (`undefined reference to
RagdollModeSystem::rebindAimToAuthoritativePlayer`). After deleting the stale
objects for the touched translation units and rebuilding, the build returned
`Status: SUCCESS`, return code 0, and produced `C:\mimita-v9\mimita.exe`.

### Human review

Pending. The runtime hybrid startup, spawn alignment, teleport recovery, and
dev-server persistence observations are described in the related changelog.

### Next migration/falsification step

If the aim-body lifecycle reset must be tuned without relinking, move the
rebind trigger and readiness decision into a live-reloadable gameplay module
rather than the fixed client tick.

## Cold-build occurrence 22

UTC time:
`2026-09-29T21:29:13Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_132503-aimbody-physical-mode.md`

### Why the cold build was required

A new per-limb hybrid config value (`arms_follow_force`) and per-part tracking
rate were added to cold ragdoll owners.

### Exact cold source / boundary

- `src/ragdoll/physical-aim.h`, `ragdoll-mode-config.cpp`,
  `ragdoll-mode.cpp`.

### Result needed from the new executable

Confirm that raising `physical.hybrid.arms_follow_force` keeps the arms on
their aimbody/animation orientation while moving fast, so the weapon does not
aim the wrong way.

### Why it could not be applied through the live path

The hybrid update runs in the cold fixed tick.

### Build result

`python build_agent.py` compiled the two changed translation units and
returned `Status: SUCCESS`, return code 0. Produced `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Move fast with hybrid mode and tune `arms_follow_force`.

## Cold-build occurrence 25

UTC time:
`2026-09-29T21:54:27Z`

Related changelog:
`docs/changelog/2026-09-29/20260929_175427-actor-collision-acceleration.md`

### Why the cold build was required

The accelerated actor-collision increment changes `CollisionMeshCache`
(embedded by value in `World`), the actor narrowphase signature, the new
AABB-tree pair traversal, the `collision.json` scratch path, and the
physical-entity contact bridge. These are cold fixed-tick owners and need a
relink to run the collision self-tests and in-game movement against one
executable.

### Exact cold source / boundary

- `src/physics/physics-types.h` (`CollisionMeshCache::triangleAABBs`).
- `src/physics/movement/physics-collision-shared.h`,
  `physics-collision-mesh.cpp`, `physics-collision.cpp`,
  `physics-collision-body.cpp`, `actor-triangle-solver.cpp`,
  `collision-aabb-tree.h` (new).
- `src/map/map-loader-collision.cpp`.
- `src/physics/physical-entity.cpp`.
- `src/config/collision-config.{h,cpp}`, `config/collision.json`.

### Result needed from the new executable

Confirm the collision self-tests still pass (candidate sets, contact parity,
crate behavior) and that the crate/entity path no longer allocates a temporary
`World` per iteration.

### Why it could not be applied through the live path

Collision runs in the cold fixed tick; the cached-AABB layout and the
scratch-buffer narrowphase are compiled into the client/server executable.

### Build result

`python build_agent.py`: one build failed with
`too few arguments to function 'collectActorMeshContactsInto'` because the
default arguments were added after the first compile; after adding the defaults
the build returned `Status: SUCCESS`, return code 0. A later build added the
AABB-tree pair traversal and comparison mode and also returned `Status: SUCCESS`.
Produced `C:\mimita-v9\mimita.exe`. Self-tests run against it with acceleration
enabled: actor-triangle-solve 26/26, collision 19/19, moving-crate 23/23, subgrid
9/9; `tests/collision-aabb-tree-test.cpp` 5/5; comparison-mode smoke 26/26.

### Human review

Pending. Play near the reported cylinder and crate to confirm the FPS drop is
gone or reduced and collision feel is unchanged.

### Next migration/falsification step

Read the `collision.solve.summary` counters (`candidate_pairs`,
`triangle_tests`, `rounded_feature_calls`, `solve_ms`) in a dense map and the
`collision.narrowphase.compare` diff with `actorCollisionComparison` enabled. If
the rounded-feature narrowphase now dominates, optimize that; if the candidate
gather dominates, consider a persistent per-chunk tree.

## Cold-build occurrence 26

UTC time:
`2026-09-30T15:07:52Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_110900-hybrid-arm-sway-correction.md`

### Why the cold build was required

The hybrid arm correction changes the cold ragdoll fixed-tick owner so the
requested arm behavior could be compiled into a new executable.

### Exact cold source / boundary

- `src/ragdoll/ragdoll-mode.cpp`
- `src/ragdoll/ragdoll-mode.h`
- `src/ragdoll/physical-aim.h`

The active hybrid solve order is compiled into the executable and is not
replaceable through the current live module path.

### Result needed from the new executable

Compile the new post-physics torso-relative arm correction so human playtesting
can compare left/right movement sway with `arms_follow_force` tuning.

### Why it could not be applied through the live path

`RagdollModeSystem::updateAim` and `applyHybridSprings` run in the cold fixed
tick. The available live build cannot replace this owner.

### Build result

The first incremental build reported `SUCCESS` with `Nothing changed` and was
not accepted as proof. After removing the exact stale ragdoll object and
dependency files, `python build_agent.py` returned `BUILD SUCCESS`, compiled 1
translation unit, skipped 496, and linked `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Move left and right in hybrid mode and confirm the arms retain the
centered torso-relative aim while still showing editable natural sway.

### Next migration/falsification step

Move hybrid arm presentation policy behind the live behavior boundary if this
setting needs tuning without a cold executable build.

## Cold-build occurrence 27

UTC time:
`2026-09-30T15:29:41Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_110900-hybrid-arm-sway-correction.md`

### Why the cold build was required

The follow-up added the camera-directed arm scale and changed the cold ragdoll
configuration owner and hybrid fixed-tick implementation.

### Exact cold source / boundary

- `src/ragdoll/physical-aim.h`
- `src/ragdoll/ragdoll-mode-config.cpp`
- `src/ragdoll/ragdoll-mode.cpp`
- `src/ragdoll/ragdoll-mode.h`

### Result needed from the new executable

Compile `physical.hybrid.arms_camera_follow` so values `1.0` and `10.0` can be
compared in hybrid mode.

### Why it could not be applied through the live path

The hybrid arm solve and ragdoll config owner are in the cold fixed-tick
executable and are not currently exposed through the live module boundary.

### Build result

`python build_agent.py` returned `BUILD SUCCESS`, compiled 3 translation units,
skipped 494, and linked `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Set `arms_camera_follow` to `10.0`, launch the new executable, move
left/right, and compare the arms against the `1.0` behavior.

### Next migration/falsification step

Move the hybrid arm camera policy behind the live behavior boundary if repeated
tuning is required without cold builds.

## Cold-build occurrence 29

UTC time:
`2026-09-30T15:37:00Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_113700-dev-launch-crate-mode.md`

### Why the cold build was required

The new launch-mode startup-command contract adds a client command-line field
and executes it from the full client main loop after the multiplayer world is
ready.

### Exact cold source / boundary

- `src/main.cpp`
- `src/network/net_mode.cpp`
- `src/network/net_mode.h`

### Result needed from the new executable

Compile the `--startup-command` parser and the one-shot post-load execution
path so launch mode 3 can run `crate_spawn` in the loaded client.

### Why it could not be applied through the live path

The command-line parser and full-client main-loop owner are cold executable
code. The current live module cannot replace process startup or the top-level
loop.

### Build result

`python build.py build-only` returned success and updated the executable. The
resulting executable contains both `--startup-command` and `crate_spawn`.

### Human review

Pending. Start dev-loop launch mode 3, confirm the server uses `funworld3` with
zero NPCs, confirm the client enters the map, and confirm exactly one crate is
spawned after player/world load.

### Next migration/falsification step

If startup commands become frequently edited, move the post-load command policy
behind the live development boundary while keeping CLI parsing as cold startup
mechanism.

## Cold-build occurrence 28

UTC time:
`2026-09-30T15:37:00Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_113700-infinite-dungeon-room-config.md`

### Why the cold build was required

The procedural-world spawn and door policy owner is compiled into the current
executable. The requested behavior needed the new JSON fields and runtime
reload call sites compiled into that owner.

### Exact cold source / boundary

- `src/procedural/procedural-world.cpp`
- `src/procedural/procedural-world.h`
- `src/procedural/procedural-world-client.cpp`

The server fixed-tick procedural owner and client procedural-world tick are not
currently replaceable through the available live build entry.

### Result needed from the new executable

Compile JSON-owned player spawn selection, rotation-aware door collision, and
timestamp-based door reload for the running Infinite Dungeon Slayer mode.

### Why it could not be applied through the live path

The documented `devscripts/live-build.py` entry is absent in this checkout, and
the current procedural files are built through the cold executable path.

### Build result

`python build_agent.py` returned `BUILD SUCCESS`, compiled 10 translation units,
skipped 487, and linked `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Run `pwsids`, verify the first-room spawn and room progression, then
edit the room door JSON during the active session and observe the live change.

### Next migration/falsification step

Expose the procedural-world policy and reload boundary through the stable live
module, or provide the missing live-build entry, before the next tuning pass.

## Cold-build occurrence 31

UTC time:
`2026-09-30T15:56:50Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_110900-hybrid-arm-sway-correction.md`

### Why the cold build was required

The camera-follow target needed to use the equipped weapon's actual local
grip-to-muzzle axis instead of assuming the generic local `+Y` axis.

### Exact cold source / boundary

- `src/ragdoll/ragdoll-mode.cpp`

### Result needed from the new executable

Compile the automatic weapon-axis correction so the arms point along the
camera-forward line instead of approximately 90 degrees toward the ground.

### Why it could not be applied through the live path

The hybrid arm solve is part of the cold fixed-tick executable and is not
currently exposed through the live module boundary.

### Build result

`python build_agent.py` returned `BUILD SUCCESS`, compiled 1 translation unit,
skipped 496, and linked `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Check camera-forward, camera-up, camera-down, and left/right movement
at `arms_camera_follow: 10.0`.

### Next migration/falsification step

If this axis correction needs more tuning, move the camera-follow arm policy
behind the live behavior boundary so it can be adjusted without a cold build.

## Cold-build occurrence 32

UTC time:
`2026-09-30T16:07:26Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_110900-hybrid-arm-sway-correction.md`

### Why the cold build was required

Attempt 5 changes the cold hybrid ragdoll arm solver so high
`arms_camera_follow` values blend toward each arm's captured default
aimbody/animation pose instead of a weapon-derived camera orientation.

### Exact cold source / boundary

- `src/ragdoll/ragdoll-mode.cpp`
- `src/ragdoll/ragdoll-mode.h`
- `src/ragdoll/physical-aim.h`
- `config/ragdoll.json`

### Result needed from the new executable

Compile the general all-weapons arm target blend while preserving the existing
physical torso, legs, movement, and weapon attachment behavior.

### Why it could not be applied through the live path

The hybrid arm fixed-tick solver is part of the cold executable and is not
currently exposed through the live module boundary.

### Build result

`python build_agent.py` returned `BUILD SUCCESS`, compiled 1 translation unit,
skipped 496, and linked `C:\mimita-v9\mimita.exe`.

### Human review

Pending. The dedicated review space is in
`docs/regressions/2026-09-30/hybrid-arm-camera-aim-REG.md`.

## Cold-build occurrence 33

UTC time:
`2026-09-30T16:09:09Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_110900-hybrid-arm-sway-correction.md`

### Why the cold build was required

The arm-only `arms_mode` setting and the attempt-5 target blend changed the
compiled aimbody configuration owner and hybrid ragdoll solver.

### Exact cold source / boundary

- `config/aimbody.json`
- `src/entities/aimbody-config.h`
- `src/entities/aimbody-config.cpp`
- `src/ragdoll/ragdoll-mode.cpp`
- `src/ragdoll/ragdoll-mode.h`
- `src/ragdoll/physical-aim.h`
- `config/ragdoll.json`

### Result needed from the new executable

Allow only the arms to choose default-style or hybrid-style behavior while
keeping the rest of the body physical and preserving all weapon-specific
attachment transforms.

### Why it could not be applied through the live path

The hybrid arm fixed-tick solver and the aimbody configuration owner are not
currently exposed through the live module boundary.

### Build result

`python build_agent.py` returned `BUILD SUCCESS`, compiled 7 translation units,
skipped 490, and linked `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Use the review space in
`docs/regressions/2026-09-30/hybrid-arm-camera-aim-REG.md`.

## Cold-build occurrence 30

UTC time:
`2026-09-30T15:50:35Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_110900-hybrid-arm-sway-correction.md`

### Why the cold build was required

The camera-directed arm target needed a correction in the cold hybrid ragdoll
solve so it uses the weapon's actual local forward axis and attachment frame.

### Exact cold source / boundary

- `src/ragdoll/ragdoll-mode.cpp`
- `src/ragdoll/ragdoll-mode.h`

### Result needed from the new executable

Compile the corrected camera-follow target so `arms_camera_follow: 10.0`
aims the weapon and arms along camera forward instead of sideways or backward.

### Why it could not be applied through the live path

The hybrid arm solve is part of the cold fixed-tick executable and is not
currently exposed through the live module boundary.

### Build result

`python build_agent.py` returned `BUILD SUCCESS`, compiled 13 translation
units, skipped 484, and linked `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Launch the new executable with hybrid mode and verify camera-forward,
camera-up, camera-down, and left/right movement behavior.

### Next migration/falsification step

If the arm camera behavior needs repeated tuning, move this policy behind the
live behavior boundary so it can be adjusted without a cold build.

## Cold-build occurrence 34

UTC time:
`2026-09-30T20:40:00Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_164000-procedural-teleport-handoff.md`

### Why the cold build was required

The procedural client room-slot owner and server movement-validation teleport
handoff are cold executable owners. The requested correction needed both owners
compiled into the executable.

### Exact cold source / boundary

- `src/procedural/procedural-world-client.cpp`
- `src/network/movement-validation.cpp`

### Result needed from the new executable

Make the client render the same room slots as the server and keep the server at
the exact authoritative teleport target until the new transform epoch is
acknowledged.

### Why it could not be applied through the live path

The available live build path does not replace these cold owners.

### Build result

`python build_agent.py` returned `BUILD SUCCESS`, compiled 5 translation units,
skipped 498, and linked `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Run `pwsids` in the new executable and verify the room, spawnpoint,
teleport handoff, and normal movement after acknowledgement.

### Next migration/falsification step

Move the procedural room-slot presentation and teleport handoff policy behind
the stable live boundary if further live tuning is required.

## Cold-build occurrence 35

UTC time:
`2026-09-30T20:43:20Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_164000-procedural-teleport-handoff.md`

### Why the cold build was required

The procedural NPC membership preservation is owned by the cold server
executable. The room-clear correction needed that owner compiled into the
executable.

### Exact cold source / boundary

- `src/network/server-npcs.cpp`

### Build result

`python build_agent.py` returned `BUILD SUCCESS`, compiled 1 translation unit,
skipped 502, and linked `C:\mimita-v9\mimita.exe`.

### Human review

Pending. Verify room 1 clears immediately after its one NPC dies and room 2
spawns two NPCs.

## Cold-build occurrence 36

UTC time:
`2026-09-30T21:01:24Z`

Related changelog:
`docs/changelog/2026-09-30/20260930_164000-procedural-teleport-handoff.md`

### Why the cold build was required

The authoritative teleport, fixed-tick invulnerability, and client shield
rendering are cold executable owners. The requested behavior needed those
owners compiled and linked into the executable.

### Exact cold source / boundary

- `src/network/server-packets.cpp`
- `src/network/server-gamemode.cpp`
- `src/network/server-damage.cpp`
- `src/procedural/procedural-world-client.cpp`

### Build result

The clean build completed successfully, and the final incremental build
returned `BUILD SUCCESS`, compiled 1 changed translation unit, skipped 502,
and linked `C:\mimita-v9\mimita.exe`. The preceding clean/integration build
compiled the other new owners before this final correction.

### Human review

Pending. Test the exact-room teleport, 60-tick protection/countdown, and
room-owned door replacement in a live Infinite Dungeon Slayer session.

## Cold-build occurrence 37

UTC time:
`2026-10-01T13:43:29Z`

Related changelog:
`docs/changelog/2026-10-01/20261001_134329-destructible-crater-penetration.md`

### Why the cold build was required

The destructible penetration fix lives in the shared projectile sweep and the
boolean wrapper, both cold executable owners. The self-tests (`--destructible`,
`--moving-crate`, `--destruction-replication`) are compiled into the executable,
so proving the fix required a relink; there is no live-reload path for these
translation units in this build.

### Exact cold source / boundary

- `src/combat/projectile-simulation.cpp` (projectile sweep narrowphase)
- `src/impact/boolean-mesh.cpp` (cutter tessellation)
- `src/physics/physical-entity.{h,cpp}` (shared cached entity surface)
- `src/network/server-projectiles.cpp` (server projectile entity query)
- `src/combat/client-collision-world-view.cpp` (client projectile entity query)
- `src/impact/destructible-selftest.cpp` (full-chain self-test)

### Build result

`python build.py build-only` completed successfully and relinked
`C:\mimita-v9\mimita.exe`. Each incremental run recompiled the changed
translation units (verified by object mtimes) and linked a fresh executable.
`mimita.exe --destructible-selftest`, `--moving-crate-selftest`, and
`--destruction-replication-selftest` all reported PASS after the final build.

### What the cold build was needed to prove

That the corrected `closestPointOnTriangle` vertex-C region test stops the
phantom sphere/triangle overlap at a crater's entry plane, so repeated shots on
one crater monotonically deepen it and eventually penetrate the crate, without
regressing actor contacts, moving crates, or destruction replication.

### Why live activation was impossible

The projectile kernel, boolean wrapper, and entity query are statically linked
into the executable; the project's live path does not hot-replace these cold
owners.

### Smallest change that would make this hot

Expose the projectile narrowphase and boolean wrapper as replaceable game
modules under the live-code-development contract so a kernel fix can be loaded
without a full relink. Until then, a cold build is required for these owners.

### Next migration/falsification step

Measure whether the shared entity surface cache and AABB tree survive a runtime
stress pass (many moving entities + repeated cuts) without cache thrash; if they
do not help, the tree query can be falsified and removed.
