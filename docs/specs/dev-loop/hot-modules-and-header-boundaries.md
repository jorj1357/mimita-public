// 2026-11-27
/* purpose
* define the resident/hot boundary, hot module shape, and header fan-out rules
* make one header own one concept and be included by as few files as possible
* make header edits predictable instead of rebuilding hundreds of units
* this file DOES NOT change gameplay ownership or the ECS/actor model
* this file DOES NOT authorize a big-bang rewrite
*/

# Hot modules and header boundaries

**Status:** Target architecture, phased migration.

## The rule

> Resident code owns the process, an OS handle, or the ability to report a DLL
> crash. Everything else is hot.

> One header owns one concept. A header includes the minimum it needs. A file
> includes the minimum it uses.

These two rules decide where code and declarations live. Build speed is the
consequence, not the goal.

## Resident boundary

Resident sources stay in the EXE and cannot be hot-reloaded. A file MAY be
resident only when it owns at least one of:

- the process entry point or CLI bootstrap;
- a window, GL device, socket, thread, or other OS handle whose lifetime spans
  a module reload;
- the crash handler, log sink, or module host that must work when a module
  faults;
- the stable C ABI boundary between the host and hot modules.

Every resident file MUST have a one-line justification in the residency table.
The resident set MUST stay below 10% of game sources. Files that do not meet a
justification move hot.

Current resident set is 42 sources; the target is a justified subset. Candidate
moves hot when they own no OS lifetime: `persistence-queue.cpp`,
`json-file-watch.cpp`, `replay-factory-worker.cpp`,
`quick-hit-debug-visual.cpp`.

## Module map

The whole game is one logical generation built from a small number of hot
modules with stable C ABI surfaces:

```text
core        actor/world/sim state, contact, damage
gameplay    weapons, movement, gamemodes, npc behavior
present     render, animation, audio, ui/hud
editor      map editor, animation editor, weapon pose editor
net         client/server packet handlers and replication
content     loaders, maps, assets, config
```

Rules:

- Every gameplay source belongs to exactly one module.
- Modules share one frozen snapshot and one generation manifest.
- A module may depend on `core` ABI, never on another module's internals.
- Only changed modules relink and reload.
- The generation manifest records per-module SHA-256.
- A runtime pins one manifest; mixed module generations are rejected.

The existing module host and map-editor module prove the mechanism. The work is
to expand it, not invent a second loader.

## Header ownership

- One header = one concept. If a header needs "and" to describe it, split it.
- A public header exposes the smallest stable surface: forward declarations,
  opaque handles, or a small struct plus free functions.
- Implementation details stay in the `.cpp` or a private header.
- A header MUST NOT include an aggregate header (`config.h`, a types dump, a
  "common" header) to get one type.
- A header MUST NOT include another subsystem's header for a type it can
  forward-declare.
- A `.cpp` includes what it uses, in ownership order, not alphabetically by
  accident.
- No project header is added to `src/pch.h`.

## Dependency direction

```text
content -> core
net     -> core
gameplay-> core
present -> core
editor  -> core + present
core    -> nothing above it
```

An edge that points upward is a defect, even if it compiles.

## Fan-out budgets

Budgets are per header, measured from compiler `.d` files:

| Kind | Budget | Examples |
|---|---:|---|
| PCH | exempt | `src/pch.h` |
| Stable ABI | ≤ 40 | host ABI, logging API |
| Core type | ≤ 50 | physics types, actor body |
| Subsystem | ≤ 30 | weapon types, movement types |
| Feature | ≤ 10 | one editor panel, one effect |
| Leaf | ≤ 3 | one implementation detail |

Rules:

- A changed header reports its exact dependent count before compiling.
- A header over budget MUST NOT gain a dependent.
- A budget can only ratchet down. Raising it needs an issue and a measured
  reason.
- A new include of an over-budget header from a leaf file is rejected.
- The fan-out report is generated on every check and summarized in the
  postmortem.

## Required splits

The current top fan-in is the work list:

```text
structured-log.h   360  -> stable logging API + implementation
physics-types.h    312  -> per-concept headers + forward declarations
movement-types.h   292  -> movement state vs. movement config
map-mesh-types.h   287  -> mesh handle vs. mesh data
weapon-types.h     286  -> weapon identity vs. weapon runtime state
config.h           286  -> owning config headers only
avatar-rig.h       280  -> opaque rig handle + accessors
actor-body.h       272  -> opaque body handle + part query
```

Each split MUST preserve behavior and ABI compatibility or bump the ABI
version explicitly.

## Include lint

The check runs an include lint on the diff:

- new include of an over-budget header from a leaf;
- hot header including a cold or aggregate header;
- public header with no owner;
- new dependency edge that points upward;
- duplicate declaration of an existing concept.

The lint output names the header, the dependent, the budget, and the smallest
fix.

## Migration

1. Publish this spec and the residency table.
2. Add the budgets and lint in report-only mode; record the baseline.
3. Split the top three headers with measured before/after fan-out.
4. Turn the lint into a gate for changed files.
5. Extract the `content` and `editor` modules first; they have the fewest
   upward edges.
6. Extract `present`, then `net`, then `gameplay`.
7. Keep `core` last; it is the shared ABI everything else depends on.

No step may leave the tree unbuildable.

## Acceptance

- Every resident file has a justification and the set is below 10%.
- A leaf edit rebuilds one object and links one module.
- A changed header reports its exact dependent count before compiling.
- No header over budget gains a dependent.
- The top three headers are below half their 2026-11-27 fan-in.
- Two independent modules can be rebuilt and reloaded without relinking the
  others.
- The include lint rejects an upward edge and a new aggregate include.
