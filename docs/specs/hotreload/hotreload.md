# MiMITA Hot Reload

## Purpose

This is the current hot-reload specification. It defines the desired end state,
the cold/hot boundary, the live-development loop, and the evidence required to
claim that behavior is hot.

The historical design record is archived at
[`docs/archive/hotreload/hotreload-history-2026-11-14.md`](../../archive/hotreload/hotreload-history-2026-11-14.md).
It is background evidence, not current authority.

## Specification authority

This document states desired behavior and standing decisions. It is not a
status tracker or a work log.

- Do not add current generation numbers, file counts, "currently working",
  session progress, current bug lists, plans, phases, or next steps here.
- Current machine state belongs in `build/hot-reload/status.json`.
- Work history, evidence, and "what next" belong in `docs/changelog/`.
- Historical counts in handoffs, audits, or prior reviews are not current
  claims. In particular, old figures such as `must_move=570` or
  `42 raw GL sources` are point-in-time snapshots. Regenerate
  `build/source-manifest.json`, `build/hot-reload/audit.json`,
  `build/hot-reload/cap-render.json`, and the cold-link trial before using any
  count as Cutover-1 evidence.
- Temporary agent focus belongs in
  `docs/handoffs/hot-reload-agent-focus.md`.
- If code disagrees with this document, the code is wrong unless this
  document is deliberately revised.
- Superseded hot-reload documents are pointer stubs to this file.

## The dream

The game is a live, editable system:

- edit C++, save, build a candidate generation, and activate it in the same
  running process;
- fix NPC behavior, collision, vehicle physics, combat, rendering, or
  networking without closing the EXE or losing the match;
- add or change packets, fields, handlers, game rules, UI, tools, and render
  policy live;
- change platform/backend behavior live in later phases;
- collaborate across the same live systems without a central authority.

The first target is **Cutover-1**. The larger dream is explicitly later.

## Cutover-1

Cutover-1 is complete when:

1. The normal client and dedicated server run from one replaceable `game.dll`.
2. Every ordinary behavior source is hot, except the cold core named below.
3. All client and server packet handlers run in the DLL.
4. No migrated behavior silently falls back to resident code.
5. Join, reconnect, session, packet, simulation, publish, gamemode, and error
   paths are hot.
6. Render policy, draw submission, uploads, effects, UI, and post-processing are
   hot. Only window/device/context/swap mechanisms are cold.
7. Input bindings, action mapping, and gameplay/UI actions are hot. Normalized
   input and cross-platform backends are later.
8. Resident state survives a generation swap, and schema migration is validated.
9. Old generations quiesce and unload safely; no job, callback, resource, or
   return address can enter them after unload.
10. A real gameplay or networking bug is fixed in the same process, with the
    same PID and session, and the behavior change is visible.
11. Rollback restores the previous generation and state.
12. The game is visibly playable in the target mode.

Cutover-1 does not require normalized input, ECS expansion, agent command
tooling, hot backends for every OS/device, or distributed storage.

2026 11 15 1030 jorj - this means like
ideal fps: 240, actual fps: 10-100
we can add logging live, fix it live, change .cpp code live, look at events.jsonl changing live
ai agents can see results of their cahnges live
no rebuild of the exe needed, no closing and re opening the exe, no 28 minute build times, that is the worst

npcs arent aggressive enough and .json edits arent fixing? .cpp edit live , fix live
vehiclel phsics arent accurate to actual what we want? fix live
want to add swimming physics? add live
want to remoev swimming physics? done live
want to edit networking, e.g. tweak lag compensation and rewind logic live? doable live no rebuild
want to edit GUI issues live? doabel live
want to add a entirely new system like video editor, audio editor, non euclidean geometry, 4d world, 2d world, quaternion camera instead of 3d fixed axis cant look around 360 degrees? doable live
edit maps live, with map editor
make new gamemodes live, with AI and existing code
refactor live, removing old dead code that is causing inefficiency live 
all doable without a single .exe rebuild or needing to close the .exe and then open it again 

prediciton: since im not so sure what im doing, this might taek a while, but i think if we do this and stick with it, and keep working on this until this dream and others are doable live, and its consistently live doable, this is a huge step forward for development speed as a whole, so i tink that 2026 11 15 1034 jorj i ts important  that we continue to do this and dont  be less than strict, dont let something that kinda works be an actual solution, build this  with it in mind that once it works, its likely that it keeps working indefinitely, given API changes and underlying technology changes kept up with

## Cutover-2 and Cutover-3

Cutover-2 is the backend phase:

- hot transport/backend modules behind stable capabilities;
- hot render-device/backend modules;
- hot logging/storage backends;
- the same behavior across platforms and input devices.

Cutover-3 is the broader live-collaboration phase:

- live authoring and shared documents;
- distributed persistence with no required central server;
- live tools beyond the game.

These phases must preserve the Cutover-1 evidence rules.

## Architecture

```text
resident EXE host
  process lifetime
  loader + generation barrier
  capability table
  window / GL context / device
  OS/input/network primitive pumps
  logging sink
  resident state storage
  transport primitive

one replaceable game.dll
  game init and frame
  simulation
  input/action behavior
  gameplay, NPC, collision, vehicles
  combat, audio policy, UI, tools
  render policy and draws
  packets, schemas, handlers, sessions
  server behavior
  replay and presentation policy
```

The resident host does not own editable game policy.

## Cold core

These are intentionally cold for Cutover-1:

- process entry and lifetime;
- loader, generation validation, barrier, and capability table;
- window, GL context, device creation, swap, and OS event pump;
- OS/input/network primitive calls: sockets, ICE/UDP library, framing/encryption
  primitives, and timers;
- logging sink;
- resident state storage;
- process/worker handles that must outlive a generation.

Every cold entry must name its owner, reason, hot replacement target, and removal
gate. The list is expected to shrink.

## Cold-to-hot promotion pattern

Promoting a behavior out of the cold core follows one repeatable sequence:

1. Resolve every target source in `build/source-manifest.json`. Stop if the
   owner is `resident`, `bootstrap`, or `launcher`; those are cold changes.
2. Separate the immutable mechanism from editable policy. The resident side may
   keep the window, OS event pump, socket, device, file watcher, or worker. The
   game side owns routing, bindings, validation, decisions, and presentation.
3. Define the narrow boundary. Prefer an existing versioned capability or a
   plain fixed-size event. Do not pass STL containers, vtables, or generation
   pointers across the ABI.
4. Add the hot consumer in the game DLL. It must be safe to replace on every
   generation activation.
5. Register and unregister callbacks/jobs through the host. A callback pointer
   into a game DLL must not remain installed after that generation unloads.
6. Add one owner-level diagnostic event that proves the hot path ran, plus a
   bounded failure reason when it did not.
7. Build only the game DLL through the root dev loop. A successful compile is
   not activation evidence.
8. Exercise the exact behavior in the same PID and inspect `events.jsonl`.
9. Prove rollback, then remove the duplicate cold behavior.
10. Update the cold-entry record with owner, reason, replacement target, and
    removal gate.

A new source file under `src/**/*.cpp` defaults to owner `game`. If a proposed
fix adds a source outside that rule, resolve its owner before writing code; do
not assume a new file will be hot.

## Hot behavior

All ordinary behavior is hot:

- gameplay, movement, collision, physics policy, vehicles;
- NPC perception, targeting, navigation, combat, and lifecycle;
- weapons, projectiles, damage, effects, ragdolls, audio policy;
- game UI, HUD, menus, terminal commands, tools;
- render policy, shaders, uniforms, draw streams, render targets, uploads;
- replay and presentation policy;
- world queries and streaming policy;
- save/load and document policy above the storage mechanism;
- every packet ID, schema, layout, encode/decode path, handler, and validation
  rule;
- join, reconnect, session health, map sync, spawn authority, lag compensation,
  snapshot policy, error paths, and server tick behavior.

A source is not hot because it compiles. It is hot only when normal play's
`events.jsonl` proves the DLL path for that behavior.

## Networking is entirely hot

The only cold networking piece is the OS/socket/ICE primitive adapter.

Hot:

- packet IDs and tombstones;
- packet schemas, layouts, encode and decode;
- every client handler;
- every server handler;
- HELLO, JOIN, WELCOME, reconnect, session health, map sync;
- input, movement, combat, projectiles, NPC/actor events;
- avatar, vehicle, ragdoll, gamemode, chat, terminal, progression;
- validation, rejection reasons, outage and timeout behavior;
- lag compensation and snapshot policy;
- ICE session policy and peer lifecycle decisions.

Packet IDs are tombstones and are never reused. Unknown fields are ignored;
missing fields receive documented defaults; schema changes validate at
activation.

### Packet ledger meaning

`config/hot-reload/packet-ledger.json` is generated from actual code:

- `src/network/packets.h` supplies packet IDs.
- `src/network/multiplayer-tick.cpp` supplies client inbound branches.
- `src/network/server-packets.cpp` supplies server inbound branches.
- Other active sources are scanned for inbound/outbound packet references.
- `build/source-manifest.json` supplies the source owner.

The ledger records the source file, line, owner, and inbound handler for each
direction. A direction with no discovered reference is `unknown`; it is never
silently defaulted to `resident`. `client_inbound` and `server_inbound` show
which rows are actual dispatch handlers rather than outbound/reference-only
uses.

The old hand-maintained `config/hot-reload/packet-schema.json` defaults and
overrides are deprecated. `devscripts/packet_registry_codegen.py` no longer uses
them to decide a path.

## Server split

The resident server keeps only the pump:

- bind/listen, send/receive;
- ICE agent/library tick;
- timers and scheduling;
- process lifetime.

The DLL owns server behavior:

- session accept/reject;
- join, reconnect, timeout, and disconnect policy;
- packet dispatch and validation;
- simulation, NPCs, combat, projectiles, vehicles;
- spawn authority;
- publish/snapshot policy;
- gamemode tick;
- error and outage handling.

The same `game.dll` serves client and server in Cutover-1. A later split requires
measured build, activation, or failure-isolation benefit.

## State

Hot reload replaces code, not the world.

- Durable data lives in resident versioned domain records.
- Behavior in `game.dll` reads and writes domains through bounded host ops.
- Do not pass STL containers, C++ objects, vtables, or old-generation pointers
  across the ABI.
- `EntitySnapshotV1` is transitional. It is replaced domain by domain and
  deleted after its last use.
- Each domain has a version, field mask, readers, writers, migration, and
  default policy.
- A candidate declares the old and new schema, stages a copy, migrates,
  validates, and commits atomically.
- A failed migration leaves the old generation and old state active.
- `state.migration-selftest` exercises the migration API and rejection path in
  the live process. It is not a substitute for a real generation-to-generation
  domain migration proof.

## Input

- Cold: OS/window/device polling, raw hardware events, focus/hotplug.
- Hot: bindings, chords, deadzones, sensitivity, action mapping, context
  routing, and simulation-facing actions.
- A direct device call in gameplay is a documented cold exception and is
  journaled.
- Normalized cross-device input and hot input backends are Cutover-2 work.
- Agent, terminal, replay, keyboard, mouse, controller, touch, and network input
  normalize into the same action surface; they do not create parallel gameplay
  paths.

## Rendering

Cold:

- window creation, GL context, GLAD dispatch, framebuffer size, swap, poll,
  video mode, cursor, VSync, shutdown.

Hot:

- shaders, programs, uniforms, textures, render targets, viewport, clear state,
  passes, draw submission, post-processing, effects, UI, debug visuals, and
  render policy.

Raw GL in hot code is transitional. New code uses the render capability; the
transitional bridge is removed when coverage is complete.

## ABI and generations

The ABI is the contract between resident host and `game.dll`:

- exported functions;
- signatures;
- struct layouts and sizes;
- versions;
- pointer and ownership rules;
- capability tables.

Changing the ABI is a deliberate cold host build. Changing behavior inside the
DLL is not.

A generation is immutable. Activation:

1. detect a coherent source revision;
2. build and link a candidate;
3. validate ABI, build identity, and schema;
4. load while the old generation remains active;
5. quiesce at a safe boundary;
6. commit code and migrated state atomically;
7. keep the old generation for rollback.

A failed build, validation, migration, or activation leaves the old generation
active.

## Live loop

The normal loop is:

```text
edit hot source or schema
  -> source manifest snapshot
  -> codegen checks
  -> candidate game.dll build
  -> validation
  -> publish ready generation
  -> activate at safe boundary
  -> exercise behavior
  -> inspect events.jsonl
  -> accept, reject, or roll back
```

The root dev-loop is the only build and run authority. It owns the build lock,
source watcher, codegen checks, game-DLL request, publication, activation
status, and run control. Agents do not invoke compilers or game build workers
directly.

The hot-client live-fix phase contract is in
[hot-client-live-fix.md](hot-client-live-fix.md). It defines the required
default-client frame ownership, same-PID activation, and cosmetic/avatar live
fix behavior.

## Evidence

A build proves compilation. It does not prove configuration, packets, state,
behavior, or visual acceptance.

A hot claim requires:

- exact source revision;
- changed files;
- candidate generation and DLL;
- unchanged EXE PID;
- same session/world where applicable;
- activation and rollback events;
- `events.jsonl` path and relevant records;
- before/after state or behavior;
- human acceptance where visual or feel is involved.

No fake hot: a migrated path must not silently use resident code.

## Changelogs and status

- `docs/changelog/` is history: what changed, evidence, failures, reverse steps,
  and next work.
- `build/hot-reload/status.json` is generated current status.
- `build/hot-reload/audit.json` is generated unload-safety audit output.
- `docs/handoffs/hot-reload-agent-focus.md` is temporary coordination
  scratchpad, not specification.
- No hand-maintained hot-reload status or plan is authoritative.

## Generated files and coordination paths

These paths are architectural roles, not current status.

| Path | Role | Written by | Hand-edit? |
|---|---|---|---|
| `build/source-manifest.json` | Source ownership and revision | `devscripts/source_manifest.py` | No |
| `build/link-inputs-<mode>.json` | Actual object/source list passed to the EXE linker | `build.py` | No |
| `build/hot-reload/status.json` | Build/run/coverage/gap summary | `devscripts/hot_reload_status.py` | No |
| `build/hot-reload/audit.json` | Static unload-safety findings | `devscripts/audit_triage.py` | No |
| `config/hot-reload/packet-ledger.json` | Packet client/server hot coverage | `devscripts/packet_registry_codegen.py` | No |
| `config/hot-reload/state-ledger.json` | State-domain coverage | `devscripts/resident_state_codegen.py` | No |
| `docs/handoffs/hot-reload-agent-focus.md` | Temporary agent focus | Agents | Yes, temporary |
| `docs/changelog/` | History, evidence, reverse steps, next work | `devscripts/changelog.py` + agents | Yes |

Do not create a second source inventory, status document, audit script, or build
coordinator. Extend the existing owners.

> **2026-11-14 17:28 TODO:** the first generated status/audit outputs are
> provisional. Validate the source manifest, packet/state ledgers, audit
> classification, build identity, and runtime fields before treating those
> values as evidence.

## Non-goals for Cutover-1

- normalized cross-device input;
- ECS expansion;
- agent command expansion;
- hot OS/device/transport backends;
- distributed persistence;
- cross-platform process portability.

These remain in the later phases.
