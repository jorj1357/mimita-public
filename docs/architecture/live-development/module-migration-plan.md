// 2026-11-08
/* purpose
* own the continuation plan for moving MiMITA behavior into live modules
* track cold-build boundaries, repo-local build work, and remaining migrations
* this plan DOES NOT redefine gameplay behavior or the module ABI
* this plan DOES NOT permit agents to invoke builds
*/

# Module migration plan

Canonical status and continuation plan for the module-first direction.

## Working goal

The slogan is "one cold build every 999999 years." The engineering target is:

- cold builds only for process, device, transport, packet-schema, document
  storage, loader, and public-ABI changes;
- every gameplay and tool behavior owned by a reloadable module;
- module, module-private-header, and config edits never relink the EXE;
- warm boundary builds are seconds with ccache/object cache;
- clean release/install builds remain slower and are not claimed to be zero.

Exact zero is not physically possible for a clean machine build. The long-term
path to "effectively never" is a generic, versioned, data-driven event/behavior
IR so new gameplay behavior does not add host call sites. EXE changes then
remain limited to process/authority/device/transport mechanisms.

## Repository-local workspace

See [self-contained-workspace.md](self-contained-workspace.md). All lanes,
scheduler slots, ccache, temp files, runtime room files, world projects, build
artifacts, and journals live inside the repository under gitignored roots.

## Status

| Work | Status | Cold boundary? | Notes |
|---|---|---|---|
| Agent lane authority | Implemented / active | no | Agents observe; lane dev-loop builds; module host builds modules |
| Repo-local lanes/scheduler/cache/temp | This plan | no | Lane root moves to `.dev/agent-lanes` |
| Link ledger and no-op artifact reuse | Implemented | no | Stale relinks self-repair; no-op does not create a new artifact |
| Module-private headers | Implemented | no | Private headers rebuild only the module |
| One ABI-first boundary build | Next | **yes, one** | Install all host seams before feature extraction |
| Async authored rebuild + detached Recast + atomic swap | Implemented | host slice | Worker off-tick; atomic swap; old worlds retired |
| `map-geometry` hot module | Pending | part of boundary | Signature/transform/flat-ground policy |
| Full map-editor extraction | Pending | part of boundary | Snapshot store, undo/redo/clipboard, object/draw/input/command |
| GLB schema v2 | Partial | sidecar | Rotation/scale/tombstone/clone runtime implemented; imports data model only |
| Networking behavior subsystem | In progress | one capability build | Map-sync/reconnect/session-health decisions; transport and packet schemas stay cold |
| NPC perception subsystem | In progress | one capability build | FOV/range/LOS gates, memory, belief, prediction; world query stays host |
| NPC behavior subsystem | In progress | no | Goal/action/targeting/combat/team policy; authoritative application stays host |
| Remaining hot owners | Later | reserved surfaces | Gamemode, ragdoll, contact, remaining networking |
| Physics/collision behavior | Later (not current scope) | reserved surfaces | Query/collision policy behind bounded hot capabilities; fixed tick, cached broadphase, collision solver, and authority stay host |
| Data-driven IR/behavior | Long-term | eventual kernel only | New mechanisms without new host call sites |

## One host EXE and shared module store

The target workflow is one canonical host EXE plus one shared content-addressed
module store. Module-only lanes consume the shared host and publish module
generations; they do not create a per-lane EXE. Host/ABI/packet lanes require a
single host boundary build. Config/assets/modules resolve from one runtime root
with a lane overlay for unvalidated work. Static-linking all modules into the
EXE is explicitly not the target because it removes hot reload.

Canonical generation identity is `(module, abi, state_schema, code_hash)`.
Identical module code must not produce different generations, or the module-set
hash and peer agreement will diverge. The lane land/publish path is the only
writer of the shared host/store; game processes and module workers never write
it. See `docs/specs/dev-loop/agent-lanes-build-authority.md` and
`docs/specs/dev-loop/dev-loop.md`.

## One ABI-first boundary

The single boundary build installs:

- detached `RecastNavMeshCandidate`, adopt/retire, non-building query;
- async rebuild worker snapshot/result types and fixed-tick drain/swap hooks;
- `map-geometry` capability/events and host fallback;
- editor snapshot/object/draw/UI/input/command/project/world-edit services;
- GLB v2 schema/base-transform/tombstone/clone/import host hooks;
- generic reserved policy surfaces for later hot owners;
- module-private header catalog support.

After promotion, feature work is module-only unless a genuinely new kernel
mechanism is discovered. That is a `HOT_RELOAD_BOUNDARY_VIOLATION` and is
scheduled deliberately, not hidden.

## Hot subsystem targets

- **Networking:** move the map-sync, reconnect, session-health, snapshot,
  spawn-authority, and combat-validation state machines into the networking
  module. Keep transport, packet encode/decode, map/asset loaders, and sockets
  cold. Reconnect accepts must restore the authoritative map id (or preserve the
  previous map when an older peer omits it).
- **NPC perception:** move FOV/range/LOS gating, memory decay, belief, and
  prediction into `npc-perception`. Expose only a bounded world-query
  capability; the module decides when to query and how to gate the result.
- **NPC behavior:** move targeting, navigation choice, combat intent, team
  coordination, and utility policy into `npc-behavior`. The host keeps actor
  state, world/navmesh queries, collision/physics application, and the fixed
  tick.
- **Server budgets:** phase-budget thresholds and yield intent are hot policy;
  measured durations and the tick loop remain host mechanisms.

A new behavior that needs a one-off host call site is a
`HOT_RELOAD_BOUNDARY_VIOLATION`. Generalize the event/action/capability surface
instead.

## Immediate continuation order

1. Finish repo-local workspace migration and documentation.
2. Create the boundary lane inside `.dev/agent-lanes`.
3. Implement and run the one ABI-first boundary build through the lane dev-loop.
4. Land async candidate rebuild and detached Recast.
5. Add the `map-geometry` module.
6. Extract the map editor in phases (snapshot/undo/clipboard, objects, draw/UI,
   input, commands/projects/world-edit, remove host duplicates).
7. Add GLB v2 rotation/scale/tombstones/clones/imports.
8. Move gamemode/network/NPC/ragdoll/contact policy behind reserved surfaces.
9. Introduce the data-driven IR/behavior path behind the same evidence rules.

## Evidence rules

Build, runtime, and human acceptance are separate claims. Every module change
reports lane, worktree, module generation, PID, exact `events.jsonl`, the owner
event, and before/after fields. A build or `module.activated` alone is not
behavior proof.

## Cross-references

- [Hot reload specification](../../specs/hotreload/hotreload.md)
- [Live development invariant](live-development.md)
- [Hot kernel](hot-kernel.md)
- [Agent lanes and build authority](../../specs/dev-loop/agent-lanes-build-authority.md)
- [Build and EXE](../../operations/build-and-exe/build-and-exe.md)
- [Self-contained workspace](self-contained-workspace.md)
