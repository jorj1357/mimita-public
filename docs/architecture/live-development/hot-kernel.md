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
- `docs/architecture/live-development/hot-kernel-next-steps.md`
- `docs/architecture/ecs-entity-etc/ecs.md`
- `docs/architecture/ecs-entity-etc/ecs-migration.md`
- `docs/features/live-code-development/live-code-development.md`
