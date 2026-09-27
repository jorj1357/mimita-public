// 09 12 2026
/* purpose
* Define the Version 3 kernel/hot-runtime boundary for MiMITA.
* Explain how gameplay behavior is hot-replaceable while the EXE stays alive.
* Explain the generic event/behavior path that replaced feature-specific hooks.
* this file DOES NOT define individual gameplay formulas
* this file DOES NOT replace the live-development invariant or the hot ABI
* this file DOES NOT permit relinking a running executable
*/

# Hot kernel and hot gameplay

## Model

```text
Entity
+ Components
+ Events
+ Behaviors
+ Hot-reloadable code references
```

The EXE is a small stable kernel. Gameplay behavior is hot-loaded and
replaceable. The mental model is:

```text
THE ENTITY STAYS
THE BEHAVIOR CHANGES
```

## Kernel / cold

- process lifetime, memory/lifetime
- EntityId allocation and sparse component storage (`src/ecs/`)
- generic event queue and dispatch (`GameEventV1`)
- fixed-tick scheduler
- network transport and reliable gameplay event replication
- server/client authority infrastructure
- physics query mechanism
- render/audio/input capability handles
- hot module loader and stable runtime ABI (`src/hot-reload/game-api.h`)
- persistent world/session state
- explicit authoritative safety limits (separate from gameplay tuning)

## Hot gameplay

- rocket/grenade/weapon damage policy
- explosion falloff and knockback policy
- projectile motion policy
- NPC intent and actor decisions
- gamemode rules
- presentation tuning
- future gameplay systems

Hot module sources live in `src/hot-reload/modules/` and compile into
immutable `mimita-live-gNNNNNN.dll` generations.

## Generic event/behavior boundary

The kernel emits events with plain-data, kernel-owned, mutable payloads. Hot
behaviors read/write the payload and set `handled`. The kernel applies the
result. No new EXE call site is needed to add behavior for an existing event
type.

```text
kernel fills base values
    -> GameEventV1 { typeId, sourceEntity, targetEntity, projectileEntity, tick, payload }
    -> hot behavior (onEvent) reads/writes payload, sets handled
    -> kernel applies the result
```

First implemented event: `GAME_EVENT_DAMAGE_POLICY` with payload
`DamagePolicyV1` (`baseDamage`, `outDamage`, knockback, source, entity ids).
The kernel resolves it in `src/network/server-damage-policy.cpp` and applies
the result; `config/weapons.json` remains the base data.

### Delegation rule

> A gameplay feature that needs a new cold call site is a
> `HOT_RELOAD_BOUNDARY_VIOLATION`.

The preferred response is to generalize the runtime interface (add behavior for
an existing event / add data the generic system consumes), not to add a
one-off feature bridge. New event and component types are intended to become
data/schema-driven so they do not require new EXE call sites.

## Rocket vertical slice

```text
player emits FireIntent
-> weapon behavior spawns rocket entity (Transform, Velocity, Collider, Owner, Projectile)
-> kernel tick moves the projectile
-> kernel detects collision/lifetime and emits the explosion path
-> kernel emits GAME_EVENT_DAMAGE_POLICY per victim with base values
-> hot behavior sets outDamage (for example 999999)
-> kernel applies the returned value to Health (no gameplay clamp)
-> authoritative replication sends the result
```

Editing `src/hot-reload/modules/rocket-behavior.cpp` changes the authoritative
damage live, with no `weapons.json` edit and no EXE relink.

## Safety limits

The kernel owns an explicit authoritative damage limit
(`serverAuthoritativeDamageLimit`), separate from gameplay tuning. On the
private development branch it is `0` (unlimited) so behavior proofs can use
large values. Public servers set a finite bound; untrusted client damage claims
remain capped separately in `server-packet-handlers.cpp`.

## Generation agreement protocol (seed)

`PACKET_CODE_GENERATION` (`src/network/packets.h`) is the generic, extensible
seed of the future multiplayer READY/switch-tick protocol:

```text
direction: 0 = client report, 1 = server announce
phase:     0 = status, 1 = READY, 2 = SWITCH at switchTick (reserved)
generation, codeHash (low 64 bits), moduleSetHash (reserved), switchTick
```

- The client reports its active generation/hash every ~30 client ticks.
- The server stores the report per player and announces its own generation/hash
  (and a provisional `switchTick`) whenever it activates a new generation.
- The client stores the server's announcement and warns when its generation
  differs.

Each process keeps independent runtime state but shares source content. The
packet is deliberately wide (phase, switchTick, module-set hash) so a full
agreement protocol can extend it without a new packet type. A later phase adds
READY handshakes, a shared switch tick, and module-set hashing.

## Why a cold build is required (and what the next phase must remove)

The hot loader, the live journal and its identity fields, the notification
schema, the authoritative damage-policy call site, and the server live-code
lifecycle are **EXE-owned mechanisms**. A running process cannot gain a new call
site, a new struct field, or a new lifecycle hook. So this observability/retry
work needs one cold build to install. That is the same reason the earlier
server bridge needed one.

This is the exact problem the next phase must reduce:

- every new mechanism currently lives in cold code and forces a cold build;
- gameplay policy should not. The next phase moves behavior behind the hot ABI
  (behavior bindings, component capabilities, kernel event queue) and grows a
  generic kernel so future changes are data/behavior, not new C++ call sites;
- long-term, a bytecode/IR or data-driven evaluator makes even new mechanisms
  hot, so the executable only changes when the process/authority model changes.

Until then, any edit to a file listed in the manifest `cold` block is a
`HOT_RELOAD_BOUNDARY_VIOLATION` and must wait for an intentional cold build
window, never a kill/relink of a running process.

## Server lifecycle

The dedicated server (`mimita.exe --server`) bypasses `gameInit`, so it owns its
own live-code lifecycle in `src/network/server.cpp::runServer`:

- `LiveEventJournal::init()` and `HotReloadSystem::startup()` at server start,
  with a `[SERVER LIVE CODE] loaded= generation= code_hash=` log line;
- `HotReloadSystem::pollAndAdvance()` at the top of each fixed simulation step
  (the safe activation boundary);
- `unloadGameDLL()` and journal shutdown on exit.

The client-hosted listen server is covered by the client `gameInit` /
`engineTickSetup` lifecycle. Generation output files include the process id so a
server and client compiling at the same time never collide.

## Cold-start reminder

While a `cold` source change is pending, the runtime records a
`hot_reload_boundary_violation` and emits an in-game notification every ~10
seconds (`cold_restart_pending`) so a human knows a cold start is required.
Live gameplay edits keep working; only the cold change waits.

## Private development only: detours and hot-patching

A loaded DLL could patch EXE code (detours/hot-patching) to redirect functions
at runtime. This is intentionally **not** the architecture. It is documented
only as private development / security research material in the hacker-vs-hacker
and bug-bounty sense: it is fragile across builds, needs symbol or pattern
discovery, bypasses ABI validation, and must never be used on public servers.
The supported mechanism is the stable hot ABI and generational loader.

## Long-term direction

```text
tiny kernel
+ generic entity/component store
+ generic runtime capabilities
+ hot gameplay module / future bytecode or IR
```

This makes natural later: canonical component serialization, world/state
hashing, deterministic replay, content-addressed behavior code, distributed
storage, causal event history, and Blender/MiMITA live world editing. C++ hot
modules are the current implementation; the boundary does not prevent a later
sandboxed evaluator.

## Related

- `docs/architecture/live-development/live-development.md`
- `docs/architecture/live-development/project-layer.md`
- `docs/architecture/live-development/hot-kernel-next-steps.md`
- `docs/gold/2026-09-12-live-code-hot-reload-journey.md`
- `docs/architecture/ecs-entity-etc/ecs.md`
- `docs/architecture/ecs-entity-etc/ecs-migration.md`
- `docs/features/live-code-development/live-code-development.md`
