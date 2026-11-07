// 2026-11-07
/* purpose
* define the desired native C++ module build/reload path and migration target
* distinguish current source behavior from architecture goals and historical notes
* define state ownership, safe activation, generation identity, and proof
* Audience: gameplay, engine, tools, and AI contributors changing C++ behavior
* DOES NOT define individual gameplay formulas or visual design
* DOES NOT promise that native C++ source runs without compilation
* DOES NOT claim all C++ is currently reloadable
*/

# MiMITA Live Modules and Hot Reload

## 1. Goal and plain-language contract

The goal is to let contributors change gameplay and tool behavior while the
same MiMITA process remains open. The ordinary workflow is:

```text
edit a subsystem's normal C++ files and save
  -> detect the owning module
  -> compile and link only affected module code in the background
  -> validate a new immutable module generation
  -> activate at the subsystem's safe boundary
  -> inspect that process's events.jsonl for the active generation and result
```

“No build” for native C++ means no manual build request, no full executable
rebuild, and no process restart for a change contained within an existing
module. Native C++ still must be compiled and linked into machine code before
it can run. Saving a file should start that work automatically. Data/config
changes that already have a validated reload path should continue to reload
without C++ compilation.

The long-term target is every gameplay and tool behavior source being assigned
to a reloadable subsystem module. A module groups related files; files do not
each become a DLL. A future restricted script or data-driven layer may let
selected small policies change without compiling native C++, but it must use
the same state, authority, safe-activation, and evidence rules. It is a later
design and is not a claim that C++ source itself runs without compilation.

This specification defines the target and an NPC proof slice. It does not
claim that the general module workflow is implemented today. Full integration
and release builds remain required when the kernel, module ABI, or other
non-reloadable mechanism changes.

## 2. Module and kernel responsibilities

### 2.1 Module ownership

A module is one cohesive behavior boundary such as `npc-behavior`,
`map-editor`, `movement-policy`, or `weapon-policy`. Its ordinary source files
remain in understandable subsystem locations. A module catalog maps paths or
source globs to one module and declares its dependencies. The watcher assigns
saved, added, renamed, and removed files through that catalog; contributors do
not need a separate manual build command for each file.

A source file may belong to only one module. Shared code belongs in a stable
host interface or an explicit shared target, not duplicated between modules.
When a shared interface changes, rebuild only the modules whose declared or
compiler-discovered dependencies use it. An ambiguous or unowned source change
must be reported clearly rather than silently omitted from a build.

### 2.2 Stable host and state ownership

The kernel is the stable EXE host. It owns process lifetime, module loading,
simulation scheduling, persistent world/session state, and the core mechanisms
used by modules. Common actor state—including identity, transform/position,
velocity, health, team/role, and target relationships—is host-owned and remains
available while module code is replaced.

Modules read state through stable, documented host capabilities and request
changes through the owning gameplay system. A module must not bypass authority,
damage validation, collision, fixed-tick execution, or the host's state owner.
For example, an NPC behavior module may request an actor action; the host still
validates and applies movement, attacks, health changes, and collision using
their existing authoritative mechanisms.

The host must support versioned module-owned state schemas for behavior-specific
state so adding a behavior field does not require adding a field to a fixed
kernel C++ struct. Schema migration is prepared and validated before activation
and committed atomically with the code generation. Failed migration leaves the
old state and old code active. The host's generic storage, scheduler, or
capability mechanism may still require a full EXE build when it must gain a
fundamentally new operation.

The module boundary uses a versioned ABI with plain, bounded data and explicit
host capabilities. It must not pass ownership of C++ containers, exceptions,
or module-private pointers across the boundary. A candidate whose ABI, required
capabilities, dependencies, or state schema do not match is rejected while the
currently active generation remains usable.

### 2.3 Cold mechanisms and reloadable behavior

The kernel may retain mechanisms that need stable process-wide ownership:
process/window lifetime, fixed-tick scheduling, memory and module loading,
generic state storage, logging, file/resource access, rendering-device access,
network transport, and physics/collision query infrastructure. Policy and
behavior that use those mechanisms should move behind module interfaces over
time. The kernel remains small in responsibility and stable in interface; it
need not be a small EXE by byte count for module edits to be fast.

## 3. Save-to-live build and activation contract

1. Detect a completed save, debounce editor writes, and resolve every changed
   path to its module and dependency set. Unsaved keystrokes do not start a
   build.
2. Build an immutable source snapshot for that module generation. Compile only
   affected translation units, use correct compiler-cache keys, and link the
   module to a new staging path. Never write over a DLL loaded by a process.
3. Report changed source identity, module, phase timings, compiler diagnostics,
   and result to the owning development loop. If files change during the build,
   finish against the captured inputs and coalesce one follow-up generation.
   Failure records identify the failed translation unit or link step, exit
   code, and bounded compiler output; a generic `BUILD FAILED` alone is not
   actionable.
4. Validate the candidate's hash, module identity, ABI, dependencies, required
   capabilities, and state-schema migration without replacing the active code.
5. Queue a valid candidate for activation at a safe owner boundary. Authoritative
   gameplay modules activate on the fixed 60 Hz simulation tick, never on each
   render frame or halfway through a tick. Other modules use their owning
   subsystem's safe boundary, with no callback still executing old code at
   unload time.
6. Atomically publish the new module function table and state migration. Keep
   the previous generation available until the new one has executed and passed
   its runtime checks; allow rollback without restarting the EXE.

Compile, link, validation, migration, or activation failure must leave the
last-known-good generation active, identify the exact failure, and retain
enough candidate/build information to diagnose it. A new module generation
must not reset host-owned actor, world, match, or editor state.

An initial kernel build is required to install the module loader, host
capabilities, state ownership, and safe activation path. Changes to module
internals after that point should not relink the EXE. Changes to the kernel or
public ABI can still require it; the design minimizes these changes through
generic capabilities and versioned state rather than promising they never
happen.

The module worker uses the canonical dev-loop's immutable build-input and
artifact identity contract in
[`docs/specs/dev-loop/dev-loop.md`](../dev-loop/dev-loop.md), including the
desired section 5.3 snapshot-pinned build and run rules. A module build must not
read a moving source tree or load a mutable `latest` DLL path.

## 4. Generations, multiplayer, and journal evidence

Each process has one run identity and an active generation map keyed by module
name. The run manifest records the EXE build identity and, for every loaded
module, its generation, source/content hash, ABI version, and activation tick.
Each process writes its own canonical `events.jsonl`; server and client
journals are correlated by run/session identity, not merged into one writer.

The journal records module build start/success/failure, candidate validation,
activation, and rollback with the module name, old/new generation, source hash,
and relevant simulation tick. Feature behavior events identify the generation
that made the decision. Events remain transition-based and bounded; a module
must not log every actor decision every frame merely to establish identity.

Illustrative records for one server process:

```json
{"event":"module.activated","run_id":"run-id","instance_id":"server-01","pid":1234,"build_id":"exe-id:sha256","module":"npc-behavior","generation":13,"code_hash":"sha256","tick":820}
{"event":"npc.utility-decision","run_id":"run-id","instance_id":"server-01","pid":1234,"build_id":"exe-id:sha256","module":"npc-behavior","generation":13,"actor":41,"goal":"approach","action":"move_to_target","tick":821}
```

The active module map is also recorded in `run.json`/process-start evidence so
the journal remains attributable even when a module makes no decision change
immediately after activation. The first relevant post-activation decision is
logged once for the exercised actor, then ordinary change-edge or bounded
sampling rules apply. Runtime records use `StructuredLogger` and the
canonical per-executable journal; module code must not add a second file logger
or use console text as the only proof of activation or behavior.

Authoritative NPC and gameplay decisions are proved from the server process's
journal. A module active only in a client proves presentation behavior, not
server authority. Modules that affect both server and client behavior must use
a generation-compatibility gate: they may not silently apply conflicting
authoritative rules. Until the live protocol can confirm compatible code and a
safe shared switch, activation is deferred to a session/round boundary or
restricted to the authoritative process.

Build success alone is not behavior proof. A runtime trial uses the exact
running process and journal, confirms the new module generation activated,
exercises the real behavior, and checks the owner-level inputs, decision, and
result. Visual or gameplay acceptance is reported separately.

## 5. First proof: NPC utility goal/action selection

The first vertical slice moves only NPC utility goal/action selection behind a
hot module. The current decision path is in `src/npc/npc.cpp::senseWorld`,
using `selectUtilityGoal` and `actionForGoal` from `src/npc/npc-utility.cpp`.
The authoritative server NPC simulation is driven through
`src/network/server-npcs.cpp::simulateSharedNpcs`. Existing bounded evidence
includes `npc.target-sense`, `npc.utility-decision`, `npc.pursuit-state`,
`npc.movement-decision`, and `npc.target-changed`.

The module receives a stable observation/context built by the host, including
the actor's identity, current position/health/team, target relationship and
perception result, relevant objective/hazard facts, and versioned module-owned
decision state. It returns a goal/action decision. The host applies the result
through existing navigation, movement, combat, collision, and damage systems.
This first slice does not move NPC physics, weapon execution, collision
mechanisms, lifecycle, or network transport into the module.

An NPC C++ save builds the candidate in the background. The authoritative
server validates it and activates it only at a fixed simulation-tick boundary.
The module's decision state migrates without resetting NPC position, health,
target, life, or the match. The first trial verifies a real NPC encounter and
shows, in the same server `events.jsonl`, the old generation, activation tick,
first decision from the new generation, and the resulting movement/action
state. Compilation or reload failure leaves the old NPC decision generation
active.

The legacy decision implementation may remain as a temporary migration fallback
until a real scenario proves parity and last-known-good behavior. Exactly one
decision owner may control an NPC in a tick; the cold fallback and module must
not both apply decisions.

## 6. All-systems migration inventory

The target is a source-backed inventory for every active subsystem. It records
the current native-code owner, current hot-reload status, intended module
boundary, state owner/capabilities, dependencies, migration order, and one
runtime proof per behavior. The initial inventory is:

| Subsystem | Current native-code status | Kernel state/services and module dependencies | Migration order and real behavior proof |
|---|---|---|---|
| NPC decisions | Native decision, navigation, combat, and lifecycle owners remain in `src/npc/` and server adapters; profile/config data has separate reload owners. | Host actor state and perception/context; returned intent uses existing actor-action capabilities. | **1 — first proof:** utility goal/action selection; exercise a real server NPC encounter and correlate decision, action, position/target result, and generation in the server journal. |
| Player movement and collision | Native movement and physics owners remain EXE code; collision settings have their own config reload owners. | Host Transform/Velocity/runtime state, fixed tick, cached broadphase, collision queries, and result application. | **2:** move a bounded movement policy after NPC intent works; replay a human-observed movement/collision case and inspect fixed-tick state and contact evidence. |
| Weapons and projectiles | Native combat and server execution remain EXE code; weapon definitions are separate data. | Host action validation, ammo/cooldown state, physics contact, damage application, and replication. | **3:** migrate a bounded weapon or projectile policy; use a real fire/impact trial and inspect accepted action, contact, damage, and resulting state. |
| Match and gamemode rules | Native server/match owners remain EXE code; mode data is separately loaded. | Host match state, authority, lifecycle mechanisms, and generic event dispatch. | **4:** migrate one round rule after actor/action state is stable; exercise a full real phase transition and score/result event. |
| Map editor and creation tools | Native editor/tool code remains EXE code; project data/storage has its own owner. | Host project persistence, entity/resource handles, input, renderer, and window lifetime. | **5:** migrate one editor action; change a real project, save/reopen it, inspect journal records, and review the visible editor result. |
| UI, animation, and presentation | Native update/draw code remains EXE code; selected layouts/configs/resources reload as data. | Host GPU/device/resource lifetimes, UI state, and render-boundary coordination. | **5:** migrate one presentation or interaction owner after host resource lifetime is documented; verify the live generation and visible result. |
| Networking policy | Native prediction/reconciliation/relevance and transport code remain EXE code. | Host sockets, packet framing, connection lifecycle, server authority, and module-generation compatibility. | **6:** migrate policy only after two-process generation agreement is proven; verify server/client journals and a real connected session. |
| World generation, effects, and audio policy | Native behavior remains EXE code except the narrow effect/debug DLL pilot; asset/config data may have separate reload paths. | Host world/asset IO, effect pools, audio device, and resource lifetimes. | **7:** migrate bounded policies independently; exercise a real generated-world or audio/effect path and inspect owning-process evidence. |
| Process, platform, and runtime kernel | EXE-owned process, platform, scheduler, storage, loader, logger, renderer/device, transport, and physics mechanisms. | This is the stable service boundary used by modules. | **0 and continuous:** install and verify the module/state/diagnostic substrate first; keep mechanisms cold unless a separate design proves a safe replacement boundary. |

This is a migration map, not a claim that every row has an existing module.
Before promoting any row to implemented, trace the active source owner and
prove its real module build, load, behavior, and journal. A module is one
cohesive subsystem target, not one DLL per `.cpp` file.

## 7. Timing baseline and improvement measures

Do not promise zero seconds or invent a speedup. Before setting numerical
targets, measure representative edit-to-evidence cycles and report their
individual stages:

- issue-to-diagnosis and diagnosis-to-saved-edit time;
- save-to-detection and debounce/queue time;
- affected translation units, compile time, link time, validation time, and
  candidate-to-activation time;
- activation-to-first relevant owner event and time to inspect decisive
  `events.jsonl` evidence;
- total time from diagnosed issue to verified next trial.

Measure at least ten repeated trials for each workload: a no-op/save with no
module input change, one internal NPC module `.cpp` edit, and a shared module
interface/header change. Report the median and range initially; do not call a
small sample a stable p95. Record warm/cold cache state, compiler/toolchain,
changed-file/dependency count, and build generation. Set and review latency
targets only after these measurements. The aim is to make a normal module edit
approach the compile/link/activation floor, then reduce repeated human setup and
evidence-search time through automatic testing and journal correlation.

## 8. Acceptance gates

The module workflow is acceptable only when all of the following are shown in
the real game process:

- Saving an internal NPC module `.cpp` starts the module build automatically;
  a successful change activates without changing the EXE build identity or
  restarting the process.
- The NPC decision event after activation identifies the new module generation
  and is correlated with its real fixed-tick action and resulting actor state.
- Host-owned actor/world state survives activation unchanged except for changes
  intentionally caused by normal gameplay decisions.
- Compile, ABI, dependency, and state-schema failures leave the last known good
  module and state active, with an actionable reason in the dev-loop result
  and process journal.
- Activation happens only at the authoritative fixed-tick boundary for
  gameplay modules, does not cause render-frame-driven gameplay, and preserves
  the 60 Hz fixed-tick contract.
- A source edit rebuilds only its owning module and dependency-affected modules;
  unrelated modules are not relinked. Shared-interface edits rebuild their
  actual dependents.
- One real scenario exercises the changed behavior. The journal identifies the
  exact EXE and module generation and shows inputs, decision, action, and result.
  A missing required path is `NOT_COVERED`, not a pass. Human visual/feel
  acceptance is reported separately when appearance or feel is part of the
  change.
- Each all-systems inventory row advances only after its own owner-level
  runtime proof, safe failure/rollback proof, and source-backed status update.

## 9. Source-checked implementation status (2026-11-07)

The tracked source currently contains a narrow `build_game_dll.py` target with
`src/effects/effect-part.cpp` and
`src/hot-reload/quick-hit-debug-visual.cpp`. `HotReloadSystem` watches that
focused source set (and its API header), invokes `python build_game_dll.py`
synchronously, validates `GameAPI`, copies/loads a unique DLL path, and swaps
the small function table. This does not provide a general module catalog,
background per-subsystem build, authoritative tick activation transaction,
generic module-state migration, or module generation fields in all NPC events.

The current NPC utility selection, movement, combat, and lifecycle code remains
in the normal EXE source path. The tracked `src/hot-reload` tree does not contain
the historical `src/hot-reload/modules/` implementation described in older
architecture notes and changelogs. Those notes are historical evidence only;
they must not be used as proof of current source behavior. The current
source-backed inventory in section 6 and fresh runtime evidence are authoritative
for migration status.

The first kernel/loader installation will require an intentional full EXE build.
Thereafter, native behavior edits wholly contained behind a validated module
interface should compile and link only that module and activate in the running
process. Kernel/API edits remain a full-build boundary.

## JSON hot reload

Status recorded: 2026-09-08T14:13:15Z / 2026-09-08T10:13:15-04:00 (Eastern Daylight Time).

Status recorded: 2026-09-08T14:13:15Z / 2026-09-08T10:13:15-04:00 (Eastern Daylight Time).

This document describes the implementation currently present in the repository.
The GUI behavior specification remains authoritative: a GUI element's
presentation must be JSON-defined and hot reloadable. This document records
which paths currently satisfy that rule and which still need migration.

## Master gate and polling

The master switch is `config/hotreload.json`. The owner is
`src/engine/engine-tick-setup.cpp`.

1. `engine-tick-setup.cpp:53-75` reads the master gate once at startup.
2. A missing or malformed file defaults hot reload to enabled.
3. `engine-tick-setup.cpp:100-102` polls configuration every ten frames when
   the gate is enabled.
4. `engine-tick-setup.cpp:104-147` calls the individual reload owners,
   including `GamemodeRegistry::pollReload()` and
   `GuiLayoutManager::pollReload()`.

At approximately 60 FPS, the ten-frame throttle means a changed GUI file is
normally noticed within about 0.17 seconds, subject to frame timing and file
save behavior.

## Ordinary GUI layouts

`src/gui/gui-layout.cpp:82-287` owns parsing. It supports the v2 array format
and v3 object format. A successful parse replaces the element map and records
the file timestamp.

When parsing fails, the loader returns false and leaves the prior element map
active. Therefore malformed JSON does not intentionally erase a working GUI.

`src/gui/gui-layout.cpp:445-450` compares the current file timestamp with the
last accepted timestamp. `src/gui/gui-layout.cpp:485-510` reloads changed
layouts and reports failures while retaining the previous valid layout.

The terminal commands are registered in
`src/gui/gui-editor-commands.cpp`:

- `gui_load <filepath>` at lines 53-65 explicitly reloads one layout.
- `gui_debug_layout` at lines 150-177 lists known layouts and reports that
  polling is active.

## Gamemode metadata GUI

`config/gui/gamemode-meta-gui.json` is the single presentation file for
gamemode-specific HUD sections. Its top-level `gamemodes` object is keyed by
the gamemode ID from `config/gamemodes/*.json`.

`src/gui/gui-layout.cpp` reuses the normal `GuiElement` parser for a selected
section. `GuiLayoutManager::getGamemodeLayout()` normalizes network names to
the current gamemode IDs:

- `team_deathmatch` -> `tdm`
- `free_for_all` -> `ffa`
- `bomb_tag` -> `bombtag`
- `duel` -> `duel`
- `sandbox` -> `sandbox`

Existing loaded gamemode sections are checked by the same
`GuiLayoutManager::pollReload()` path. The active HUD reads the new section on
the next UI render after a successful reload.

The migrated consumers are:

- `src/engine/engine-tick-ui-overlays.cpp:557-621` for community match title,
  TDM score, countdown, intermission, and time-left presentation;
- `src/gui/hud/match-leaderboard.cpp:37-195` for FFA/TDM leaderboard and
  score-gain presentation;
- `src/duel/duel-ui.cpp:192-277` for network duel presentation;
- `src/game/duel.cpp:241-279` for local duel presentation.
- `src/game/gamemode-manager.cpp:266-301` for Bomb Tag HUD presentation.

## Current limitations and unresolved paths

The following visible presentation was still hardcoded or only partially
JSON-backed at the time this document was recorded:

- `src/engine/engine-tick-ui-overlays.cpp:541-547` draws room status with
  hardcoded position, scale, and colors.
- `src/engine/engine-tick-ui-replay-hud.cpp:95-109` assembles reconnect text
  in C++ rather than using a JSON template for the complete presentation.
- `src/engine/engine-tick-ui-replay-hud.cpp:111-121` draws the replay-record
  indicator with hardcoded coordinates, text, scale, and colors.
- `src/game/duel.cpp:301-303` contains a duel result score path with
  hardcoded presentation values.

These remain unresolved until a runtime test proves that editing their owning
JSON definition changes the visible result without a rebuild or restart.

## Safe editing workflow

1. Keep `config/hotreload.json` enabled.
2. Edit a JSON presentation value and save it.
3. Wait for the next polling interval.
4. Confirm the relevant reload diagnostic and visible result.
5. If JSON is malformed, fix the file; the last valid layout should remain
   active.

Source parsing and reload success are not the same as visual acceptance. A
live game or replay capture is still required to prove that the intended HUD
element changed and that the correct owner is being rendered.
