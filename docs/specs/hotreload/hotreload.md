2026 11 11 1507 jorj

"Finish the inventory. Map each .cpp to its dependencies and identify which code owns persistent state, active callbacks, and background jobs. python devscripts/module_ownership.py already checks current source ownership; extend that inventory for the planned runtime and game targets.

Install the runtime/game boundary. Add a resident runtime.dll for persistent state and coordination, and a whole-game lifecycle API for game.dll. The EXE becomes a small launcher. The current EXE does not have this boundary, so installing it requires one initial EXE build. After that, ordinary game-code edits should not relink the EXE.

Prove one NPC path. Keep the same NPCs and world alive, edit NPC C++, build a candidate game.dll, and activate it in the same process. Prove state continuity, compile-failure fallback, rollback, and journal evidence.

Expand the default source ownership. Once that proof works, route ordinary implementation .cpp files to one game.dll. Split the DLL only if measured link times make that useful.

Add the one-command Python loop. A command such as python devscripts/live.py should watch saves, snapshot inputs, compile affected files in the background, and notify the runtime when a candidate is ready. It can automate the workflow; it cannot create the missing runtime boundary by itself."

do this all

# MiMITA Continuous Live Development

*Purpose: define the intended C++ edit-to-live loop, its architecture, migration phases, and proof requirements.*
*Audience: engine, gameplay, tools, build, and AI contributors.*
*This document distinguishes desired behavior from source-checked implementation status. It does not claim universal hot reload before the acceptance evidence exists.*

## 1. Goal and user-visible contract

Keep the same MiMITA process, world, players, NPCs, vehicles, documents, and connected sessions alive while ordinary C++ implementation changes become active. The intended loop is:

```text
edit a game-owned .cpp and save
  -> detect the completed save automatically
  -> compile affected translation units and link a candidate game.dll in the background
  -> validate the candidate and any state migration
  -> activate at a safe simulation/render boundary
  -> keep the same process and runtime state
  -> inspect the exact process events.jsonl and exercise the behavior
```

For ordinary game-code edits, “no build” means no manual build request, no full EXE relink, and no process restart. Native C++ still has to compile and link to machine code. Save-to-live latency must be measured by stage; it is not assumed to be one second or constant for every edit.

The first implementation target is one large `game.dll`. Split it into multiple code DLLs only if measured compile/link/activation times, ownership, or failure isolation justify the extra boundaries. A source file is not its own DLL.

## 2. Agreed architecture

### 2.1 Bootstrap EXE

The EXE is the small process entry point. It parses launch mode and starts the resident runtime. It does not contain ordinary game behavior. The same process remains open while game-code generations change.

### 2.2 Resident `runtime.dll`

`runtime.dll` stays loaded across `game.dll` generations. It owns persistent runtime data and generic services, including entity/component records, player and NPC state, world/session state, connected-peer state, active-generation agreement, and safe activation coordination.

The runtime data store is schema-aware and generic. It does not require one fixed C++ struct to gain a new member every time game code adds a field. Runtime records have entity/type identity and schema version; new game generations provide migration/default logic for records they own. Migrations are validated and committed atomically with code activation. A failed migration leaves the old generation and old state active. The hot per-tick path must use batched or direct stable-handle access; it must not perform reflective field-name lookups for every NPC field on every tick. Schema conversion happens at generation activation, not continuously during play.

Keep the runtime's own editable behavior small. Changeable NPC, networking, rendering, physics, gameplay, GUI, and tool logic belongs in `game.dll`, even when it reads or changes runtime-owned data. The runtime API should be broad and versioned so ordinary behavior and component-field changes do not require a runtime rebuild. Only a genuinely missing stable mechanism or incompatible runtime ABI is a runtime boundary change; this is minimized, not claimed impossible forever.

### 2.3 Replaceable `game.dll`

Initially, ordinary game implementation `.cpp` files are assigned to one `game.dll` by default. The DLL receives a versioned runtime function table and stable handles; it owns behavior, not persistent world/session data.

Function signatures and C++ types inside `game.dll` may change freely when the affected code is rebuilt together. Across the runtime/game boundary, use a versioned ABI with size/version fields, plain data, handles, and explicit ownership. Do not pass STL containers, exceptions, allocator-owned objects, or pointers to short-lived game-DLL memory across that boundary.

Adding or changing a field such as position, velocity, or health is a game-owned schema change plus migration. It should not alter a fixed runtime C++ struct. The runtime preserves the record; the candidate game generation interprets or migrates it.

### 2.4 Multiplayer and graphics

The runtime preserves connection/session state and coordinates candidate-generation identity, compatibility, readiness, and activation timing. Changeable multiplayer rules live in `game.dll`. Use a generic versioned message/event envelope where possible so new behavior does not require a new runtime wire mechanism. A protocol feature that needs a capability the runtime does not provide is a deliberate boundary change.

Native-code sharing is for explicitly trusted development sessions. The host approves the candidate and all participating processes report compatible code/schema generations before activation. Public servers do not execute arbitrary client-supplied native code.

A live renderer/backend change may recreate the graphics device and re-upload resources while keeping the process and runtime state alive. It is limited to backends supported by the current platform, driver, and build. A backend unavailable on the current OS or hardware cannot be enabled by hot reload alone.

## 3. Save, build, validate, activate

The eventual human/AI entry point should be one command, proposed as:

```powershell
python devscripts/live.py
```

This command does not exist yet. It should start or attach to the canonical live-development coordinator; it must not require fixed sleeps or repeated manual status polling.

The coordinator must:

1. Watch the source inventory and debounce completed saves. Resolve each changed `.cpp` and header to the game or runtime owner. Ordinary game `.cpp` files default to `game.dll`; runtime/bootstrap files are explicitly listed. Ambiguous ownership is an error, never a silent cold-build fallback.
2. Snapshot consistent inputs. Compile only affected translation units using discovered header dependencies and a compiler cache, then link an immutable candidate DLL. If files change during a build, finish the captured snapshot and queue one newest follow-up.
3. Report source hash, changed files, compile/link/validation/activation times, diagnostics, and generation identity through events and the process journal. Compile errors leave the active generation untouched.
4. Validate ABI, dependency set, code hash, required runtime capabilities, and state-schema migration before activation.
5. Stop new calls into the old generation at a safe boundary and wait for active calls, callbacks, and jobs to finish. Activate gameplay at the fixed 60 Hz tick boundary and rendering/tool code at its owning safe boundary. Never unload code while a thread or stored callback can still execute it.
6. Keep the prior generation available for rollback. A failed candidate or a runtime regression must not discard the current world/session state.

This pipeline can automatically assign and build files after ownership rules exist. It cannot safely convert arbitrary code merely by changing a filename or compiler flag: source that directly owns persistent state, relies on EXE globals, or leaves callbacks/jobs pointing into a DLL needs a one-time boundary adaptation.

### 3.1 Concrete first-implementation runbook

This is the implementation order for the first working save-to-live loop. It deliberately starts in the existing repository with one NPC behavior slice; it does not require a new repository or a complete field-by-field audit of all 594 current source files before work can begin. Keep world loading on its current path during this proof. Expand the audit as each later source batch is moved.

#### Step 0: reuse what already exists and identify the seam

Before changing code, re-check the source inventory and these current implementation touchpoints:

| Existing path | Current job | How the pilot uses it |
|---|---|---|
| `build_game_dll.py` | Builds the existing focused game DLL source list. | Extend or replace its focused list for the pilot; do not mistake it for a whole-game build. |
| `src/hot-reload/game-api.h` | Declares the existing narrow `GameAPI` and export. | Evolve toward the versioned lifecycle/function-table boundary while preserving compatibility until the replacement is proven. |
| `src/hot-reload/hot-reload-system.cpp` | Loads candidates and currently triggers its focused DLL build synchronously. | Keep candidate loading/rollback ownership here or move it to the chosen resident owner; remove synchronous compilation from the game tick. |
| `src/engine/engine-tick-setup.cpp` | Calls the current reload check during engine setup/tick work. | Keep only a cheap notification/activation check at the safe boundary; never wait for compilation here. |
| `devscripts/module_ownership.py` and its catalog | Audits source-to-target ownership for existing subsystem modules and the EXE. | Extend the single ownership map to distinguish bootstrap, runtime, and game targets and reject duplicate/unassigned production owners. |
| `devscripts/cpp-source-inventory.py` | Records source/dependency and planned ownership evidence. | Keep it as the generated audit/report; the build coordinator should consume the canonical owner manifest rather than infer ownership from filenames. |

The current source-checked snapshot says the focused DLL contains `src/effects/effect-part.cpp` and `src/hot-reload/quick-hit-debug-visual.cpp`, while 582 sources are assigned to the EXE and 12 to subsystem modules. Re-check before implementation because the counts can change. The existing effect/debug API proves only that narrow feature path, not arbitrary C++ reload.

Use the mapped server NPC path as the first trace: `runServer -> simulateSharedNpcs -> NpcSystem::updateOneWithTarget -> NpcSystem::updateOneNpc`. The existing functions pass live C++ references such as `Npc&`, `World&`, `Player&`, and `NpcMovementContext&`; those references are not the permanent cross-DLL interface. Inspect the actual field reads/writes and choose the smallest behavior decision that can be represented as plain input/output data. If the selected decision needs world information, request it through a bounded runtime capability rather than passing a `World&` through the ABI.

#### Step 1: establish the persistent-data rule

- `runtime.dll` is resident code plus the owner of live process-memory records. The DLL file itself is not where changing player/NPC positions are stored.
- The runtime allocates and owns the state that must survive game-code replacement: for the pilot, at minimum the NPC's stable ID and the exact fields used by the selected behavior, plus whatever world/session identity is needed to prove the same test is continuing.
- Implement only the runtime record types and fields needed by this pilot, while following the generic entity/type/schema-version rules above. Do not migrate every entity/component or build a reflective field database before the first reload works.
- `game.dll` owns replaceable behavior and temporary implementation state. Do not put the only copy of NPC position, health, identity, or session state in a game-DLL global, static, heap allocation, callback closure, or thread-local object.
- The first proof is code-only: keep the state schema unchanged. This isolates “did code reload preserve the NPC?” from “did a data migration work?” Add field/schema migration after code-only replacement passes.
- If the process exits, runtime-owned memory also exits unless separately saved. Persistent-across-process saves remain a separate data-saving path; this plan promises continuity across a game-code DLL switch, not automatic disk persistence.

#### Step 2: install the one-time stable loader boundary

1. Define the minimum versioned C ABI needed for startup, querying runtime capabilities, game-module initialization, one NPC decision call, quiescence, and shutdown. Put the authoritative declaration in one shared header. Include ABI version and structure-size fields.
2. Pass fixed-width values, plain structs, byte spans, and stable entity handles across the boundary. Keep C++ classes, STL containers, references, exceptions, RTTI-dependent objects, and allocator ownership on one side.
3. Make the EXE load `runtime.dll`; make the resident runtime load and validate the candidate `game.dll`. Keep a stable function table in the runtime and call the active generation through it.
4. Create or update the agent-lane host target so this boundary is installed by one intentional initial EXE/runtime build. This is the expected cold boundary. After it is running, a game-owned `.cpp` edit must only compile/link a game DLL generation; it must not relink the EXE.
5. Retain current subsystem modules and the current world-loading implementation while the pilot is built. Do not copy the entire repository into a new repo or attempt to move world loading in the same boundary change.

Before proceeding, start the game once and confirm that the runtime and a trivial game module load, report their ABI/generation, reject an invalid candidate, and leave the process open. This verifies the loader before NPC behavior is mixed in.

#### Step 3: move one NPC decision behind the ABI

1. Keep the current server tick, entity lookup, world loading, physics/collision, and authoritative application in their existing owners for this proof.
2. At the chosen NPC decision seam, build a bounded `NpcBehaviorInputV1` from runtime-owned data. Include stable entity IDs and only the values that decision needs. Add an output struct containing the decision/result; do not return pointers into game-DLL memory.
3. If the decision needs a world/nav query, expose a versioned runtime capability that accepts stable handles and plain query/result structs. The runtime performs the query with current host-owned data; the game module chooses behavior from the result.
4. Validate the returned action in the authoritative owner, apply it to the same NPC record, and keep fixed-tick physics rules unchanged.
5. Initially leave the NPC object and complex systems in their current representation. Adapt only the selected decision seam. Later migrations can move whole subsystems once their ownership is clear.

The intended shape is:

```text
resident server tick looks up NPC state
  -> runtime makes plain versioned input
  -> game.dll computes NPC behavior
  -> runtime/server validates and applies result
  -> same NPC record continues to next tick
```

This is one ABI adaptation at the behavior boundary, not a wrapper added to every C++ function. Calls wholly inside `game.dll` remain ordinary C++ calls.

#### Step 4: make save trigger an asynchronous candidate build

- Use one watcher/coordinator owned by the existing dev-loop/module build system. `python devscripts/live.py` is the proposed human-facing entry point, but it does not exist yet. Do not start an independent competing build loop.
- Watch only the canonical source manifest and dependency graph. A saved game-owned source defaults to `game.dll`; runtime, bootstrap, tool, test, and data owners are explicit. Unknown ownership stops with a useful error instead of silently requesting an EXE build.
- On save, capture an immutable source revision (hash and changed paths). Compile only affected translation units plus required dependents, then link a candidate DLL in the approved repository-local lane path. Continue the running simulation while the build runs.
- Coalesce multiple saves while a build is active: finish the captured revision, then build the newest coherent revision. Never compile a file while it is only partially saved, and never publish a DLL assembled from mixed revisions.
- Publish to a unique immutable path such as `game-g000042.dll`; Windows cannot safely overwrite the currently loaded DLL. Validate exports, ABI version/size, runtime capabilities, dependency/source hash, and any state migration before notifying the runtime.
- Keep game-thread work bounded: compilation/linking belongs to the background coordinator. The runtime receives a ready-candidate event and activates only at the owning safe point.
- Report detected revision, affected translation units, compile/link/validation durations, candidate path/hash, and errors. The old generation remains active when a stage fails.

#### Step 5: activate, prove continuity, and roll back safely

At a fixed simulation boundary for NPC/gameplay code:

1. Stop dispatching new NPC calls to the old generation.
2. Unregister callbacks owned by that generation and prevent new jobs from entering it.
3. Drain/cancel and join old-generation jobs; wait for in-flight function calls to return.
4. Prepare and validate any required state migration without overwriting the live records.
5. Atomically change the runtime's active function table/generation.
6. Run a bounded first-call health check. On success, commit migration and mark the generation active. On failure, restore the old function table and old state.
7. Unload the old DLL only when no call, callback, job, function pointer, or return address can still enter it. Keep its immutable file available for rollback.

Record the EXE process ID, map/mode, NPC ID, NPC position/health before and after, active DLL generation, changed source revision, activation event, behavior result, frame/tick measurements, and exact `events.jsonl`. For a no-op/failed candidate, assert the previous generation remains active. For rollback, assert world/NPC data is unchanged.

#### Step 6: use an explicit proof ladder

| Proof | Edit or failure | Required observation in the same process |
|---|---|---|
| Loader proof | Valid trivial game generation | ABI accepted; generation changes; EXE PID stays the same. |
| NPC behavior proof | Change one NPC decision constant/math expression | Same NPC ID/world remains; visible behavior changes; no EXE link/restart. |
| Compile-failure proof | Introduce a temporary syntax error | Build reports source diagnostic; old generation and world continue. |
| Recovery proof | Fix the syntax error and save | Newer valid generation activates in the existing process. |
| State-layout proof | Later, add one versioned NPC field | Migration/default is validated; existing NPC continuity and rollback are proven. |
| Rollback proof | Activate a valid but deliberately worse behavior generation | Prior code is restored; runtime-owned NPC/world state is retained. |

Do not combine the first code reload with a schema change, world-loader move, networking protocol change, or renderer-device rewrite. Prove each boundary separately so a failure has one likely cause.

#### Step 7: expand from one behavior to all eligible game sources

After the NPC proof passes, update the canonical ownership manifest so ordinary production implementation files under `src/**/*.cpp` default to `game.dll`. For each batch:

1. Select a dependency-connected group from the inventory and name its current owner, planned owner, persistent state, jobs/callbacks, and external resources.
2. Move ownership in the build manifest and remove each moved source from its old EXE/subsystem target in the same change. Enforce exactly one link owner.
3. Resolve EXE globals and static data by moving durable state into runtime records, converting constants to immutable module data, or making temporary state generation-scoped and resettable.
4. Keep third-party implementations, build tools, tests, bootstrap/process/device/transport mechanisms, and runtime/ABI mechanisms in their correct targets; list each exception and reason. Do not report these as ordinary game sources.
5. Build/activate through the lane workflow and exercise the real feature. Record source counts, compile/link/activation times, state continuity, and journal evidence for each batch.
6. Move existing subsystem-module sources into the single `game.dll` target only after their behavior and dependencies are accounted for. Avoid duplicate copies in both subsystem modules and `game.dll`.

Start with low-fanout behavior leaves. Defer central orchestration, shared headers, world loading, renderer/audio/physics resources, networking transport, and public packet/ABI code until the relevant state and lifecycle owners are mapped. The target remains one large `game.dll` until measured link time, ownership isolation, or rollback requirements justify splitting it.

## 4. Migration phases

### Phase 0 — Inventory and baseline

- Inventory every active `src/**/*.cpp`, header dependency, current build target, global/static state owner, thread/job callback, and network message boundary.
- Record current EXE, narrow DLL, and subsystem-module ownership. Separate source facts from proposed future ownership.
- Measure current build stages and representative save-to-evidence cycles: save detection, affected translation units, compile, link, validation, activation, and first decisive journal event.
- Capture an FPS/server-tick baseline on named hardware and workload. Do not assume an NPC triangle-scan problem until the journal or profiler identifies it.

**Pilot-start gate:** enough evidence for the selected NPC path, not a full understanding of the whole repo: exact source/call path; fields read/written by the chosen decision; current owner of those fields; callbacks/jobs that can execute that path; existing loader/target; one repeatable map/workload; and an authorized route for the boundary build/runtime run. This lets Phase 1 begin while the wider inventory continues.

**Full Phase 0 exit evidence:** complete, overlap-free inventory; exact representative build/save-to-activation timings; named-hardware runtime baseline; and known state/loader boundaries for the sources planned for migration. Do not mark all of Phase 0 complete based only on the pilot-start gate.

### Phase 1 — Runtime/game boundary and NPC vertical proof

- Install the resident runtime state store, versioned component records, stable game ABI, generation validation, safe call/job quiescence, and rollback path.
- Move one complete NPC decision/update path into the pilot `game.dll` target. Keep the same NPC identities, positions, health, world, and connected session in runtime-owned state.
- Save a change to NPC movement code. Build and activate it without relinking the EXE or restarting the process. Confirm that the NPC behavior changes and the pre-existing NPC/world state remains.
- Record the candidate generation and the relevant before/after NPC, movement, frame-time, and server-tick evidence in the exact running process journal.

**Exit evidence:** real same-process activation, state continuity, compile error keeps the old version active, and rollback restores the prior behavior without losing world state.

### Phase 2 — Default whole-game DLL ownership and scale the one-command workflow

- Change build ownership so ordinary game implementation `.cpp` files are routed to `game.dll` by default; retain only the minimal bootstrap and generic resident runtime implementation in their stable targets.
- Make the source inventory/catalog, build scripts, dependency scanner, and watcher agree on one owner per file. Preserve existing module code during transition; merge it into the pilot target or keep it explicitly catalogued until migration is verified.
- Generalize the pilot watcher/coordinator to every game-owned source. Use background incremental compilation and immutable candidate generations; do not make the game thread compile synchronously.
- Measure single-`.cpp`, shared-header, and large-link edits. Keep one `game.dll` while timings meet agreed targets; split only when measurements show the large link is the bottleneck.

**Exit evidence:** every active implementation source is classified, ordinary saved game-source edits build only the affected game objects plus a new DLL link, and no EXE relink or restart occurs.

### Phase 3 — Use the live loop to restore performance

- Use owner-level performance events to identify the actual expensive NPC/system path. Distinguish render frame cost, GPU/device wait, fixed-tick cost, allocations, and background work.
- Change the identified C++ implementation in `game.dll`; save, compile, activate, exercise the real workload, and compare the same measurements. Repeat without restarting the process.
- Treat “NPC scans every map triangle every tick” as a hypothesis until code-path and runtime evidence confirm it. If confirmed, compare a spatially bounded query against the existing path while preserving collision correctness and the cached broadphase rule.

**FPS target:** at least 99.99% of rendered frames meet the 240 FPS budget (frame time at or below 4.17 ms), with worst frame and p50/p95/p99/p99.99 reported separately. State workload, duration, hardware, and frame count. Do not substitute average FPS for the frame-budget result.

**Server target:** maintain the authoritative fixed 60 Hz tick and report missed/over-budget ticks separately. A hot reload must not change tick semantics. At least 99.99% of legitimate, server-consistent client hit claims should be accepted over the full measured test session; report the valid-claim count and false-rejection count. Invalid claims must still be rejected.

### Phase 4 — Multiplayer, world editing, and more subsystems

- Prove one host/server plus multiple local clients. Preserve connections and game state through a C++ edit; require compatible code/schema hashes and coordinated activation. Test mismatched or disconnected clients and rollback.
- Add and migrate component fields while existing entities remain alive. Prove defaults, migration, rejection of incompatible schemas, and rollback.
- Expand live implementation coverage to movement/collision, weapons, vehicles, rendering, GUI, animation, audio, and creation tools. Keep each subsystem's fixed-tick, authority, and resource-lifetime rules.
- Treat maps, music, animation, video, and documents as synchronized data operations; they should not need C++ compilation for ordinary content edits.
- For renderer backend changes, prove same-process device recreation and resource recovery on each supported platform/backend pair. Do not claim unsupported drivers/backends or simultaneous mixed backends without separate evidence.

### Phase 5 — Collaborative authoring and durable operations

- Use one canonical project/build coordinator with isolated revisions, conflict detection, and per-user undo history; do not destroy or overwrite existing agent-lane work before verifying and preserving it.
- Synchronize trusted native-code proposals only after host approval and compatibility validation. Synchronize creative document/map/media edits as data operations.
- Record who proposed, approved, built, activated, and rolled back each code generation. Public clients never run unapproved native code.

## 5. Latency goals and reporting

The ideal is about one second for a tiny warm edit. The practical goal is to make a routine single-`.cpp` edit take seconds and avoid routine waits over one minute. These are targets to measure, not guarantees for every source change. A widely included header or a large DLL link may take longer and must be reported separately.

For every trial, report:

- changed files and dependency fan-out;
- warm/cold compiler-cache state and toolchain;
- compile time, DLL link time, validation/migration time, and save-to-activation time;
- active/previous generation and process identity;
- runtime state continuity and behavior evidence;
- the exact `events.jsonl` path and owner-level event;
- whether any stable runtime/ABI boundary remained.

Do not use `Start-Sleep` as a build-completion detector. The coordinator should notify on completion and expose a machine-readable state/event stream.

## 6. Current source status (2026-11-11)

Source inspection on 2026-11-11 found 594 `src/**/*.cpp` files. The existing `devscripts/module_ownership.py` audit assigns 12 sources to subsystem modules and 582 to the EXE. `build_game_dll.py` builds only `src/effects/effect-part.cpp` and `src/hot-reload/quick-hit-debug-visual.cpp`; `HotReloadSystem` watches that focused source list and invokes its build synchronously. The current `GameMemory`/`GameAPI` supports a small effect/debug feature, not a complete game/runtime boundary. No whole-game `game.dll` or persistent-state `runtime.dll` implementation was verified.

The repository also has a subsystem `ModuleHost` and catalog. These are existing migration assets, not evidence that all `.cpp` files are hot. During the transition, keep current module builds working until their sources and behavior have been proven in the `game.dll` path. Do not bulk-reassign files in the catalog and claim they are reloadable without a successful runtime proof.

## 7. Acceptance gates

Do not claim the target is implemented until the relevant running-game evidence exists:

1. Saving NPC movement C++ produces a candidate game generation without an EXE relink or process restart.
2. The same NPC IDs, positions, health, world, client/server session, and editor documents survive activation.
3. The changed NPC behavior is observed in play and in the correct process's `events.jsonl`; a generation-loaded event alone is insufficient.
4. A compile error, ABI mismatch, failed schema migration, and runtime regression each leave the last-known-good generation and state usable; rollback is observed in the same process.
5. The source inventory assigns every active implementation file exactly once to bootstrap/runtime/game ownership; a saved ordinary game `.cpp` is detected automatically.
6. Latency evidence includes build stages and generation identity; no fixed sleep is needed.
7. The 240 FPS frame-budget and 60 Hz server-tick goals are measured under stated workload and hardware, separately from code-reload success.
8. Multiplayer activation is coordinated across trusted connected clients; incompatible clients are held on the prior generation or safely disconnected, never silently divergent.
9. GUI/rendering, creative-data collaboration, and supported backend recreation each receive their own real runtime proof before being marked complete.

A build proves compilation/linking. An activation event proves code was selected. Only exercising the behavior and observing the resulting state proves runtime behavior. Visual/gameplay acceptance remains a separate human judgment.

## 8. Historical and implementation-specific notes

The remaining sections document the existing subsystem-module implementation, past module proofs, inventories, and JSON hot reload. They remain useful evidence. Where they describe subsystem DLLs as the final target, an EXE-owned kernel, or map-editor-first migration order, the architecture and phases above supersede that older target. Do not treat an old module activation or dated inventory as proof of the new `runtime.dll` + whole-game `game.dll` design.
# Existing subsystem-module implementation and dated evidence

> **Status:** The material below records the earlier subsystem-module design and implementation evidence. It is retained so existing code ownership and successful module proofs are not lost. The current desired architecture, phase order, and acceptance rules are in sections 1–7 above and take precedence wherever the text below describes modules or an EXE-owned kernel as the final target.

## Actor rendering migration (2026-11-08)

The `actor-rendering` catalog entry activates on the main thread before a
render frame. Its plain-data events select body-part pose transforms,
calculate local vertex bounds, decide whether an outline with actor-relative
bounds or a camera-inside case may draw, and plan body-part visibility and
overlay layer order. The host passes borrowed position data only for the
duration of a synchronous bounds call, keeps the resulting bounds
in host-owned meshes, and owns GL state, shader programs, buffers, and draw
submission. `shaders/basic.vert` remains an independently reloaded asset.

This is an actor-rendering behavior boundary. The module plans part visibility,
combined-mesh eligibility, outline rejection, and overlay ordering; the host
executes the plan and retains material/GL state and actual part submission.
The long-term target is to move further presentation policy behind the same
stable draw-plan interface, leaving as little policy in the host as practical. A source
save is hot only for behavior already behind this boundary; changes to the
host adapter or public ABI still require an EXE build.

## Legacy module target: goal and plain-language contract

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

## Cold-build policy

Cold EXE builds are a boundary, not the normal workflow. They are reserved for
process lifetime, device access, transport, packet schema, document storage,
loaders, and public module ABI/capability changes. Gameplay and tool behavior
belongs in modules.

A module source, module-private header, or data/config edit must not relink the
EXE. The working goal is "effectively never" for behavior work, not a literal
promise of zero for clean machine builds. Warm boundary builds use ccache and
object caching and target seconds; clean release/install builds remain slower.

The long-term path to fewer boundaries is a generic, versioned, data-driven
event/behavior IR: new gameplay mechanisms should use existing host
capabilities and event/data schemas rather than add a new EXE call site. EXE
changes then remain limited to process, authority, device, and transport
mechanisms. See
[`module-migration-plan.md`](../../architecture/live-development/module-migration-plan.md)
and
[`self-contained-workspace.md`](../../architecture/live-development/self-contained-workspace.md).

### Cold boundaries are the current phase, not permanent

The cold list above is the boundary for the current phase, not a permanent
classification. Physics/collision query infrastructure, loaders, document
storage, transport, device access, packet schema, and the public ABI are cold
now because they need stable process-wide ownership, lifetime control, and
authoritative safety validation. The long-term direction is that even those
mechanisms become hot behind versioned capabilities or the generic
event/behavior IR. Physics and collision policy in particular are intended
eventual hot candidates.

That migration is explicitly out of scope for current work. Do not delay
behavior extraction or hold back a live fix because a mechanism is "cold
forever." Whenever such a mechanism does move later, it obeys the same rule as
everything else: install the generic host seam once with one intentional
boundary build, then keep later changes module-only.

## Hot subsystem target: networking and NPC (2026-11-09)

The target is not "some policy callbacks are hot." Entire behavior subsystems
are hot. The host exposes bounded inputs and capabilities; the module owns the
subsystem state machine and returns decisions/actions.

### Networking

`network-policy` owns map-sync, reconnect, session-health, snapshot, spawn
authority, and combat-validation decisions. The host owns sockets, ICE/UDP
transport, packet wire structs, encode/decode/framing, map loading, and fixed
tick scheduling. A reconnect accept carries the authoritative map id (with a
preserve-previous-map fallback) so map-sync can re-arm without an EXE restart.
New networking behavior must use the existing event/action schemas; a new packet
schema or transport mechanism is a deliberate cold boundary.

### NPC perception and behavior

`npc-behavior` owns goal/action/targeting/combat/team policy. `npc-perception`
owns FOV/range/LOS gating, memory decay, belief, and prediction. The host owns
actor state, the immutable world/navmesh query mechanism, collision and physics
application, and the fixed tick. The `CAPABILITY_NPC_WORLD` capability exposes
only bounded line-of-sight/ray queries; the module decides when to ask and how
to gate the result. Editing perception, sensing cadence policy, targeting, or
navigation choice must not relink the EXE.

The ideal cold list is therefore only process/device/transport/packet-schema/
document/loader/public-ABI changes. Everything else is a hot subsystem module
with versioned plain-data inputs, module-owned state, and host validation.

## Legacy module target: module and kernel responsibilities

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
`devscripts/module_ownership.py` is the executable ownership check: it expands
the catalog for the EXE source inventory, fails on overlap or a missing catalog
entry, and keeps the dev-loop cold watcher from treating a module source as a
host input.

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

Long-term modularity direction: make the renderer's remaining upload/resource
calls, world-loader upload path, and editor host glue as thin and as
module-owned as practical. This is a direction, not a current requirement. The
implemented slice deliberately keeps GPU/device/resource lifetime, CPU world
data, document persistence, and editor state authority in the host so module
source saves stay fast and safe. Moving more of that surface behind module
capabilities should be done incrementally with parity and runtime evidence, not
as an unverified rewrite.

## Legacy module target: save-to-live build and activation contract

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

The running host limits module compilation to two background workers. A module
with an active worker keeps newer saves pending until that worker exits, then
builds the newest saved generation. The worker caches objects by source,
compiler inputs, and discovered header dependencies, recompiles affected
translation units, and links a new generation. An EXE build remains serialized
per checkout and is needed when a host source or public ABI changes.

Source ownership is exact and catalog-driven. `config/modules/modules.json` is
the single ownership map; `devscripts/module_ownership.py` expands it for
`build.py`, the dev-loop watcher, and the module worker. Every module source is
excluded from the EXE inventory, the audit fails on dual or missing ownership,
and only an explicit `shared_with_host` migration entry may temporarily keep a
module source in the EXE. Module compilation uses a per-module `optimize`
value (default `-O2`) so hot policy is not compiled slower than the host.

Agents never invoke a builder. The lane dev-loop is the only EXE builder and
the lane game's `ModuleHost` is the only module builder. Agents use isolated
mode 2 lanes and read status/journals while the dev-loop auto-detects saved
edits. A repository-local scheduler allows one EXE build and two module builds
across the checkout's lanes; all writable state stays under the repository's
gitignored .dev root. See
[`../dev-loop/agent-lanes-build-authority.md`](../dev-loop/agent-lanes-build-authority.md).

The live repair slice adds bounded collision and networking decisions without
moving host data ownership:

- `chunk-collision` events 404/405 decide actor-pair push/support response and
  the bounded final-pose containment limits. The host keeps the broadphase,
  capsule sweep, world triangles, `Player` mutation, and safety application.
- `network-policy` events 702-705 decide movement-correction class, hit-claim
  tolerances, local reconciliation logging, interpolation gap snapping, and
  snapshot queue pressure. The host keeps packets, rewind poses, prediction
  buffers, contexts, and authoritative state.
- Module activation now occurs outside an executing fixed tick, and module
  invocation shares a lock with activation/unload so a listen-server tick
  cannot race a live swap.
- State export/import is bounded, validated before activation, and journalled;
  a rejected migration keeps the previous generation active.
- `shared: true` catalog entries publish a module-set hash to
  `build/modules/agreement/<run_id>/<instance>.json`. A shared candidate waits
  until every fresh peer reports the same target hash, then activates at the
  next safe boundary. This is a same-checkout/dev-loop agreement mechanism; a
  remote wire protocol remains a separate later slice.

For the current performance and authority repair, classify candidate sources
before extraction: `src/npc/npc-*.cpp` and `src/npc/team-brain.cpp` contain
behavior candidates; `src/network/server-npcs.cpp` applies authoritative actor
actions. `src/physics/movement/actor-pair-collision.cpp`, the
`physics-collision*.cpp` family, and `src/network/server-physical-contact*.cpp`
are fixed-tick mechanisms; the existing `chunk-collision` module owns contact
policy. `src/combat/weapon-fire*.cpp`, `melee-client.cpp`, and
`projectile-simulation.cpp` contain weapon behavior candidates, while
`src/network/server-attack.cpp`, `server-melee-actions.cpp`,
`server-projectiles.cpp`, and `server-damage.cpp` own server validation and
application. `src/combat/death-system.cpp` and
`src/network/actor-lifecycle.cpp` own lifecycle mechanisms.
`src/mapeditor/map-editor*.cpp` contains editor behavior candidates;
`src/world/streamed-world-layer.cpp`, `src/procedural/infinite-world-runtime.cpp`,
and `src/world/world-gltf-loader.cpp` own streaming/geometry mechanisms around
the existing `infinite-worldgen` and `map-editor` policy modules.
`src/network/multiplayer-reconcile.cpp`, `multiplayer-interpolation.cpp`, and
`movement-validation.cpp` own correction and validation mechanisms. This is a
trace inventory, not an instruction to move every listed file into a DLL.

## Existing module-generation evidence

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

## Existing map-editor module proof

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

## Historical subsystem-module inventory

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
| Actor rendering | `actor-rendering` currently owns pose interpolation, local avatar mesh bounds, part visibility, combined draw eligibility, outline rejection, and overlay order. Actor/world bounds assembly, team/config selection, material policy, draw submission, and GPU resources remain in the EXE. | Host player/NPC state, avatar mesh data, camera, GL/device lifetime, and synchronous plain-data event calls. Shader source has its own reload path. | **6a:** verify module generation changes in one running client, then use actor ID, avatar, bounds, pose step, camera distance, and outline decision in that client's journal to investigate the giant silhouette. Move further actor policy only with a stable host input/result contract. |
| Other UI, animation, and presentation | Native update/draw code remains EXE code; selected layouts/configs/resources reload as data. | Host GPU/device/resource lifetimes, UI state, and render-boundary coordination. | **6b:** migrate one presentation or interaction owner after host resource lifetime is documented; verify the live generation and visible result. |
| Networking policy | Native prediction/reconciliation/relevance and transport code remain EXE code. | Host sockets, packet framing, connection lifecycle, server authority, and module-generation compatibility. | **7:** migrate policy only after two-process generation agreement is proven; verify server/client journals and a real connected session. |
| World generation, effects, and audio policy | Native behavior remains EXE code except the narrow effect/debug DLL pilot; asset/config data may have separate reload paths. | Host world/asset IO, effect pools, audio device, and resource lifetimes. | **8:** migrate bounded policies independently; exercise a real generated-world or audio/effect path and inspect owning-process evidence. |
| Process, platform, and runtime kernel | EXE-owned process, platform, scheduler, storage, loader, logger, renderer/device, transport, and physics mechanisms. | This is the stable service boundary used by modules. | **0 and continuous:** install and verify the module/state/diagnostic substrate first; keep mechanisms cold unless a separate design proves a safe replacement boundary. |

This is a migration map, not a claim that every row has an existing module.
Before promoting any row to implemented, trace the active source owner and
prove its real module build, load, behavior, and journal. A module is one
cohesive subsystem target, not one DLL per `.cpp` file.

## Historical module timing baseline

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

## Existing module-system acceptance gates

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
generation 4 in the same PID. Successful module and shader activations now show
a dark-green/green notification with the full edited paths; failures show a red
notification with the failing path and reason. The map editor presentation is
now primarily JSON-defined in `config/gui/map-editor.json`. The `map-editor`
module also now owns the drag-delta projection math and gizmo axis hit-distance
math; a live edit doubling the plane drag scale activated a new generation and
changed the reported probe delta in the same process. Selection command behavior
(`select`, `select_kind`, `select_route`, `selectall`) is now parsed by the
module and applied through a host selection service. Clipboard/undo/redo,
save/reload/autosave, focus, nudge, transform fields, bomb-site radius, and
rename commands are also parsed by the module and applied through host-owned
document services. A mode-6 cursor/box-select/click-priority fix currently lives
in host `src/mapeditor/map-editor.cpp`; it is the next candidate for live input
extraction. Full input handling, outliner/properties, remaining
commands/projects/overlays, and the full draw-path extraction remain future work
and must not be reported as complete.

### Infinite World hot boundary slice (2026-11-07)

The module ABI is now the generic `mimita_module::ModuleV1` lifecycle/event
interface. `ModuleHost` no longer switches on module names during validation or
activation; catalog entries declare `events`, `capabilities`, and an activation
boundary. The source catalog includes these modules:

- `map-editor` — selection, transform, command, add-chunk, chunk-part,
  connector, and connection-preview decisions through the editor capability;
- `world-rendering` — the canonical six-face box geometry and world-unit UV
  policy used by both authored blocks and streamed chunks;
- `infinite-worldgen` — connector compatibility, quarter-turn placement,
  deterministic neighbor choice, chunk bounds, and grid-cell placement;
- `chunk-collision` — collidable-part selection and world-space box triangles
  after chunk and part transforms, actor contact/surface policy, and bounded
  shared depenetration correction;
- `network-policy` — consolidated snapshot-budget, keepalive, and reconnect
  decisions for both client and server;
- `npc-behavior` — utility goal/action decisions, goal-score diagnostics,
  grenade throw policy, dash choices, and Payload cart-role destinations on the
  authoritative server. `npc-utility.cpp` is module-owned; the host adapter is
  `src/npc/npc-utility-bridge.cpp` and retains no scoring policy.
- `npc-perception` — FOV/range/LOS gating, memory decay, belief, and prediction.
  The host exposes `CAPABILITY_NPC_WORLD` for bounded line-of-sight/ray queries
  and keeps actor/world ownership; `src/npc/npc-perception.cpp` is the host
  adapter and retains the pre-module fallback only until parity is proven.

This inventory describes source ownership, not proof that every module has
activated in the running EXE. `npc-perception` owns FOV/range/LOS gating,
memory, belief, and prediction through the bounded `CAPABILITY_NPC_WORLD` query.
Candidate gathering, state-machine execution, weapon legality, and weapon firing
remain host code. Target priority/retention scoring, attack intent, and
dash/down-dash threshold policy are module-owned source slices; state-machine
transition extraction is deferred.

Actor-versus-static-world correction now has a bounded hot snapshot/result path
with host fallback. Dynamic-object, actor-pair, and weapon/projectile solvers
remain host code. Their planned hot migration requires bounded versioned input
and output, host-side result validation, and real gameplay evidence for each
owner.

Host capabilities cover editor operations, world-generation tables, mesh face
submission, and collision-proxy construction. The host keeps documents, unsaved
editor state, GPU/VBO lifetimes, static-union processing, and physical-entity
registration.

`WorldGenLibrary::pollReload` watches the active profile and its referenced
chunk files, validates a candidate, and commits only on success.
`MapConfigRegistry::pollReload` defers applying an external map edit while the
editor has unsaved changes. Connector-mode `stream_radius` is interpreted as
world-space meters; regular grid mode keeps cell counts. Connector streaming
uses a bounded distance frontier rather than grid offsets, so mixed-size and
rectangular chunks share the same placement path.

Writing `*-module.cpp` sources does not relink the EXE; module builds are
excluded from the executable source list. The module worker discovers
transitive headers with `-MMD`, writes `deps.json`, and prepends the compiler's
runtime directory to `PATH` so the Windows `cc1plus` helper can start. A failed
module build or validation keeps the previous generation active.

## 10. Collaborative live C++ proposals — future goal (not implemented)

This section records the desired multiplayer development direction only. It is
not implemented by the first module slice.

The target topology is one authoritative host process and several clients:

```text
client A (host + server) <-> client B <-> client C <-> client D
```

Any connected client may propose a native C++ module change, for example a
niche bug fix found in live multiplayer. A proposal is not an edit to a running
generation. It is a bounded, reviewable transaction:

1. Client creates a proposal with a stable proposal ID, module name, source
   snapshot or patch, target ABI, dependency set, state schema, and source hash.
2. Host distributes the proposal identity and a bounded summary to every client
   editor. Source and artifacts are content-addressed; the preview is not active
   code.
3. Each client accepts or rejects. The host/owner makes the final decision. The
   recommended default is host/owner approval after client votes, not automatic
   unanimous execution.
4. The host builds or verifies the candidate through the same snapshot-pinned
   module worker and validates ABI, dependency, state-schema, and capability
   compatibility.
5. Activation happens only at the owning subsystem boundary. Server-authoritative
   changes defer to a fixed-tick or session/round boundary. Clients never
   silently apply conflicting authoritative code.
6. Every process publishes its accepted module-set hash in `run.json` and
   `events.jsonl`. A mismatch blocks activation and is reported rather than
   hidden.
7. The host can roll back to the prior module set without restarting. A
   disconnected client resumes the last confirmed module set or forks.

Proposal events are bounded and transition-based:

```text
module.proposal.created
module.proposal.distributed
module.proposal.vote
module.proposal.accepted
module.proposal.rejected
module.compatibility.rejected
module.activation.pending
module.activation.activated
module.rollback
```

Security and trust rules are mandatory:

- A public or untrusted server must not accept arbitrary unapproved native code.
- Proposal mode is opt-in and requires explicit host/owner policy and client
  consent.
- Artifacts are identified by source hash, ABI, dependencies, and module-set
  hash; a `latest` mutable path is never sufficient identity.
- A client whose toolchain, platform, ABI, or dependency set cannot build the
  candidate rejects it instead of running a guessed replacement.
- Activation must not bypass the fixed 60 Hz authoritative boundary, damage
  validation, collision, or server authority.
- The host records who proposed, who voted, what was accepted, what activated,
  and why a candidate failed.

This is a specification goal for later work. It does not claim that clients may
currently submit executable code to one another.

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
