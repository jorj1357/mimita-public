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

This specification defines the target and a map-editor proof slice. It does not
claim that the general module workflow is implemented today. Full integration
and release builds remain required when the kernel, module ABI, or other
non-reloadable mechanism changes.

## 2. Module and kernel responsibilities

### 2.1 Module ownership

A module is one cohesive behavior boundary such as `map-editor`,
`npc-behavior`, `movement-policy`, or `weapon-policy`. Its ordinary source files
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
available while module code is replaced. Authored map/project documents and
their dirty/saved revisions likewise remain in host-owned registry/storage;
reloading editor code must never reload the document from disk or discard
unsaved edits.

Modules read state through stable, documented host capabilities and request
changes through the owning gameplay system. A module must not bypass authority,
damage validation, collision, fixed-tick execution, or the host's state owner.
For example, the map-editor module may request an authored transform edit; the
host validates the target and applies it through the map registry, undo history,
geometry, collision, and persistence services. An NPC behavior module may
request an actor action; the host still validates and applies movement,
attacks, health changes, and collision using their existing authoritative
mechanisms.

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
module, its generation, source/content hash, ABI version, and activation
boundary (simulation tick for gameplay or editor frame for the map editor).
Each process writes its own canonical `events.jsonl`; server and client
journals are correlated by run/session identity, not merged into one writer.

The journal records module build start/success/failure, candidate validation,
activation, and rollback with the module name, old/new generation, source hash,
and relevant simulation tick or subsystem activation boundary. Feature
behavior events identify the generation that handled the operation. Events
remain transition-based and bounded; a module must not log every editor frame
merely to establish identity.

Illustrative records for one client process editing a map:

```json
{"event":"module.activated","run_id":"run-id","instance_id":"client-01","pid":1234,"build_id":"exe-id:sha256","module":"map-editor","generation":3,"code_hash":"sha256","boundary":"before_editor_frame","frame":420,"tick":820}
{"event":"map-editor.transform.apply","run_id":"run-id","instance_id":"client-01","pid":1234,"build_id":"exe-id:sha256","module":"map-editor","generation":3,"map":"proof_map","object":"proof_block","field":"position","before":[0,0,0],"after":[3,0,0],"source":"property_panel","revision":18,"operation_sequence":23,"frame":421,"tick":821}
{"event":"map-editor.autosaved","run_id":"run-id","instance_id":"client-01","pid":1234,"build_id":"exe-id:sha256","module":"map-editor","generation":3,"map":"proof_map","path":"config/maps/proof_map.json","revision":18,"frame":448,"tick":848}
```

The active module map is also recorded in `run.json`/process-start evidence so
the journal remains attributable even when a module makes no decision change
immediately after activation. The first relevant post-activation operation is
logged once for the exercised map/object, then ordinary change-edge or bounded
sampling rules apply. Runtime records use `StructuredLogger` and the
canonical per-executable journal; module code must not add a second file logger
or use console text as the only proof of activation or behavior.

Local map-editor behavior is proved from the client/editor process that accepted
the edit, using that process's journal and the exact project document it
changed. This first proof is local authoring. Collaborative world editing is a
later server-authoritative slice and must separately prove accepted operations,
revision agreement, and client generation compatibility before editor code
that affects shared state activates across a session.

Authoritative NPC and gameplay decisions are proved from the server process's
journal. Modules that affect both server and client behavior must use a
generation-compatibility gate: they may not silently apply conflicting
authoritative rules. Until the live protocol can confirm compatible code and a
safe shared switch, activation is deferred to a session/round boundary or
restricted to the authoritative process.

Build success alone is not behavior proof. A runtime trial uses the exact
running process and journal, confirms the new module generation activated,
exercises the real behavior, and checks the owner-level inputs, decision, and
result. Visual or gameplay acceptance is reported separately.

## 5. First proof: map-editor transform and save

The first module goal is to make all map-editor-specific C++ behavior
reloadable as one `map-editor` module. Related editor files are grouped into
one module; they do not each become separate DLLs. The target covers the
viewport, selection/outliner, property editing, gizmos, add/delete/duplicate,
undo/redo, editor commands, chunk/world-edit tools, project/editor workflows,
editor overlays, and editor mode/input policy. New or renamed editor behavior
files are assigned to this module through the source catalog automatically.

The first live proof is deliberately one end-to-end action: select a
map-authored block, change its position in the properties panel, allow the
existing debounced autosave, reload that same map, and confirm the block still
appears at the new position. This proves that a new module generation handled
a real editor input, changed the visible scene, and persisted the result; an
activation message by itself is insufficient.

### 5.1 Source path and module membership

The current property-edit path is:

```text
src/input/input-commands.cpp::MapEditor::handleTextKey
  -> src/mapeditor/map-editor.cpp::handleTextKey / applyPropertyPosition
  -> MapEditor::setSelectedPosition / nudgeSelection
  -> MapConfigRegistry authored-data mutation + markChanged
  -> src/mapeditor/map-editor.cpp::tick / autosave
  -> src/gamemode/map-config.cpp::MapConfigRegistry::save
  -> config/maps/<map-id>.json
  -> MapEditor::reload / MapConfigRegistry::load and viewport redraw
```

`src/mapeditor/*.cpp` is the initial source group, including
`map-editor.cpp`, `map-editor-commands.cpp`, `world-edit-commands.cpp`,
`map-editor-projects.cpp`, and editor-specific configuration application.
Editor behavior embedded at current integration sites such as
`src/engine/engine-tick-camera.cpp`, `src/engine/engine-tick-ui.cpp`,
`src/engine/engine-tick-render.cpp`, `src/engine/engine-tick.cpp`,
`src/input/input-commands.cpp`, and `src/main-init.cpp` moves behind the same
module entry points. Those EXE call sites remain thin dispatch adapters; they
must not retain a second editor implementation or policy branch.

The editor module owns editor behavior: interpreting editor input, selection,
property and gizmo actions, command behavior, undo/redo policy, editor UI and
world overlays, and deciding which host edit services to call. The host owns
the window and raw platform input, renderer/GPU/device lifetime, map/project
document and dirty revision in `MapConfigRegistry`, JSON/file persistence,
atomic save/backup, world geometry and collision mechanisms, and stable object
handles. `MapEditorConfig` values and GUI layouts remain hot-reloadable data;
they are passed to the module as validated settings rather than compiled into
the module.

The module boundary uses versioned plain-data inputs/results and opaque host
handles. It does not pass `MapEditor*`, `MapConfigRegistry&`, `GLFWwindow*`,
`Camera&`, STL containers, or module-owned pointers across the DLL ABI. The
host provides explicit capabilities for map-object lookup/edit transactions,
undo snapshots, geometry/collision refresh, save/reload, input state, and
render/UI commands. The persistent map document stays in the host. The editor's
transient state—active/frozen mode, selected stable IDs, clipboard, text
fields, undo/redo history, drag state, and pending autosave data—is captured
and restored through a versioned schema. A reload must preserve unsaved edits
and may not replace them by reading the last saved JSON again.

### 5.2 Safe reload and behavior parity

The module compiles in the background when a mapped editor C++ source is saved.
The candidate is validated and activated on the main thread at a safe editor
boundary: after the current editor callback returns and before the next input,
update, or render callback starts. Never unload a generation while its code is
executing. Preserve the current editor session state and map revision during
the swap. A failed build, ABI check, capability check, or state migration
leaves the previous editor generation active and reports the failure.

The initial extraction copies the current editor behavior without changing
selection, transform math, undo/redo, geometry refresh, save semantics, or
commands. For the first proof, the property edit must have the same before/after
position and saved JSON as the current code. During migration, a candidate may
be compared with the previous implementation using copies of the same editor
state and operation; only one result may mutate the map document. After parity
is established, a later editor behavior fix must state one intended change,
the exact input/action, the expected visible and persisted result, and which
neighboring editor operations remain unchanged.

After the one-time host/module-loader installation build, make and save an
internal, behavior-preserving edit to a `src/mapeditor/*.cpp` implementation.
Verify that the running process loads the new `map-editor` generation without
rebuilding or restarting the EXE, then perform the property-position scenario
through that generation. This proves source-save activation and real editor
behavior in the same trial.

### 5.3 Journal proof and contributor checklist

The editor process journal identifies the EXE, active `map-editor` generation,
project/map ID, stable object ID, prior/new property values, operation sequence,
dirty/saved revision, and source hash. Extend the existing bounded
`map-editor.transform.apply` event to cover position edits; the current source
logs transform events for some fields, while `MapEditor::setSelectedPosition`
currently does not emit a position event. Correlate it with
`map-editor.autosaved`, a `map-editor.reload.result` event, and a read-back of
the exact map JSON. The reload event reports map ID, success/failure, resulting
revision, and module generation; failed reload preserves the last valid map
state. Each record uses the canonical process identity and journal; do not
write a second debug file. Human-visible confirmation of the block at the new
position is reported separately from source, build, and JSON evidence.

Before changing editor code, the contributor records:

1. the one editor behavior being changed and its module owner;
2. the input-to-edit-to-render/save path and expected result;
3. adjacent behavior held constant (selection, undo, collision, autosave,
   commands, or other editor surfaces);
4. the real map/project scenario and exact journal fields proving success; and
5. any path the run did not reach, marked `NOT_COVERED`.

Do not bundle separate editor failures into a general cleanup. If the object
was not selected, the property input was rejected, the document did not become
dirty, autosave did not run, or the reload did not read the edited map, record
the first missing step and fix that owner before changing a neighboring one.

This first proof is local map authoring. Collaborative project editing,
multi-client revision agreement, conflict resolution, and server-authoritative
edit permissions remain a later project/network migration and require their
own process journals and acceptance scenarios.

### 5.4 Second module: shared world rendering and shader assets

The shared world rendering boundary is the `world-rendering` module. Its target
scope covers authored-box face construction and face/culling analysis for
`src/render/render-world.cpp`, `src/render/render-world-mesh.cpp`,
`src/world/streamed-world-layer.cpp`, and the renderer side of
`src/world/world-gltf-loader.cpp`. The host keeps GPU/device lifetime, texture
handles, world/collision data, file IO, and the actual GL draw submission. The
implemented first slice owns the authored-box face/culling calculation; moving
the full draw submission and upload path remains the next renderer slice.

The host owns a shader program service. `shaders/basic.vert` and
`shaders/basic.frag` are asset inputs, not C++ translation units. Saving either
file compiles and links a candidate program on the main thread at a frame
boundary. A successful program becomes the active world program; a failed
compile or link leaves the prior program active and emits
`shader.compile.failed` with the source hash. Shader generations are independent
of module generations.

The module host uses a generic catalog at `config/modules/modules.json`. Each
entry declares source/header globs, ABI version, state schema, activation
boundary, and required host services. `devscripts/module-build-worker.py`
captures a source snapshot, builds an immutable generation DLL under
`build/modules/<module>/generation-<n>/`, and publishes `current.json`. The
running process validates name, ABI, state schema, generation, and source hash
before activating. Build, validation, interface, or state-migration failure
keeps the last active generation.

A controlled proof map, `config/maps/hot_render_proof.json`, contains one
authored box with six distinct per-face materials. Authored blocks now support
`faces[6]`; empty entries fall back to `material`. The renderer records
`world-render.face-template` at generation, `world-render.mesh-upload` on mesh
revision change, `world-render.draw-summary` when culling or revision changes,
and `world-render.face-audit` when `worldrender audit_faces <blockId>` runs.
The audit reports each face's normal, winding, material, submitted batch range,
and whether the camera is outside it with culling enabled. Human visual review
remains a separate claim; an unreached face is `NOT_COVERED`.

This six-face scenario verifies the hot rendering path. It does not claim that an
unspecified existing missing-face bug has been diagnosed or fixed.

## 6. All-systems migration inventory

The target is a source-backed inventory for every active subsystem. It records
the current native-code owner, current hot-reload status, intended module
boundary, state owner/capabilities, dependencies, migration order, and one
runtime proof per behavior. The initial inventory is:

| Subsystem | Current native-code status | Kernel state/services and module dependencies | Migration order and real behavior proof |
|---|---|---|---|
| Map editor and creation tools | `src/mapeditor/*.cpp` and editor policy at engine/input call sites remain native EXE code; map document persistence and JSON settings have separate owners. | Host map registry/document, input/window, renderer, geometry/collision, stable object handles, and save/backup services; module owns editor interaction and authoring behavior. | **1 — first proof:** edit an authored block position through the property panel, verify the same visible result, autosave and reload the exact map JSON, and correlate edit/revision/generation in the client/editor journal. |
| NPC decisions | Native selection, navigation, combat, and lifecycle owners remain in `src/npc/` and server adapters; profile/config data has separate reload owners. | Host actor state and perception/context; module returns the utility goal/action result to the host. | **2:** after the editor module proves stateful reload, port the existing utility decisions without behavior changes and prove one live NPC path. |
| Player movement and collision | Native movement and physics owners remain EXE code; collision settings have their own config reload owners. | Host Transform/Velocity/runtime state, fixed tick, cached broadphase, collision queries, and result application. | **3:** move a bounded movement policy after NPC decision ownership is migrated; replay a human-observed movement/collision case and inspect fixed-tick state and contact evidence. |
| Weapons and projectiles | Native combat and server execution remain EXE code; weapon definitions are separate data. | Host action validation, ammo/cooldown state, physics contact, damage application, and replication. | **4:** migrate a bounded weapon or projectile policy; use a real fire/impact trial and inspect accepted action, contact, damage, and resulting state. |
| Match and gamemode rules | Native server/match owners remain EXE code; mode data is separately loaded. | Host match state, authority, lifecycle mechanisms, and generic event dispatch. | **5:** migrate one round rule after actor/action state is stable; exercise a full real phase transition and score/result event. |
| UI, animation, and presentation | Native update/draw code remains EXE code; selected layouts/configs/resources reload as data. | Host GPU/device/resource lifetimes, UI state, and render-boundary coordination. | **6:** migrate one presentation or interaction owner after host resource lifetime is documented; verify the live generation and visible result. |
| Networking policy | Native prediction/reconciliation/relevance and transport code remain EXE code. | Host sockets, packet framing, connection lifecycle, server authority, and module-generation compatibility. | **7:** migrate policy only after two-process generation agreement is proven; verify server/client journals and a real connected session. |
| World generation, effects, and audio policy | Native behavior remains EXE code except the narrow effect/debug DLL pilot; asset/config data may have separate reload paths. | Host world/asset IO, effect pools, audio device, and resource lifetimes. | **8:** migrate bounded policies independently; exercise a real generated-world or audio/effect path and inspect owning-process evidence. |
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
module input change, one internal map-editor module `.cpp` edit, and a shared
module interface/header change. Report the median and range initially; do not
call a small sample a stable p95. Record warm/cold cache state, compiler/toolchain,
changed-file/dependency count, and build generation. Set and review latency
targets only after these measurements. The aim is to make a normal module edit
approach the compile/link/activation floor, then reduce repeated human setup and
evidence-search time through automatic testing and journal correlation.

## 8. Acceptance gates

The module workflow is acceptable only when all of the following are shown in
the real game process:

- Saving an internal `src/mapeditor/*.cpp` file starts the `map-editor` module
  build automatically. A successful edit activates without changing the EXE
  build identity or restarting the process.
- The editor module inventory covers viewport input, selection/outliner,
  property editing, gizmos, add/delete/duplicate, undo/redo, terminal commands,
  chunk/world editing, projects, overlays, and editor mode policy. Remaining
  EXE code is limited to stable host dispatch and core platform/render/storage
  services; no second editor behavior owner remains.
- The first property-position proof records the active generation, exact map
  and object IDs, old/new positions, project revision, autosave, and reload.
  The reloaded document contains the new value and the viewport shows that same
  object at that position. The tested source save occurs after the initial
  module-loader EXE installation and before this edit operation.
- Behavior-preserving extraction produces the same selected object, transform,
  undo/redo result, map revision, and save output for the same operation before
  an intentional editor behavior change is accepted.
- Host-owned project/document state and unsaved edits survive module activation;
  editor state migration preserves stable selection IDs, undo/redo history,
  clipboard, active/frozen mode, pending operation, and save status where
  compatible. Incompatible migration leaves the old module and document active.
- Compile, ABI, dependency, and state-schema failures leave the last known good
  module and state active, with an actionable reason in the dev-loop result
  and process journal.
- Gameplay modules activate only at the authoritative fixed-tick boundary and
  preserve the 60 Hz contract. The local editor module activates on the main
  thread after the current editor callback and before the next input, update,
  or render callback; it is never unloaded while executing.
- A source edit rebuilds only its owning module and dependency-affected modules;
  unrelated modules are not relinked. Shared-interface edits rebuild their
  actual dependents.
- One real local editing scenario exercises the module-driven behavior. The
  exact editor process journal and map JSON show the input, edit result,
  revision, save, reload, and visible outcome. A missing required path is
  `NOT_COVERED`, not a pass. Human visual review remains separate from source,
  build, and persistence evidence.
- Each all-systems inventory row advances only after its own owner-level
  runtime proof, safe failure/rollback proof, and source-backed status update.

## 9. Source-checked implementation status (2026-11-07)

The tracked source currently contains a narrow `build_game_dll.py` target with
`src/effects/effect-part.cpp` and
`src/hot-reload/quick-hit-debug-visual.cpp`. `HotReloadSystem` watches that
focused source set (and its API header), invokes `python build_game_dll.py`
synchronously, validates `GameAPI`, copies/loads a unique DLL path, and swaps
the small function table. This does not provide a general module catalog,
background per-subsystem build, subsystem-boundary activation transaction,
generic module-state migration, or module generation fields in editor events.

The current map editor remains normal EXE source code in `src/mapeditor/` with
direct integration calls from engine/input files. `MapConfigRegistry` owns
authored map data and persistence; the editor's C++ behavior is not currently
an independently reloadable module. The current NPC utility selection,
movement, combat, and lifecycle code also remains in the normal EXE source path.
The tracked `src/hot-reload` tree does not contain
the historical `src/hot-reload/modules/` implementation described in older
architecture notes and changelogs. Those notes are historical evidence only;
they must not be used as proof of current source behavior. The current
source-backed inventory in section 6 and fresh runtime evidence are authoritative
for migration status.

The first kernel/loader installation will require an intentional full EXE build.
Thereafter, native behavior edits wholly contained behind a validated module
interface should compile and link only that module and activate in the running
process. Kernel/API edits remain a full-build boundary.

### Implemented first slice (2026-11-07)

The tracked source now contains the generic catalog/worker/host substrate, a
live `map-editor` module that owns the property-position decision, a live
`world-rendering` module that owns authored-box face/culling analysis, and a
host-owned shader compile/swap service. The map-module position path calls the
host document/undo/collision/persistence services; the host keeps selection and
the map document. The world-rendering module currently owns policy calculation;
moving the full draw submission and world loader upload path behind the same
boundary remains the next renderer slice.

Source-save activation was verified in one running client: `map-editor`
generations 2 and 3 and `world-rendering` generations 1 and 2 activated without
restarting, a property edit was handled by module generation 3 and autosaved,
and a six-face audit was handled by world-rendering generation 2. A valid shader
swap and a failed candidate that retained the prior program were also observed.
A later trial recorded `module.build.failed` with the exact compiler error while
`map-editor` generation 3 kept handling edits; reverting the source activated
generation 4 in the same PID. The full draw-path extraction, complete
editor-state schema ownership, and the remaining acceptance scenarios are still
future work and must not be reported as complete.

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
