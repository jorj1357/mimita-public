# 2026-09-13 — Live-runtime generic bootstrap: the "no new slot" plan

Status: `GOLD / REFERENCE` — the governing direction from 2026-09-13 onward.
Supersedes the earlier incremental "add another cold seam" plans for gameplay/editor.

## The one architectural goal

> After the generic bootstrap, adding an ordinary new gameplay/editor/system
> concept must mean **creating and registering a package / system / schema /
> event / capability / command / resource** using existing generic runtime
> mechanisms — **not** adding another field, enum, switch case, or
> subsystem-specific call site to `mimita.exe`.

And:

> `modecreate` is the first proof: its controls, selection, copy/paste, free-fly,
> notifications, commands, and source-file structure are all changeable while the
> same EXE / world / session / EntityIds stay alive.

## Guiding principle (applies to all work from 2026-09-13)

- Prefer making a thing **hot** over adding a cold seam. Every new cold ABI
  field/enum/call-site is debt; every generic registry entry is progress.
- Simplify: delete duplicate/obsolete cold paths when a generic mechanism
  replaces them. Favor deletion over addition.
- When a feature "needs" a new cold slot, first ask whether an existing generic
  primitive (package/system/schema/event/capability/resource/command) can express
  it. Only if it truly needs a new OS/hardware/kernel primitive do we evolve the
  kernel, and then we add a **generic provider**, not a feature-specific slot.
- `mimita.exe` may still grow a **fixed set of generic meta-mechanism signatures**
  during the staged bootstrap; after that, signatures change only rarely and only
  for a genuinely new class of primitive.

## Irreducible kernel boundary (honest)

These stay cold because a running process cannot gain them without OS linkage or
a memory-model change:

- process/thread lifetime, allocator, panic handling;
- `EntityId` allocation and raw component **storage bytes**;
- dynamic-library load/unload, file-watch syscall, hash syscall, PID,
  build-worker subprocess;
- socket lifecycle + raw IO, monotonic clock, input polling, GPU context + draw
  submission;
- the **generic registry schemas and registration function signatures**
  (`GamePackageDescriptorV1`, `GameSystemDescriptorV1`, `GameEventTypeDescriptorV1`,
  `GameComponentSchemaDescriptorV1`, `GameCapabilityDescriptorV1`,
  `GameCommandDescriptorV1`, `GameResourceDescriptorV1`). Adding *entries to their
  arrays* is hot; changing a *signature* is a cold build;
- scheduler **phases** the kernel times (fixed tick, render frame). New phases
  require cold only if the kernel cannot derive the timing.

## "No new slot" — precise promise

Promised (hot): new System, EventType, ComponentSchema, CapabilityProvider,
CapabilityRequirement, Resource, Command, Migration, editor operation
composition, package source files (add/rename/delete/split/merge), package
manifest changes.

Not promised (may need kernel evolution): a genuinely new OS/hardware capability
(e.g. a new GPU API, new device class) — but the kernel adds a **provider**, after
which everything built on it is hot.

## Generic package ABI

Each package (DLL generation) exports a `GamePackageDescriptorV1` via `GetGameAPI`
(or a dedicated symbol). The kernel iterates the descriptor's arrays; it does not
know the names inside them.

```text
GamePackageDescriptorV1 {
  structSize, abiVersion, packageId(hash), logicalHash, name,
  systems[], eventTypes[], componentSchemas[], capabilityProviders[],
  capabilityRequirements[], commands[], resources[], migrations[]
}
```

Extend the existing `Project::PackageManifest` (`src/project/live-runtime.h`) /
`PackageRegistry` to be the runtime manifest; the DLL descriptor is the POD mirror.

## Generic system registry

```text
GameSystemDescriptorV1 { id(hash), domainId(hash), priority, invoke(host,tick,dt),
                         name }
```
Kernel keeps a flat `activeSystems` array; registration hashes once, resolves the
entry pointer once. Runtime indexes cached slots — no string/hash lookup in inner
loops. No `switch(name)`.

## Dynamic scheduler subscriptions

Domains are data (`domainId` hash, rateHz). Packages register subscriptions
`{systemIndex, domainId, priority}`. Reserved domains the kernel times:
`gameplay.60`, `render.frame` (extend `src/sim/domain-scheduler.*`).
`ragdoll.solver`, `editor.frame`, `audio.frame` are registered data.

## Dynamic event registry

```text
GameEventTypeDescriptorV1 { id(hash), schemaHash, deliveryDomain, dispatch }
```
`GameEventV1` stays `{typeId, entities, tick, payload}`. Kernel resolves
`typeId → subscribed systems` at registration; dispatch loops the array. Bridge:
pre-register all current `GAME_EVENT_*` enum values as event types so existing
cold emit sites can migrate to `emit(typeId, payload)` one at a time.

## Dynamic component/schema registry

Extend `Project::ComponentSchemaRegistry` (`src/project/live-runtime.h:170`):

```text
ComponentSchema { typeId(hash), schemaHash, storageKind, size/align, fields[],
                  copyPolicy, serializePolicy, networkPolicy, migrations }
```
Two storage classes: (a) typed C++ fast path (existing components, pointer-stable);
(b) dynamic per-type dense arena keyed by `(EntityId, typeId)` with cached handles,
for package-declared components. Incremental: existing stay typed, new are
dynamic; migration optional.

## Dynamic capability registry

```text
GameCapabilityDescriptorV1 { id(hash), signatureId, schemaHash, callable }
```
Kernel keeps provider table + `Project::CapabilityRegistry` (grant/request/deny,
exists, currently unenforced). Packages declare requirements; activation resolves
them; missing/denied fails the candidate. Runtime uses cached typed handles.
Seed providers: `entity.spawn`, `entity.destroy`, `component.read`,
`component.write`, `relationship.add/remove`, `physics.query`, `physics.impulse`,
`resource.resolve`, `audio.play`, `network.send`, `project.apply-change`,
`notification.show`, `time.now`, `log.write`.

## Runtime command registry

```text
GameCommandDescriptorV1 { name, usage, systemId|invoke }
```
Kernel keeps `name → handler`; generation-scoped. Falsification test for the whole
package registry: a new hot package adds `invlist`/`equipslot`/`my_command`
without editing the EXE.

## Generic editor operations (no EditorOp enum)

Editor operations are compositions of capabilities in the hot editor package:
copy = `component.read` (schema `copyPolicy`); cut = copy + `entity.destroy` +
`project.apply-change`; paste = `entity.create` + `component.write` (policy
filtered) + `project.apply-change`; move/rotate/scale = `component.write`
(transform schema); add/remove component = dynamic-store writes; bind behavior /
resource = `behavior.bind` / `resource.bind`. The kernel knows no keybinds.

## modecreate after bootstrap

A package registering systems `editor.hover`, `editor.selection`, `editor.overlay`,
`editor.input`, `editor.clipboard`, `movement.freefly`, `editor.notify`; commands
`modecreate`; events `editor.selection-changed`, `editor.clipboard-changed`;
requirements `entity.*`, `component.*`, `physics.query`, `resource.resolve`,
`project.apply-change`, `notification.show`. Free-fly is a **hot movement system**
(design A): `movement.main` moves hot and modecreate swaps in a free-fly system for
the domain generation. No permanent `GAME_EVENT_MOVEMENT_STEP`.

## Whole-project watcher / build graph

Watch package roots (extend `ProjectWatcher` to emit per-path
CREATE/MODIFY/DELETE/RENAME/MOVE). Feed `DependencyGraph` (`.d` parser exists) to
compute affected TUs. Compile only affected TUs → immutable candidate generation.
Load → validate registrations/schemas/capabilities/tests → migrate → activate →
retire old. Deleting an active source does not remove active behavior until a valid
replacement activates.

## Networking, assets, cross-platform, multiplayer

- Transport kernel owns sockets/lifecycle/reliable channel/raw time; hot packages
  own encode/decode, snapshot construction, batching, prediction, interpolation,
  lag simulation as registered systems + packet/message schemas. No per-policy event.
- Resources: `ResourceDescriptor { resourceId/contentHash, type/schema, logicalName }`;
  package-declared loaders register as resource-provider capabilities. Decode hot;
  GPU upload via `render.upload` capability (kernel).
- Platform providers: `DynamicLibraryProvider`, `FileWatcherProvider`,
  `HashProvider`, `ProcessProvider`, `BuildWorkerProvider` — Win now, Linux later.
- Generation manifest: logical package manifest hash (cross-platform),
  `platformPackageHash` per platform, `moduleSetHash` = registered type-id set.
  Peers acquire package, register, validate, READY, switch at shared `switchTick`.

## Staged bootstrap (2–3 cold builds accepted)

- **Phase 0a (done 2026-09-13):** generic registry + package descriptor + system/
  event/component-schema/capability/command registration + loader wiring + terminal
  hook + reserved domain timing + a hot demo package.
- **Phase 0b (done 2026-09-13):** dynamic component byte-store; capability
  requirement resolution (package providers + kernel primitive capabilities);
  generic event emit; package-declared domains run each fixed tick; host
  capability context (`dynamicReadComponent`/`dynamicWriteComponent`).
- **Phase 0c (source done 2026-09-13; cold bootstrap pending):** per-path watcher
  over the whole `src` tree; dependency-graph/`.d` incremental compile
  (`--changed`, stable obj dir); generation manifest hash
  (`moduleSetHash`/`logicalCodeHash`) populated in code-generation packets.
- **Phase 1 (done 2026-09-13):** `modecreate` as a hot package — shared-mode state,
  hovered vs selected (`CTRL+LMB`), overlap wheel cycling, clipboard edges via
  `EditorForkFn`, combat suppression in the gameplay policy, runtime-registered
  `modecreate` command, and a hot `movement.freefly` system using a generic
  kernel movement-override capability (WASD + vertical, noclip). Installed via
  cold build; runtime human proof pending. `movement.main` is still a stub.
  Movement groundwork landed and installed: `MovementStateV1` POD,
  `physics.moveCapsule` capability, and a hot `movement.main` system (opt-in
  `hotmovement [0|1]`; create mode = free-fly) that uses them. `movement.freefly`
  is folded into `movement.main`. `Physics::moveCapsuleStep` extracted and
  covered by `--movement-selftest` (determinism, integrate parity, floor
  collision) and `--movement-parity-selftest` (drives the real hot `movement.main`
  and checks determinism + landing vs the primitive; not yet bit-parity with the
  full built-in step). Bit-parity with the built-in step before default-on.
- Later: movement/ragdoll, tools/weapons, network policy, assets, audio/UI/NPC.
  Old cold implementations remain fallback until each package passes runtime proofs.
  Old cold implementations remain fallback until each package passes runtime proofs.

## Torture / falsification suite (same PID, world, session, EntityIds)

Add/move/rename/delete editor source; register a `banana.peel-slip` system; a
`banana.eaten` event; a new component schema; a new command; a new capability
provider over an existing primitive; rewrite copy/paste, movement, weapon, and
network-batching policies; inject a syntax error (old generation stays live); fix,
activate, rollback.
Falsifiers: a new component needs an editor `if`; a new system needs a new
`GameAPI` field; a new command needs an EXE edit; a rename needs a restart.

## Primitives (minimal)

World: Entity, Component (typed/dynamic), Event, Constraint, Resource,
Relationship. Runtime meta: Package, System, Schema, Capability, Generation,
SimulationDomain. `Tool` = composition (Component + Relationship + Behaviors +
Resources). `Inventory` = generic containment/ownership (capacity/policy), not a
special component.

## What NOT to build yet

Linux providers, full dynamic-storage migration of all components, sandbox/IR
evaluator, GPU-handle hot swap, distributed package download, all asset loaders,
networked collaborative editing, vehicle/VR physics.
