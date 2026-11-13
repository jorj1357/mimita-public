2026 11 12 1816

"yes do that next stuff much as u can  , as well as tell whats next, as well as explain how close we are to that goal or drem or reality like, npc behavior is off , ic an edit npc behavior cpp, save live, load live, ai sees events and changes again based on that, load live, npc behaivor is better . im snagging on corners, ask ai to fix, it fixes live, i real time stop snagging on corners. weapon recoil fields in json dont work, ai adds fields live, updates live, its working live as i i intended. visuals arent so nice looking, ask ai to add anti aliasing, it adds it liev, i see the pixelized stuff get smooth live. as well as 4d geometry live i can add, packts and networkign all networking related things are hot reloadable, changing rendering backend or adding nrew one live eventually is hot reloadable, cahngign operaitng systems can happen with the same game running e.g. put mimita on a usb stick, it is put into windows pc, linux, mac, android, ios, ps2/3/4/5, xbox, smart tv, etc, and u can play it live, and u can hot reload on any of those live, and play/meet with anuone else on any of those platforms live too , and do live collaboration , acorss not jsut the game but  planned future things like DAW, viedo editor, replay eidotr, live coding together, live note mamangement together, etc, and all stored data is distributed so no central server is needed so it can persist past me  so i dont ahve to maintain it and it just works and can be saved locally like it becomes a super powerful replacement for tools that are typicall closed source  and not easy to fix or maek ebtter, now u can maek them better live and use them to do things too live, etc   - add this stuff to hot reload spec this is what i want to  do " - jorj1357

deepseek v4.1 flash
"1. Failed compile fallback — make the coordinator leave the old generation active when the candidate build fails, with journal evidence.
2. Real live.py whole-game integration — drive build/game/ready.json from the whole-game build.
3. Add/remove/header edit proof through the coordinator.
4. Player-list authority — needs a bounded player-list query.
5. Map/team/spawn/gamemode slices.
6. Link families — GLAD, ModuleHost, MimitaNet, ICE, duel, globals, audio helpers.
7. CAP_WORLD/CAP_RENDER and packet-handler migration.
8. Static gate 235 → 0, then the single all-files cutover and campaign."

> **Status 2026-11-12:** item 1 (failed compile fallback) is proved: a forced
> broken probe compile exited 1, left `current.json`/`ready.json` unchanged, and
> the host stayed on the old generation; `admincmds` still worked. Items 2-8
> remain open; item 2 is partially wired because the host already polls
> `build/game/ready.json`. Link pass update: the whole-game candidate now links
> at generation 68 with **0 undefined / 0 unique** and 0 duplicate definitions
> (`build/game/generation-67/game-g67.dll`, export
> `GetMimitaGameModuleV1`). GLAD is 0 via a transitional diagnostic
> `src/glad.c` object; audio, globals, Renderer lifecycle, replay jobs, ICE,
> listen-server session, and authored-rebuild references are 0. CAP_RENDER must
> replace the transitional GLAD object before cutover.

> **Closeness to the dream (2026-11-13):** the **mechanism** is now proven on
> a small probe; the **coverage** is still early.
>
> - Local live C++: probe `game.dll` can be built, activated, rolled back, and
>   keep the same process, connection, player, and position/tick continuity.
>   Exact `admincmds` is the first authoritative hot command.
> - Production coverage: **0% of ordinary game code runs from `game.dll` in
>   production today**. All 600 game sources still run in the EXE. The whole-game
>   candidate compiles and links with **0 undefined / 0 unique** symbols. The
>   rebuilt host loaded and initialized generation 68 in a dedicated server
>   (PID 13028, `game.host.probe.started`, ABI 1). The host has a barrier-
>   guarded `driveTick`/`driveRender`/`driveFrame` path and `CAP_RENDER` has
>   host-executed buffer/texture/attribute/uniform/draw commands. The first
>   host-owned-frame client attempt crashed in the menu with an unhandled
>   exception (`UnhandledExceptionFilter` / `MessageBoxA` stack). A full
>   `main-init`/`main-systems` ownership move was attempted and reverted: it
>   reintroduced 33 bootstrap-global/resident dependencies and broke link 0.
>   `frameCapable` is held at 0; the driver is not used by the legacy loop and
>   the production cutover is not running. Static gate: **0 violations / 0
>   files**.
> - Networking: the opaque envelope and command response work; most packet
>   handlers are still resident.
> - Rendering: `CAP_RENDER` is a command-stream skeleton; a resident
>   `RenderDevice` lifecycle capability now owns the Renderer instance, but GLAD
>   and the draw path are not cut over.
> - World/state: player/NPC command snapshots exist; full world, projectiles,
>   gamemode, physics, and migration do not.
> - New backends, other operating systems/platforms, distributed persistence,
>   live collaboration, DAW/video/editor/notes: **not started**.
>
> The next gates are whole-game link, compile-failure fallback, coordinator-driven
> add/remove/header edits, full packet-handler migration, `CAP_WORLD`/`CAP_RENDER`,
> static gate 0, and the single all-files cutover. The dream is real, but the
> current proof is a narrow vertical slice, not the full game yet.

2026 11 12 1008 jorj

the end goal i think here is

edit any cpp file => ctrl + s => it changes live

because i want to get rid of as much waiting as possible, and as much closing/opening the exe as possible
becasue this is emergent from the deeper desire to  shrink teh time betewen idea and reality as much as possible thta i want to do with this code wise

so the way i curently understand it to be done, is,
all cpp files no longer build into .exe, the build into a .dll

game.dll

the game.dll is reloaded live, adn the .exe is
not small in size like megabtes, but
like, in what it does
small in teh amount of times it needs to be closed and opened again
the exe should be like the doorway into a garage
it should be open like all the time, accepts 99.99% of things u put in the garage

becasuse antother big thing is
i want to be abel to add totally new arbitrarry things like

new video editor
new rendering backend, e.g. vulkan metal directx instead of opengl
new dimeional geometry, 4 dimensions. 3.5 dimesions, fractal rooms
rooms bigger on the inside than the outside

adding portals live would be amazing , no exe restart needed
adding competely new things like ray tracing, realisitc graphics
new primitives, adding cones, elongated spheres, arbitrary geometry e.g. points => lines => faces, things all made of triangles ideally

anything like that, that u might want to do, whether its game engine related collisions etc, networking, rendering, supporting a new OS/platform, porting to browser, making mobile port, console port, chromebook port from 2ombie suggested, etc
all should be doable live without the exe having to close

the exe is simply a mirror of the code , and a mirror cahnges live,
mirror does not need to break and be put back together to update the mirrors visuals

> **Current target and literal limits (2026-11-12):**
>
> - "All `.cpp` hot" means all **ordinary game implementation** sources. The resident core (loader, barrier, device, transport mechanism, document storage, capability table) stays loaded and is deliberately cold.
> - **Hot now:** gameplay, networking packet schema/IDs/handlers/error paths, render primitives and draw policy, world-query composition, portals, 4D/fractal geometry, GUI, audio policy, tools.
> - **Cold now:** process lifetime, loader/barrier, OS/device/driver interface, transport mechanism (sockets/ICE/framing/encryption/reliability), document storage, public capability table.
> - **Backend/device changes may require device recreation** while the device is active. The long-term goal is a resident backend interface with hot-reloadable backend modules; hot backend is a later phase, not a current claim.
> - The default is hot. Every cold boundary is a debt to retire, not a normal workflow.

> **Literal contract (2026-11-12):** any ordinary game implementation `.cpp`
> can be added, edited, or removed; a save produces a candidate `game.dll`; the
> host validates and activates it in the same process. No EXE relink and no
> process restart are required for ordinary game code. This includes physics,
> collision, NPC behavior, rendering commands/primitives, packet schemas and
> handlers, gameplay authority, GUI, and tools. A new backend, OS, or platform is
> a resident backend interface plus a loadable backend module; adding the module
> must not rebuild the EXE. Changing the active device may require device
> recreation, which is a runtime constraint, not an EXE rebuild.

2026 11 11 1939 jorj

Next: fill in the lifetime review for every eligible source and linked game dependency, using the dependency map to organize the review. Then finish the resident-runtime/game boundary and validate the loader and NPC continuity as proofs. After those pass, update the build ownership manifest once to move the eligible game sources into game.dll. I have not changed runtime/build ownership or run a build; the repo’s instructions assign builds to the root dev loop.
dot his
2026 11 11 1507 jorj

"Finish the inventory. Map each .cpp to its dependencies and identify which code owns persistent state, active callbacks, and background jobs. python devscripts/module_ownership.py already checks current source ownership; extend that inventory for the planned runtime and game targets.

Install the runtime/game boundary. Add a resident runtime.dll for persistent state and coordination, and a whole-game lifecycle API for game.dll. The EXE becomes a small launcher. The current EXE does not have this boundary, so installing it requires one initial EXE build. After that, ordinary game-code edits should not relink the EXE.

Prove one NPC path. Keep the same NPCs and world alive, edit NPC C++, build a candidate game.dll, and activate it in the same process. Prove state continuity, compile-failure fallback, rollback, and journal evidence.

Expand the default source ownership. Once that proof works, route ordinary implementation .cpp files to one game.dll. Split the DLL only if measured link times make that useful.

Add the one-command Python loop. A command such as python devscripts/live.py should watch saves, snapshot inputs, compile affected files in the background, and notify the runtime when a candidate is ready. It can automate the workflow; it cannot create the missing runtime boundary by itself."

> **2026-11-12 architecture decision:** the resident host is the canonical EXE, not a separate `runtime.dll`. See sections 2.1, 2.2, and 2.2.1. The `runtime.dll` wording in the dated notes above is superseded by that decision; extracting `runtime.dll` later remains an optional, ABI-preserving move.
>
> **2026-11-12 execution decision:** every eligible ordinary game source becomes hot in one reviewed cutover, not through a long-lived per-file migration. Stages 0-4 prepare that cutover with current ownership unchanged. A temporary validation-only probe may prove the loader/barrier mechanism and is reverted before the cutover; it is not a migration destination.

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

### 2.1 Resident host EXE

The canonical EXE is the resident host. It parses launch mode, owns process lifetime, keeps the persistent world/session/device/network state, holds the versioned C function table, and loads and swaps `game.dll` generations. It does not contain ordinary replaceable game behavior. The same process and the same resident records remain open while game-code generations change.

This replaces the earlier proposal to move the same responsibilities into a separate `runtime.dll`. The resident host may be extracted into `runtime.dll` later behind the same ABI if measurements justify it; that extraction is mechanical, not a prerequisite for hot reload.

### 2.2 Resident state store and services

The EXE host owns persistent runtime data and generic services, including entity/component records, player and NPC state, world/session state, connected-peer state, active-generation agreement, and safe activation coordination.

The resident data store is schema-aware and generic. It does not require one fixed C++ struct to gain a new member every time game code adds a field. Resident records have entity/type identity and schema version; new game generations provide migration/default logic for records they own. Migrations are validated and committed atomically with code activation. A failed migration leaves the old generation and old state active. The hot per-tick path must use batched or direct stable-handle access; it must not perform reflective field-name lookups for every NPC field on every tick. Schema conversion happens at generation activation, not continuously during play.

Keep the host's own editable behavior small. Changeable NPC, networking, rendering, physics, gameplay, GUI, and tool logic belongs in `game.dll`, even when it reads or changes resident-owned data. The host API should be broad and versioned so ordinary behavior and component-field changes do not require a host rebuild. Only a genuinely missing stable mechanism or incompatible host ABI is a host boundary change; this is minimized, not claimed impossible forever.

### 2.2.1 Why the EXE host instead of a separate `runtime.dll`

The hot-reload contract needs three things to stay resident: the loader, the persistent state store, and the versioned C ABI table. It does not require them to live in a separate DLL file. Keeping them in the EXE is the smaller and faster design:

- **Fewer moving parts.** One process image hosts the loader and state; there is no second artifact to build, stage, version, validate, or load.
- **One boundary instead of two.** Game code crosses one versioned table. A separate `runtime.dll` adds an EXE-to-runtime hop and a second ABI that must be versioned and proven.
- **No extra cold dependency before the first proof.** The initial EXE build installs the boundary either way. Adding `runtime.dll` also requires building and wiring that DLL before the first proof can run.
- **Same resident-host cost.** The resident host is not hot in either design: changing it, or the ABI, is a deliberate cold build. `runtime.dll` does not make resident behavior hot.
- **Marginal long-term gain.** The only plausible benefit is a cheaper rebuild of a host that is still under heavy development. Once every eligible game source lives in `game.dll`, the EXE host is already small, so that advantage mostly disappears.

The efficiency cost of a separate `runtime.dll` is therefore real and immediate (an extra artifact, loader hop, ABI, and validation path) while its benefit is delayed and likely small. Extract it later only if measured EXE relink time, artifact size, or ownership isolation shows a concrete gain, and do the extraction behind an unchanged ABI so it stays mechanical.

For the rest of this document, "runtime" means the resident host and its records (currently the EXE) unless a separate `runtime.dll` is explicitly named.

### 2.3 Replaceable `game.dll`

Initially, ordinary game implementation `.cpp` files are assigned to one `game.dll` by default. The DLL receives a versioned resident function table and stable handles; it owns behavior, not persistent world/session data.

Function signatures and C++ types inside `game.dll` may change freely when the affected code is rebuilt together. Across the resident-host/game boundary, use a versioned ABI with size/version fields, plain data, handles, and explicit ownership. Do not pass STL containers, exceptions, allocator-owned objects, or pointers to short-lived game-DLL memory across that boundary.

Adding or changing a field such as position, velocity, or health is a game-owned schema change plus migration. It should not alter a fixed resident C++ struct. The resident host preserves the record; the candidate game generation interprets or migrates it.

### 2.3.1 Bootstrap/device versus hot Engine ownership (2026-11-13)

The owner decision is explicit: **the replaceable `game.dll` owns the Engine
controller and ordinary game bootstrap; the resident host owns process
bootstrap, the window/GL device, the Renderer instance, OS services, and the
loader/barrier.**

- **Resident host:** process entry, DLL loader, generation barrier, window and
  GL context, `Renderer` device, OS event pump, sockets/ICE, audio device,
  workers, logging, and resident state.
- **Hot game DLL:** the single `Engine` object and `engine.cpp`, `gameInit`,
  `gameInitSubsystems`, input/UI policy and callback handlers, world/gameplay
  logic, frame policy, and render submission.
- The game Engine **borrows** the resident `Renderer*`/window context. It must
  not construct, destroy, or replace the device.
- The host must not construct an `Engine` or call `gameInit` when DLL frame
  ownership is active. Two Engines or two device owners is a defect.
- The host owns the actual GLFW/OS callback entry points. It forwards bounded
  input events to the active generation through generation-stamped callbacks;
  the DLL must not leave raw callback function pointers behind after unload.
- The host owns the frame clock and safe-point call. It calls the generation's
  `tick` and `render`; the generation owns simulation stepping and render
  policy. The host checks `should_close` through the resident device.
- The Engine object is generation-scoped: a swap destroys the old Engine and
  creates a new one against the same resident device. State that must survive is
  resident or explicitly migrated; Engine-local state that is safe to reset is
  reset and documented.
- `GameModuleV1` exposes the startup/bootstrap handshake and a frame-running
  check. A candidate without a usable bootstrap or frame entry is not frame
  capable and must not take ownership of the legacy loop.

The `frameCapable` flag is true only after the DLL bootstrap can run without a
host-constructed Engine and the first menu/gameplay frame proof passes. Until
then the legacy host path remains authoritative.

### 2.4 Multiplayer and graphics

The resident host preserves connection/session state and coordinates candidate-generation identity, compatibility, readiness, and activation timing. Changeable multiplayer rules live in `game.dll`. Use a generic versioned message/event envelope where possible so new behavior does not require a new host wire mechanism. A protocol feature that needs a capability the host does not provide is a deliberate boundary change.

Native-code sharing is for explicitly trusted development sessions. The host approves the candidate and all participating processes report compatible code/schema generations before activation. Public servers do not execute arbitrary client-supplied native code.

A live renderer/backend change may recreate the graphics device and re-upload resources while keeping the process and runtime state alive. It is limited to backends supported by the current platform, driver, and build. A backend unavailable on the current OS or hardware cannot be enabled by hot reload alone.

## 2.5 The plan in plain words (2026-11-12)

This section explains the same plan without jargon. It is the shared picture; the numbered stages below are the checklist.

### The toy-robot picture

- The **EXE** is the robot's body and memory. It stays switched on. It remembers where the NPCs and players are and who is connected.
- **`game.dll`** is a **dance card** that tells the robot how to dance. Ordinary game `.cpp` files are the steps on that card.
- Live editing means **swapping the dance card while the robot keeps dancing**. The body and memories stay.
- The **loader** is the hand that swaps the card.
- The **barrier** says: freeze for one split second, make sure no hand is still moving, then swap. If the new card is broken, put the old card back.
- The **manifest** is the list of which page belongs to which book.
- The **builder** is a helper that photocopies only the changed pages and prints a new card in the background.

The robot is not allowed to throw away its memory card (the EXE host) or its loader hand. Only the dance card changes.

### What already exists

- `config/source-owners.json` says who owns every `.cpp`: dance card (`game`), body (`resident`), process start (`bootstrap`), existing small modules, tools, tests, vendor.
- `devscripts/source_manifest.py` makes the list, hashes one revision, and notices added, removed, renamed, or header-changed files.
- `src/host/abi/game-host-abi.h` is the plug shape between the body and the dance card: version, size, plain data, stable handles, and a `quiesce`/`shutdown` result.
- `src/host/host-loader.h` is the hand that loads and validates a card.
- `src/host/host-barrier.h` is the freeze rule.
- `src/host/host-runtime.h/.cpp` plugs those together in the EXE. It is idle unless `MIMITA_GAME_PROBE=1`.
- `devscripts/game_build_worker.py` prints a candidate card in the background.
- `devscripts/live.py` watches files and asks the builder for a new card.

### Step A: prove one real NPC decision (the validation probe)

The chosen decision is "which enemy should this NPC target?" in `simulateSharedNpcs` (`src/network/server-npcs.cpp`). It is small: a list of candidate actors in, one target ID out.

1. The host gathers up to 64 candidates (ID, team, human/NPC, dead, relative position) and the current targeting policy.
2. It calls the active `game.dll` through the barrier: event `720`, `NpcTargetDecisionV1`.
3. The DLL runs the same pure policy (`src/npc/npc-targeting.cpp`) and writes one target ID.
4. The host checks the answer is a real, live candidate.
5. **Parity-only first:** the host logs `npc.target.probe` with `inline_target`, `probe_target`, `match`, `candidates`, and `generation`. The inline result stays authoritative. Only after parity is proven does a later change let the DLL result apply.
6. The probe source set is explicit: `config/probe/npc-target.json`. It is reverted before the all-files cutover.

Owner-run commands:

```powershell
python devscripts/source_manifest.py --write
python devscripts/game_build_worker.py --sources-file config/probe/npc-target.json `
  --out build/probe/npc-target
$env:MIMITA_GAME_PROBE = "1"
$env:MIMITA_GAME_DLL = (Get-Content build/probe/npc-target/current.json | ConvertFrom-Json).dll
```

### Step B: prove the whole path

In one running process, with the exact journal:

- load a valid card and see `game.host.probe.started`;
- edit `src/npc/npc-targeting.cpp`, rebuild, swap, and see a new generation handling `npc.target.probe` with the same NPC IDs and world;
- add a source and see it in the candidate set; remove it and see it leave;
- change a header and see only dependents rebuild;
- break the compile and see the old generation keep running;
- fix it and see recovery;
- activate a worse-but-valid card and roll back.

### Step C: all files at once

1. **Audit** every source: who owns its state, which callbacks/jobs can re-enter it, which resources it holds, and what crosses the boundary.
2. **Move durable state** into resident records. Behavior moves; memory stays.
3. **Wrap every host→game call** with the barrier. Replace detached workers with a resident executor or generation lease.
4. **Build the whole dance card** from all `game` sources in the background. Calls inside the card are ordinary C++ calls; only the plug (host services) changes.
5. **One cutover**: add every `game` source to `game.dll`, remove it from the EXE/module/CMake targets in the same change. Keep body/tool/test/vendor pages explicit.
6. **Measure** single-file, header, and whole-card build times. Keep one card unless measurements justify splitting.

### What only the root dev-loop owner or a human can do

- Run the one intentional cold boundary build that compiles the new host headers.
- Launch the server/client, exercise the NPC probe, and read the exact `events.jsonl`.
- Confirm the same PID, state continuity, compile-failure fallback, recovery, and rollback.
- Approve the final owner cutover and the human-visible gameplay result.

An agent may write and inspect code, but may not claim build or runtime proof.

### 2.6 Capability families and the complexity budget

There is one capability lookup: `get_capability(id, version)` returns a versioned plain-data struct. There are a small number of broad families, not one per file and not one giant interface.

Current families:

| Family | Responsibility | Folds in |
|---|---|---|
| `CAP_LOG` | structured event + text + counters | `StructuredLogger`, `DebugLog`, `Perf` |
| `CAP_TERMINAL` | emit + terminal state | `Terminal`, `TerminalState` |
| `CAP_GUI` | element/text read/write | `GuiLayoutManager`, tooltips, UI config |
| `CAP_DEBUG_VIS` | bounded draw queue | `DebugVis`, debug overlays |
| `CAP_ENTITY` | handle read/write snapshots | `PhysicalEntitySystem`, player/NPC records |
| `CAP_PHYSICS` | constraint/vehicle queries | `ConstraintSystem`, `VehicleSystem` |
| `CAP_WORLD` | broadphase + read-only spans + ray/query | world queries, audio occlusion |
| `CAP_RENDER` | draw-command stream + shader/resource handles | render primitives |
| `CAP_TRANSPORT` | opaque `{channel, typeId, bytes}` + session | `gpMpContext`, packets |
| `CAP_JOBS` / `CAP_CALLBACKS` / `CAP_RESOURCES` | lifetime mechanisms | barrier-owned work and resources |

The small one-off families from Pass C fold rather than adding new families: `CAP_PERSISTENCE` becomes a resident bridge or `CAP_REGISTRY`; `CAP_MODULES` becomes a resident bridge (the existing subsystem host); `CAP_AUTH` and `CAP_GAMEMODE` become `CAP_REGISTRY`/`CAP_CONFIG` reads.

Rules:

- No one-use capabilities. If a capability would retire only one or two files and is not a mandatory lifetime mechanism, fold it into a family or keep the file resident with a thin bridge.
- Every capability has one owner, one spec section, and a bounded plain-data op list.
- **If the family count keeps growing, stop and consolidate.** A growing capability table is the same failure mode as the discarded branch: more layers, less reasoning. Consolidation is required before adding another family.
- The manifest assigns sources. It cannot add a new host mechanism; that is a deliberate capability install, not a data change.

#### Family-level migration, not per-file

The remaining work is organized by **capability family**, not by file. One bridge
can retire every source in a family:

| Family | Remaining unique symbols (2026-11-13) |
|---|---:|
| process globals (`gpWorld`, …) | 0 |
| `MimitaNet` (remaining) | 0 |
| ICE (`IceAgent`) | 0 |
| Renderer | 0 |
| audio helpers | 0 |
| other | 0 |
| replay save jobs | 0 |
| ModuleHost / duel / persistence / JSON jobs / LogManager / self-tests / crash / crosshair / direct host / effects / GLAD / HotReload | 0 |

This is why the work is "little steps": each step is a family, not a file. A full
family bridge is preferred; a partial bridge is allowed when a family is large or
risky, but the remaining methods must stay `UNRESOLVED` and the link may still
fail. Do not add silent stubs that make the DLL link while disabling behavior.

The safe order is family-by-family, with the partial bridge first for the large
families (ModuleHost, audio, effects), then transport receive, then the smaller
families. The ownership cutover happens only after a probe.

#### Foundation-first bridge strategy

The linker is the worklist, but the unit of work is a capability foundation, not
one shim per symbol. The worklist tool is `devscripts/game_link_worklist.py`:

1. Read the linker undefined-reference list.
2. Classify each symbol: pure, singleton policy, device/transport, or state.
3. Pure/stateless code moves into `game.dll`; do not bridge it.
4. Singleton policy gets one typed family bridge.
5. Device/transport gets a bounded command or queue capability; never expose the
   device object or a raw operating-system handle.
6. State gets a resident record and stable handle; never pass C++ containers or
   references across the ABI.
7. Generate shims for scalar/string/vector signatures; hand-write only complex
   ownership and callbacks.
8. Batch all host ABI additions into one intentional cold EXE build.
9. After each batch, relink and record the remaining reference budget.

The foundation batch is:

- real `CAP_ENTITY` player/NPC records and `entity_read`/`entity_write`;
- inbound `CAP_TRANSPORT` delivery through
  `GameModuleV1::on_event(kEventTransportMessage)`;
- a server transport adapter with `send_to_player`/`broadcast`;
- `CAP_JOBS`, `CAP_CALLBACKS`, and `CAP_RESOURCES` generation leases;
- a `CAP_RENDER` command-stream skeleton.

#### Networking hot/cold split

**Cold (resident host):**

- sockets, UDP, ICE, framing, encryption;
- the reliability/ordering mechanism;
- peer/session table, timeouts, receive/send timing;
- transport mechanism setup.

**Hot (`game.dll`):**

- packet IDs, schemas, encode/decode;
- handlers and validation;
- reconnect/error policy;
- gameplay authority.

Rules:

- The host routes opaque `{channel, typeId, bytes}` and never parses payloads.
- Inbound messages use `TransportMessageV1` through
  `GameModuleV1::on_event(kEventTransportMessage)`.
- Outbound messages use `CAP_TRANSPORT`.
- No `SOCKET`, `sockaddr_in`, `IceAgent`, `ServerPlayer`, `World&`, or other
  C++ object crosses the boundary.
- Adding or removing packet IDs and handlers is hot. IDs are tombstones and are
  never reused.
- The reliability/ordering *guarantee* is cold; the decision to retry,
  reconnect, or abort is hot policy.
- `PACKET_SERVER_COMMAND` is the first networking proof because it is the
  terminal-first control path. It needs the inbound gate, the server transport
  adapter, the player/session state phone, and a bounded command policy before
  it can move authority out of the resident handler.

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

This is the implementation order for the whole-game save-to-live loop. The project completion target is one `game.dll` containing all eligible game implementation code, not an NPC-only DLL. The NPC path is a bounded loader proof within that whole-game effort. Inventory every repository `.cpp` before assigning the final source set; detailed semantic state/callback review may continue in dependency-connected batches. Keep world loading on its current path during the first loader proof.

#### Step 0: reuse what already exists and identify the seam

Before changing code, re-check the source inventory and these current implementation touchpoints:

| Existing path | Current job | How the pilot uses it |
|---|---|---|
| `build_game_dll.py` | Builds the existing focused game DLL source list. | Extend or replace its focused list for the pilot; do not mistake it for a whole-game build. |
| `src/hot-reload/game-api.h` | Declares the existing narrow `GameAPI` and export. | Evolve toward the versioned lifecycle/function-table boundary while preserving compatibility until the replacement is proven. |
| `src/hot-reload/hot-reload-system.cpp` | Loads candidates and currently triggers its focused DLL build synchronously. | Keep candidate loading/rollback ownership here or move it to the chosen resident owner; remove synchronous compilation from the game tick. |
| `src/engine/engine-tick-setup.cpp` | Calls the current reload check during engine setup/tick work. | Keep only a cheap notification/activation check at the safe boundary; never wait for compilation here. |
| `devscripts/module_ownership.py` and its catalog | Audits source-to-target ownership for existing subsystem modules and the EXE. | Extend the single ownership map to distinguish bootstrap, runtime, and game targets and reject duplicate/unassigned production owners. |
| `devscripts/cpp-source-inventory.py` | Enumerates repository `.cpp` files, active build membership, current owners, dependencies, and unload-risk candidates. | Keep the JSON as the detailed audit and CSV as the sortable map; the build coordinator should consume the canonical owner manifest rather than infer ownership from filenames. |

The current source-checked snapshot says the focused builder compiles two sources,
`src/effects/effect-part.cpp` and `src/hot-reload/quick-hit-debug-visual.cpp`, while both remain in the
583-source EXE list. The repo also declares a separate six-source CMake DLL target. These are separate source claims, not proof that the whole game is assigned to a DLL. Re-check the generated inventory before each migration batch.

Use the mapped server NPC path as the first trace: `runServer -> simulateSharedNpcs -> NpcSystem::updateOneWithTarget -> NpcSystem::updateOneNpc`. The existing functions pass live C++ references such as `Npc&`, `World&`, `Player&`, and `NpcMovementContext&`; those references are not the permanent cross-DLL interface. Inspect the actual field reads/writes and choose the smallest behavior decision that can be represented as plain input/output data. If the selected decision needs world information, request it through a bounded resident capability rather than passing a `World&` through the ABI.

#### Step 1: establish the persistent-data rule

- The resident host (currently the EXE) is resident code plus the owner of live process-memory records. The host image itself is not where changing player/NPC positions are stored.
- The resident host allocates and owns the state that must survive game-code replacement: for the pilot, at minimum the NPC's stable ID and the exact fields used by the selected behavior, plus whatever world/session identity is needed to prove the same test is continuing.
- Implement only the resident record types and fields needed by this pilot, while following the generic entity/type/schema-version rules above. Do not migrate every entity/component or build a reflective field database before the first reload works.
- `game.dll` owns replaceable behavior and temporary implementation state. Do not put the only copy of NPC position, health, identity, or session state in a game-DLL global, static, heap allocation, callback closure, or thread-local object.
- The first proof is code-only: keep the state schema unchanged. This isolates “did code reload preserve the NPC?” from “did a data migration work?” Add field/schema migration after code-only replacement passes.
- If the process exits, resident-owned memory also exits unless separately saved. Persistent-across-process saves remain a separate data-saving path; this plan promises continuity across a game-code DLL switch, not automatic disk persistence.

#### Step 2: install the one-time stable loader boundary

1. Define the minimum versioned C ABI needed for startup, querying resident capabilities, game-module initialization, one NPC decision call, quiescence, and shutdown. Put the authoritative declaration in one shared header. Include ABI version and structure-size fields.
2. Pass fixed-width values, plain structs, byte spans, and stable entity handles across the boundary. Keep C++ classes, STL containers, references, exceptions, RTTI-dependent objects, and allocator ownership on one side.
3. Keep the loader in the EXE; the resident host loads and validates the candidate `game.dll`. Keep a stable function table in the resident host and call the active generation through it.
4. Update the canonical root host target so this boundary is installed by one intentional initial EXE build. This is the expected cold boundary. After it is running, a game-owned `.cpp` edit must only compile/link a game DLL generation; it must not relink the EXE.
5. Retain current subsystem modules and the current world-loading implementation while the pilot is built. Do not copy the entire repository into a new repo or attempt to move world loading in the same boundary change.

Before proceeding, start the game once and confirm that the resident host and a trivial game module load, report their ABI/generation, reject an invalid candidate, and leave the process open. This verifies the loader before NPC behavior is mixed in.

#### Step 3: move one NPC decision behind the ABI

1. Keep the current server tick, entity lookup, world loading, physics/collision, and authoritative application in their existing owners for this proof.
2. At the chosen NPC decision seam, build a bounded `NpcBehaviorInputV1` from resident-owned data. Include stable entity IDs and only the values that decision needs. Add an output struct containing the decision/result; do not return pointers into game-DLL memory.
3. If the decision needs a world/nav query, expose a versioned resident capability that accepts stable handles and plain query/result structs. The resident host performs the query with current host-owned data; the game module chooses behavior from the result.
4. Validate the returned action in the authoritative owner, apply it to the same NPC record, and keep fixed-tick physics rules unchanged.
5. Initially leave the NPC object and complex systems in their current representation. Adapt only the selected decision seam. Later migrations can move whole subsystems once their ownership is clear.

The intended shape is:

```text
resident server tick looks up NPC state
  -> resident host makes plain versioned input
  -> game.dll computes NPC behavior
  -> host/server validates and applies result
  -> same NPC record continues to next tick
```

This is one ABI adaptation at the behavior boundary, not a wrapper added to every C++ function. Calls wholly inside `game.dll` remain ordinary C++ calls.

#### Step 4: make save trigger an asynchronous candidate build

- Use one watcher/coordinator owned by the existing dev-loop/module build system. `python devscripts/live.py` is the proposed human-facing entry point, but it does not exist yet. Do not start an independent competing build loop.
- Watch only the canonical source manifest and dependency graph. A saved game-owned source defaults to `game.dll`; runtime, bootstrap, tool, test, and data owners are explicit. Unknown ownership stops with a useful error instead of silently requesting an EXE build.
- On save, capture an immutable source revision (hash and changed paths). Compile only affected translation units plus required dependents, then link a candidate DLL in the canonical repository-local build path. Continue the running simulation while the build runs.
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
| Rollback proof | Activate a valid but deliberately worse behavior generation | Prior code is restored; resident-owned NPC/world state is retained. |

Do not combine the first code reload with a schema change, world-loader move, networking protocol change, or renderer-device rewrite. Prove each boundary separately so a failure has one likely cause.

#### Step 7: expand from one behavior to all eligible game sources

After the NPC proof passes, update the canonical ownership manifest so ordinary production implementation files under `src/**/*.cpp` default to `game.dll`. For each batch:

1. Select a dependency-connected group from the inventory and name its current owner, planned owner, persistent state, jobs/callbacks, and external resources.
2. Move ownership in the build manifest and remove each moved source from its old EXE/subsystem target in the same change. Enforce exactly one link owner.
3. Resolve EXE globals and static data by moving durable state into runtime records, converting constants to immutable module data, or making temporary state generation-scoped and resettable.
4. Keep third-party implementations, build tools, tests, bootstrap/process/device/transport mechanisms, and runtime/ABI mechanisms in their correct targets; list each exception and reason. Do not report these as ordinary game sources.
5. Build/activate through the canonical root dev-loop and exercise the real feature. Record source counts, compile/link/activation times, state continuity, and journal evidence for each batch.
6. Complete the semantic trace for every eligible game source and linked game dependency before changing source ownership. Record the state owner, callback/job quiescence, external-resource lifetime, ABI crossing, and unload decision per source.
7. Prepare one complete owner manifest, then move all eligible game implementation translation units into one `game.dll` in a single ownership cutover. Remove each from its prior EXE/subsystem target in that same change. Do not maintain a long-lived partial migration with ordinary game files split across targets.
8. Keep launcher/bootstrap, resident host, platform/device mechanisms, standalone tools, tests/demos, and unrelated vendor code outside `game.dll` with an explicit owner and reason. For linked vendor implementations used by the game, verify source/build provenance and route them into `game.dll` or document the verified binary dependency.

The loader and NPC continuity proof validates the mechanism; it is not an NPC-only source migration. Preparation may require multiple code changes, but the ordinary game-source ownership switch is one reviewed cutover. Keep one large `game.dll` unless measured link time, ownership isolation, or rollback requirements justify splitting it.

## 4. Migration phases

The NPC path is the first end-to-end proof of the loader, state continuity,
failure fallback, and rollback. It is not the migration destination. After the
proof, the ownership target is one `game.dll` for ordinary eligible game
implementation `.cpp` files across the project, with explicit documented
exceptions for bootstrap/runtime, device/platform, transport, third-party, and
build/test code. Split that DLL only when measurements justify it.

### Phase 0 — Inventory and baseline

- Enumerate every repository `.cpp` and every code-bearing input to the EXE, including C translation units and vendored static archives. Distinguish EXE sources from subsystem DLLs, launcher/tools/tests/demos, and unused vendor trees. Record actual source/archive provenance, resources, and runtime sidecar dependencies.
- For every source, record current owning target, overlapping/alternate target declarations, direct and transitive project-header dependencies, available compiler depfile dependencies and freshness, file size/hash, main-entry signal, and lexical state/callback/thread/job/network signals with line references.
- Map mutable state to its actual lifetime owner; trace callback captures/registration/unregistration, job submission/cancellation/join, and thread shutdown for every eligible game source and linked game dependency. Lexical matches are investigation leads, not safety findings.
- Record current EXE, narrow DLL, subsystem-module, CMake-declared DLL, and vendored static-library ownership separately from the proposed `game.dll` owner. The CSV and JSON source maps are linked below.
- Build a complete per-file semantic trace and one-owner cutover manifest for all active game code, including active vendored C++ implementations; explicitly mark stable launcher/runtime/platform/build/test exceptions.
- Measure current build stages and representative save-to-evidence cycles: save detection, affected translation units, compile, link, validation, activation, and first decisive journal event.
- Capture an FPS/server-tick baseline on named hardware and workload. Do not assume an NPC triangle-scan problem until the journal or profiler identifies it.

**First-pass file maps (2026-11-12):** [sortable per-file `.cpp` CSV](../../handoffs/cpp-source-inventory.csv), [full dependency/lifetime JSON](../../handoffs/cpp-source-inventory.json), and [EXE link-input inventory](../../handoffs/mimita-exe-link-inputs.md). The `.cpp` map covers every repository `.cpp`; the EXE map also identifies directly compiled C files, the resource, libraries, and runtime sidecars. These are static source/build-script maps, not proof that code is safe to unload or that current depfiles are fresh.

**Pilot-start gate:** enough evidence for the selected NPC path, not a full understanding of the whole repo: exact source/call path; fields read/written by the chosen decision; current owner of those fields; callbacks/jobs that can execute that path; existing loader/target; one repeatable map/workload; and an authorized route for the boundary build/runtime run. This lets Phase 1 begin while the wider inventory continues.

**Full Phase 0 exit evidence:** every repository `.cpp` and every code-bearing EXE link input classified; every eligible game source and linked game dependency semantically traced; one complete owner manifest prepared; alternate/duplicate targets and static-archive provenance resolved; dependency evidence refreshed for the current source revision; exact representative build/save-to-activation timings; named-hardware runtime baseline; and known state/loader boundaries. Track semantic review per source in the CSV; heuristic scan hits are leads, not completed reviews. The first-pass maps do not complete semantic lifetime review or the controlled timing/runtime baselines. Do not mark all of Phase 0 complete based only on the pilot-start gate.

### Phase 0 semantic-review evidence gate — every C++ source

The automated inventory is a map, not the review. Before the Phase 2 ownership cutover, complete a source-by-source semantic record for every repository `.cpp` and every code-bearing implementation linked into the game. “Complete” means every row has evidence and a disposition; it does not mean pretending that every file belongs in the game DLL. Build tools, tests, launchers, bootstrap/runtime mechanisms, unused vendor trees, and binary-only libraries must each receive an explicit owner/reason. Every active game implementation source must either qualify for `game.dll` or name the concrete blocker and its resident adapter/owner. Unknown or unverified is a blocking result, never an implicit pass.

For each source row, record:

1. **Identity and build ownership:** repository path, content hash/revision, current target(s), exact compile/link input evidence, duplicate/alternate declarations, proposed single owner, and whether it is active, unused, tool/test-only, or vendor code. For archives, identify the actual linked binary and prove source/build provenance before claiming its source is reloadable.
2. **Dependencies:** direct and transitive project headers, generated inputs, external libraries, link symbols or call edges that cross the proposed boundary, dependency evidence revision/freshness, and all callers/callees that matter to ownership. A header include graph alone is not a call graph.
3. **Persistent state:** each mutable global/static/thread-local, singleton, long-lived object, and externally stored pointer/callback introduced or used by the source; its true owner and lifetime; all readers/writers; initialization, reset, migration, and shutdown paths; and the exact resident record or generation-scoped state that will replace it. Classify immutable constants separately from mutable state.
4. **Asynchronous execution and callbacks:** every callback registration and capture, callback owner and unregister path; every thread, future, job queue, task and completion closure; cancellation, submission blocking, drain/join, and proof no callback, in-flight call, function pointer, return address, or job can enter the old generation after its unload barrier.
5. **External resources:** graphics/context objects, audio engines/voices, sockets/transports, OS handles, files/watchers, timers, library objects, and other resources. Record creation, use, release owner, release conditions, thread/context requirements, and recreation after activation. A numeric GL/socket handle does not make its resource lifetime safe.
6. **ABI and memory ownership:** every value/pointer/object crossing the proposed DLL boundary; whether it is plain versioned data or a stable handle; allocator and destructor ownership; and any adapter required to prevent STL, exceptions, RTTI, vtables, or old-generation memory from crossing the boundary.
7. **Disposition and proof:** `GAME_DLL_READY`, `RESIDENT_ADAPTER_REQUIRED`, `EXPLICIT_EXCEPTION`, or `UNRESOLVED`. Cite source functions/lines and the shutdown/activation path. `GAME_DLL_READY` requires evidence for all preceding fields and the safe-unload barrier, not merely absence of lexical matches. An exception requires its reason, owner, and whether changes to it require a cold host build.

Maintain these fields in the sortable per-file inventory (or a linked per-source review record), with reviewer/date/revision and evidence references. Review in dependency-connected batches, but reconcile the entire source set against one final ownership manifest. Every source in the final manifest must occur exactly once in its active link owner. Re-run source and build-input enumeration after review; any added, removed, renamed, or re-owned source invalidates its row until re-reviewed. Verify dependency snapshots against the same revision through the authorized root build workflow. Resolve saved-manifest-only vendor sources against the exact linked archive. Track C translation units and embedded/resource/link inputs alongside `.cpp`; they can also hold callbacks, state, or resources.

**No-unresolved-row cutover gate:** do not perform the whole-game ownership cutover while any eligible source/dependency has `NOT_REVIEWED`, `UNRESOLVED`, stale/unknown build provenance, an unowned callback/job/resource, or an unproved boundary crossing. The completion claim is “all discovered active inputs classified and every eligible game implementation source has evidence-backed ownership and unload disposition”; do not claim literal 110% certainty. New evidence can reveal missed inputs, so keep the manifest checkable and repeatable.

### Immediate next work and current evidence status (2026-11-12)

The inventory exists, but the migration is still in Phase 0. The current CSV contains 733 repository `.cpp` rows and marks all 733 `NOT_SEMANTICALLY_REVIEWED`. It maps 634 current-game or saved-library candidates (616 sources across the EXE/subsystem targets plus 18 Manifold manifest-only candidates); the EXE link-input report separately lists 22 directly compiled C files. A first detached-worker review record covers eight active EXE-owned sources and identifies blockers; it is not full semantic closure for those rows. The focused two-source DLL and existing subsystem modules are not the whole-game boundary. No universal save-to-game-DLL flow or whole-game continuity proof is established.

Do this next, in order, without changing source ownership early:

1. **Continue the per-file evidence batches.** Use the dependency map and risk signals to review each active source and linked game dependency. For every row, record actual target/overlap, content hash, state owner and lifetime, readers/writers/reset, callback/job/thread registration and stop/join, external-resource creation/release/recreation, ABI/memory ownership, source evidence, and disposition. Record `UNRESOLVED` wherever evidence is missing; do not mark a file ready because a scan found no candidates. Review all repository `.cpp` rows for an explicit owner, and include active C/vendor/link/resource inputs. The first detached-worker record is `docs/handoffs/cpp-lifetime-batch-detached-20261112.md`.
2. **Resolve build and dependency truth.** Verify the exact canonical EXE/DLL/archive link inputs and duplicate target claims. Verify or regenerate dependency evidence for the same source revision through the authorized root build workflow. Prove whether the listed Manifold sources produced the archive actually linked. Keep launcher, tools, tests, bootstrap, resident host, device/platform, unused vendor, and binary-only dependencies explicitly classified.
3. **Implement and prove the resident reload barrier.** Route all game-generation calls through one stable resident function table. At swap, block new calls and task submissions; unregister callbacks; drain/cancel and join managed work; wait for in-flight calls; release generation-owned resources while their required context is current; and require an explicit quiesced/ready result before unload. Preserve state in resident versioned records or validate a transactional migration. Keep the previous generation active on any busy, failed, or invalid result. Replace detached workers or give them a resident task owner/generation lease. Ensure partial candidate initialization has a cleanup path.
4. **Create one canonical source-owner/dependency manifest and its save coordinator.** Every active source has exactly one owner. A saved game `.cpp` compiles into a candidate game DLL; a header edit rebuilds affected dependents; adding or removing a game `.cpp` updates the candidate source set and link; unknown ownership stops with a diagnostic. Snapshot/hash one coherent revision, debounce/coalesce saves, compile/link in the background through the existing root dev-loop/module build authority, validate the candidate, and notify the resident host for safe-point activation. Compilation must not block the simulation thread. Assets/configuration use explicit data reload routes; host/loader edits remain explicit cold-boundary changes unless migrated behind a stable resident service.
5. **Prove the complete path before cutover.** In one process, verify edit of an existing source, addition of a source, removal of a source, dependent-header edit, compile failure retaining the previous generation, recovery, state continuity, and rollback. Record exact source revision, affected translation units, DLL hash/generation, unchanged EXE PID, activation/rollback events, and runtime journal. The NPC continuity path is the first behavior proof, not the final ownership scope.
6. **Perform one reviewed ownership cutover.** Only after every eligible game source/dependency has an evidence-backed disposition, the resident barrier is proven, and the manifest has no unresolved eligible rows, move all eligible game implementation sources into one `game.dll` and remove them from old targets in the same cutover. Retain explicit non-game and resident exceptions with reasons. Re-enumerate the resulting link inputs and verify each source has one owner.

**Meaning of “edit any C++ file live”:** the target is that any ordinary active game implementation source can be added, changed, or removed and the new valid game generation can become active in the same process. It does not mean the code that keeps the process alive and loads DLLs can unload itself; the bootstrap/resident host must remain. Standalone tools/tests and unused vendor sources are not game DLL members. Changes to those owners use their own build/reload route. Do not describe the contract as “every repository `.cpp` updates live” unless the owner manifest and proof explicitly cover that file category.

### Execution order and proof ownership (2026-11-12)

- **Stage 0 (agent, no build):** canonical manifest, add/remove/rename discovery, reverse header dependents, coherent-revision check, unknown/duplicate-owner rejection.
- **Phase 0 audit (agent, many batches):** 733 rows remain `NOT_SEMANTICALLY_REVIEWED`; 14 have partial batch evidence. Record the seven required fields and a source-cited disposition per row. `UNRESOLVED` blocks cutover.
- **Stage 1 (agent writes; root dev-loop owner builds):** versioned resident-host C ABI, result-returning quiesce/shutdown, EXE-host loader, generation validation.
- **Stage 2 (audit-driven):** move durable state into resident records; files with unresolved ownership stay in their old target.
- **Stage 3 (agent writes; owner builds):** generation barrier (entry refcount, callback/job registries, drain/join, resource release, partial-init cleanup).
- **Stage 4 (agent writes; owner builds):** background whole-game candidate builder and save coordinator driven by the manifest.
- **Probe proof (root dev-loop owner or human, validation only):** loader/barrier works in one process; the probe is reverted before cutover.
- **Stage 5 cutover (agent prepares; owner builds and proves):** assign every eligible game source exactly once to `game.dll`, remove it from prior targets in the same change, keep documented resident/tool/test/vendor exceptions, and re-enumerate link inputs.
- **Final proof (root dev-loop owner or human):** existing-source edit, added source, removed source, dependent-header edit, compile failure, recovery, and rollback in one process, with unchanged PID, state continuity, revision, DLL hash/generation, and exact `events.jsonl`.

Current measured status: 773 `.cpp` rows at manifest revision `e9a3aa3ff807d6872c8cb773f8df3cdf3098c3738e9c31ddb9f8ba71eff8f54d`; 685 rows are active in their owner build. The canonical owner map resolves all 770 `.cpp` rows: game 603, resident 28, bootstrap 4, module 12, test 33, tool 1, vendor 88, launcher 1 (`config/source-owners.json`,
           `devscripts/source_manifest.py`). The whole-game candidate compiles (603 game sources) and links at generation 75 with 0 undefined / 0 unique resident/vendored symbols; a usable candidate DLL exists. The rebuilt host loaded and initialized generation 75 in a dedicated server (PID 13028, run `20261113_090828`), but it has not run the full game loop or cut over. The chosen design keeps the resident host in the EXE.

Static audit triage (2026-11-12): 66 rows confirmed static-closed by a full read, 0 static-closable candidates, 41 state-review rows, 183 resident-adapter-required rows, 299 runtime-proof rows, 27 resident-host rows, 135 non-game rows, 0 unresolved. Every lexical-clean row has had a confirmation read. The resident-service dependency map is `docs/handoffs/resident-service-dependencies-20261112.json`. See `docs/handoffs/cpp-audit-static-triage-20261112.md` and the first batch `docs/handoffs/cpp-audit-batch-pure-npc-20261112.md`. No lexical-clean rows remain; every remaining row is explicitly state-review, adapter, runtime-proof, resident-host, or non-game.

Resident-service seam (400 sources call at least one): `Debug::log*` 195, `StructuredLogger` 163, `Terminal` 100, `TerminalState` 87, `WeaponRegistry` 52, `DebugVis` 48, `GuiLayoutManager` 42, `ModuleHost` 37, `Perf` 35, `PhysicalEntitySystem` 30, `GamemodeRegistry` 25, `MapConfigRegistry` 25, `AuthSystem` 22, `gpMpContext` 21, `serverGamemodeState` 20, `VehicleSystem` 17, `SizeScalingConfig` 16, `NpcDifficultyConfig` 15, `MovementJsonConfig` 13, `AnalyticsManager` 10, `BehaviorProfileRegistry` 9, `VehicleConfigRegistry` 7. Logging, terminal state, and GUI layout are the dominant seams, so logging/perf/config/registry/terminal/GUI host services are the first ABI capabilities to cover; a file either calls that service through the host table or stays resident with a thin bridge.

### Later multi-DLL design — defer until one DLL is proven

Do not split the initial migration. First prove one `game.dll` can be built from the full eligible game-source set, safely activated, rolled back, and preserve resident state. Splitting now would create more module boundaries before the ABI, lifetime barrier, and whole-game ownership manifest have been exercised.

When measured compile/link time, independent ownership, or failure-isolation needs justify a split, make multiple DLLs managed modules under the same resident host:

- Give each module a stable module ID, ABI/version and structure sizes, code hash, state-schema version, declared capabilities, and explicit dependency list in one manifest.
- Keep dependencies acyclic and load/activate them in topological order. Cross-module calls use versioned function tables or resident-host services and stable handles; no C++ object ownership, STL container, exception, allocator, or private-generation pointer crosses a module boundary.
- Treat a dependent module set as one compatible generation. Validate that all required module IDs, ABI versions, capabilities, and dependency hashes agree before activation; reject partial or mixed generations.
- Stop new calls, unregister callbacks, block submissions, drain/join work, and wait for in-flight calls per module before unload. If modules share callbacks/resources/state, unload or activate that dependency group atomically.
- Preserve independent rollback generations for the module set. If any member fails validation or its first-call health check, keep or restore the previously active compatible set.
- Split only after profiling shows a useful reduction in save-to-live latency or a concrete ownership/isolation benefit. Track per-module compile, link, validation, activation, dependency fan-out, and rollback costs; avoid one DLL per source.

The target remains one cold, minimal bootstrap/resident host plus hot game behavior. A true platform/device/loader or incompatible public-ABI change can still require a deliberate host boundary build; keep that list explicit and shrink it only when a stable resident service replaces the mechanism.

### Pre-cutover stages 0-4 (one all-files cutover)

The ownership change is one reviewed cutover. These stages prepare it with current ownership unchanged, so the EXE keeps running exactly as today until Stage 5. None of these stages may claim runtime proof before the root dev-loop owner or human runs the exact process.

#### Stage 0 - canonical manifest and dependency discovery

- `config/source-owners.json`: exactly one owner per source (`game`, `resident`, `bootstrap`, `launcher`, `tool`, `test`, `vendor`, `module:<name>`); explicit overrides carry reasons; unknown or duplicate ownership is an error.
- `devscripts/source_manifest.py` with `--write`, `--check`, `--diff`, and `--dependents`: owner resolution (module > override > ordered rules), coherent revision, add/remove/rename detection, reverse header dependents.
- Coherent revision = hash of the sorted `{path: sha256}` set plus build-config hashes. Never build a partially saved or mixed tree.
- `devscripts/cpp-source-inventory.py` consumes the resolver and reports `owner_category`, `owner_reason`, and unresolved rows.
- Acceptance: every repository `.cpp`, the EXE C inputs, the linked archive, and the resource input have one owner; deterministic revision; seeded unknown/duplicate cases fail; no `src/` change and no build.

#### Obsolete-code removal (allowed over time)

The project is client/server-only. Code that exists only for a removed local-only/single-process architecture is obsolete and should be deleted over time, not preserved for compatibility.

Allowed without a separate classification pass when it is obvious:

- zero callers, zero includes, zero build references, and zero config/terminal/asset references;
- the behavior is unused or already owned elsewhere.

Rules:

- Delete in a focused change; do not bundle an unrelated cleanup.
- Update the owning manifest/build target and re-enumerate.
- Record the deletion in the changelog.
- If it is not obvious, leave it and write a note; do not guess-delete.
- Packet IDs are tombstones: removal never reuses an ID.
- Preserve unrelated user changes and existing lane data.

#### Stage 1 - versioned resident-host C ABI

- Shared header `src/host/abi/game-host-abi.h`: ABI version and structure sizes; plain data, stable handles, result-returning `quiesce`/`shutdown`; host service table for the seam (entity/world access, config, logging, render, network, audio, files, time, jobs).
- New services are reached through one generic versioned capability lookup (`get_capability(id, version)` returning a plain-data struct with `version`/`structSize`), not by adding a field per service.
- Structured logging crosses the boundary as a bounded UTF-8 JSON string (`kMaxLogPayloadBytes`). Typed event structs are a deferred optimization, adopted only if measured logging cost justifies them.
- Resident loader `src/host/host-loader.h` in the EXE validates version/size/hash, runs partial-initialization cleanup, and keeps the old generation on any failure.
- Resident runtime `src/host/host-runtime.h/.cpp` owns the loader, barrier, and host services. It is idle unless `MIMITA_GAME_PROBE=1`.
- The existing `GameAPI` path stays until the new path is proven. This is a deliberate cold boundary.
- Scaffold status (2026-11-12): headers and the host runtime exist; the runtime is opt-in and has not been compiled by an agent.

#### Stage 2 - resident state ownership (audit-driven)

- Generic resident record store with entity/type identity, schema version, stable handle, migration/default hook, and direct access (no per-tick reflection).
- Durable state moves out of DLL globals/statics per reviewed audit row; files with unresolved ownership stay in their old target and block the cutover.
- Acceptance: probe state survives a swap; failed migration keeps old state and old code active.

#### Stage 3 - generation barrier

- `src/host/host-barrier.h`: entry refcount; generation-stamped callback registry with bulk unregister; job/task registry with cancel/drain/join; resource release under the required context; explicit `QUIESCED`/`BUSY`/`ERROR`; guaranteed partial-initialization cleanup.
- Detached workers are replaced by a resident executor or generation lease.
- Acceptance: quiesce refuses while in-flight > 0; no callback, job, function pointer, or return address can enter the old generation after unload.
- Scaffold status (2026-11-12): header exists and is not yet called by the EXE.

**Mandatory generation rules (no per-file runtime proof needed when these hold):**

- No raw `std::thread`, `std::async`, detached worker, or `CreateThread` in a `game` row; all async goes through `CAP_JOBS` and is generation-stamped.
- No raw callback registration; callbacks use generation tokens through `CAP_CALLBACKS` and are unregistered on quiesce.
- No generation-owned GL/audio/socket/file handle that outlives a call; use `CAP_RESOURCES` so the host can release it under the required context.
- `quiesce()` reports `BUSY` until entries, callbacks, jobs, and resources are zero.
- If anything remains, unload is refused: the old generation stays active, the loader emits an Important/Error `game.generation.unload-refused` event naming the blocking owner/category, and no silent `FreeLibrary` occurs.
- Partial candidate initialization must have a cleanup path.
- A static gate rejects banned raw patterns in `game` rows; a row that bypasses the barrier is a defect, not an unproven row.

#### Stage 4 - background whole-game builder and coordinator

- `devscripts/game_build_worker.py` compiles the manifest's `game` sources, links an immutable `game-g<N>.dll`, verifies source coherence before and after, and publishes `build/game/current.json`.
- `devscripts/live.py` watches sources and headers, debounces/coalesces saves, pins one coherent revision through `devscripts/source_manifest.py`, spawns the worker in the background, and publishes `build/game/ready.json` for safe-point activation.
- Unknown ownership stops with a diagnostic; compile failure keeps the active generation.
- Acceptance: one coherent revision per build; add/remove/rename and header changes rebuild the right translation units; no compiler invocation from the manifest tool.
- Diagnostic status (2026-11-13): all 603 `game` sources (575 original + CAP_LOG bridge + compat bridge + transport bridge + module/audio/effects/entity/lifetime/render bridges + whole-game entry/dispatch) compile as DLL translation units. Before CAP_LOG the link failed with 389 unique resident/vendored symbols; after the global/replay/audio/RenderDevice/diagnostics/ICE/TransportSession/AuthoredRebuild bridges it links at generation 68 with 0 undefined / 0 unique and 0 duplicate definitions; all previously listed families are 0. Static gate is now **0/0**. Stage 2 resident state covers world-loaded/map/mode, player/world/NPC/weapons/multiplayer-context, camera, game state, chat history, freecam/editor/death/selected-object, duel config, replay pointers/collections, command binds, and debug scalars. The compatibility bridge was removed. The host has a barrier-guarded whole-game frame driver and `CAP_RENDER` command execution; frame ownership was attempted in a client and crashed in the menu. Moving `main-init`/`main-systems` ownership to the game also reintroduced 33 bootstrap/resident link references, so the ownership switch was reverted and `frameCapable` remains off. Game-owned `DuelManager`/`GamemodeManager`/`FramePacer` objects remain in a small game module. Client/direct socket paths now use opaque handles; server packet-send call sites still need the final CAP_TRANSPORT pass. GL create/delete is host-owned, but draw code still uses raw GL ids; full `CAP_RENDER` draw-state ownership remains. A transitional `src/glad.c` object is linked only for the diagnostic DLL; CAP_RENDER remains the cutover-safe replacement. See `docs/handoffs/cpp-game-dll-diagnostic-link-20261112.md`. This is compile/link evidence, not runtime proof.

Foundation batch status (2026-11-12): the host ABI now declares `CAP_ENTITY`,
`CAP_RENDER`, `CAP_JOBS`, `CAP_CALLBACKS`, and `CAP_RESOURCES`. The resident host
has a real entity adapter path for server players/NPCs, inbound
`TransportMessageV1` delivery through `on_event(kEventTransportMessage)`, a
server transport adapter, generation job/callback/resource leases, and a render
command-stream skeleton. The probe generation links with the new game-side
bridges and logs a `PACKET_SERVER_COMMAND` policy decision (`admincmds`/
`player_list`) using the player snapshot from `CAP_ENTITY`; exact `admincmds` is
now authoritative in the probe. The host was rebuilt through the dev loop. A real server/client run
(`20261112_215614`) proved inbound delivery and player state parity for
`admincmds`: the game decided `accepted=1` and the resident handler also
accepted with `admincmds=1 (host)`. A first authority attempt reported
`ack=1` from the host but the client did not observe the result. A second attempt
used a host-owned `TransportResponseV1` envelope and the resident server sent a
valid 264-byte `PACKET_SERVER_COMMAND_RESULT` to the exact inbound endpoint
(`sent=true`, magic/version/type/tick verified). The client logged
`network.server-command.result`; exact `admincmds` is now authoritative in the
probe, including the enabled/disabled host status cases. No
same-PID swap, compile-failure, rollback, or state-continuity proof was run. The symbol worklist is
`devscripts/game_link_worklist.py` and
`docs/handoffs/game-dll-undefined-worklist-20261112.md`.

#### What is left by gate (2026-11-12)

| Gate | Current | Needed |
|---|---:|---|
| Whole-game link | **0 undefined / 0 unique** (generation 75) | 0 undefined |
| Live activation | probe swap, failed-candidate and failed-compile fallback, rollback, callback resume proved; whole-game generation 75 loaded/initialized | full-game tick/render + swap + coordinator |
| State continuity | position/tick continuity in probe | resident records + schema migration |
| Networking hot | exact `admincmds` authoritative | all packet handlers through the host |
| Render/world | ABI skeleton | `CAP_RENDER`/`CAP_WORLD` + GLAD path |
| Static gate | **0 violations / 0 files** | 0 |
| Cutover | not started | 584 game sources moved |
| Runtime campaign | not started | add/remove/edit/rollback proof |

#### Command response envelope (2026-11-12)

The host owns the response endpoint; the game owns the response bytes.
`TransportDispatchV1` carries an inbound message and a bounded
`TransportResponseV1`. This avoids exposing `SOCKET`/`sockaddr_in` and avoids
depending on a cached player address.

Current status: the envelope is implemented and the server sends a valid
264-byte `PACKET_SERVER_COMMAND_RESULT` to the exact inbound source endpoint. The
client receives it and logs `network.server-command.result`. Exact `admincmds`
is now authoritative in the probe; the resident handler does not send a second
result for that command. Other command arguments and packet families remain
resident.

#### Live activation (2026-11-12)

`HostRuntime::poll()` reads `MIMITA_GAME_READY` or `build/game/ready.json`. When a
new `generation`/`dll` is ready, it:

1. quiesces the barrier and waits up to 2 seconds;
2. loads and validates the candidate while the old generation is still loaded;
3. quiesces and shuts down the old generation;
4. commits the candidate and starts the new generation;
5. keeps the old generation if quiesce or load fails.

Probe evidence (`20261112_230534`): the same server PID `22784` and client PID
`11324` started on `generation-20`, rejected a missing `generation-99` candidate
while staying alive, activated `generation-21`, then rolled back to
`generation-20`. Exact `admincmds` produced the game-owned client result before,
after the failed candidate, after the swap, and after the rollback. Player
position and server tick continued within normal idle/physics movement. This is
probe-only live activation, not the full game cutover. The barrier now preserves
callbacks through a failed quiesce and can resume the old generation; a standalone
barrier test proved `Busy -> resume -> callback` works. A forced probe compile
failure left `current.json`/`ready.json` unchanged and the host stayed on the old
generation. Whole-game swap, schema migration, add/remove/header-edit through the
coordinator, and cutover remain to be proved.

#### Whole-game entry point (2026-11-13)

The whole-game source set now has its own `src/game/game-module.cpp` export,
separate from the probe export. It initializes the resident capability bridges
and handles `kEventTransportDispatch` through the shared
`server-command-dispatch-module.cpp`. The probe module is excluded from the whole-game
source set. Full engine tick/render orchestration remains the next migration
step; the entry point currently makes the DLL loadable and event-capable.

#### Deferred cleanup: legacy `Debug::log` facade

`Debug::log*` already forwards to `StructuredLogger`; it is an API cleanup, not
a second log sink. It is not part of the live-activation critical path.

- Do not add new `Debug::log*` call sites.
- Resident/bootstrap call sites must be migrated before the all-files cutover
  because `debug-log.cpp` is game-owned.
- Remove the remaining game-side call sites over time; delete
  `debug-log.h/.cpp` when the count reaches zero.
- `logAuto`'s `"WARNING"`/`"ERROR"` string parsing needs manual classification.
- Add a static gate rule when the facade is removed.

#### Long-term agent build path: dev-loop `game-dll` request (Option C, not implemented)

The standing agent path is a dev-loop request, not a blanket build override:

- The dev-loop owns the game-DLL build queue, lock, and status.
- Agents may request `game-dll` and poll progress; they never invoke a compiler or the EXE builder.
- The request builds only `game.dll`/probe outputs under `build/game`, using the persistent object cache, parallel jobs, and live progress.
- One game-DLL worker runs at a time. EXE builds and runtime launches remain owner-only.
- This is not implemented yet. Until it is, a session may use the narrow game-DLL worker only under an explicit human build-authority exception, recorded in the changelog.

#### Temporary validation probe

The probe is the NPC target-selection decision in `simulateSharedNpcs` (`src/network/server-npcs.cpp`). It is validation-only and is reverted before the Stage 5 cutover; it is not a partial migration and does not change the final owner set.

- Input/output: `src/host/abi/npc-target-probe-abi.h` (`NpcTargetDecisionV1`, event `720`, up to 64 plain candidates).
- Probe generation: `src/game/probe/npc-target-probe-module.cpp` exports `GetMimitaGameModuleV1` and runs `src/npc/npc-targeting.cpp`.
- Build set: `config/probe/npc-target.json`; build with `python devscripts/game_build_worker.py --sources-file config/probe/npc-target.json --out build/probe/npc-target`.
- Host: `src/host/host-runtime.cpp` loads the candidate only when `MIMITA_GAME_PROBE=1` and `MIMITA_GAME_DLL` point at it.
- Parity-only: the host logs `npc.target.probe` with the inline target, the probe target, `match`, candidate count, and generation. The inline result stays authoritative until parity is proven.


#### Stage 5 - single all-files cutover (gated)

- Requires: no unresolved eligible rows, a proven barrier, and the probe proof.
- Add every eligible `game` source to `game.dll` and remove it from prior targets in the same change; retain documented resident/bootstrap/tool/test/vendor exceptions.
- Re-enumerate link inputs and verify one owner per source. Keep one `game.dll` unless measured link time or ownership isolation justifies a split.


### Phase 0 information-sufficiency gate

This gate defines when the project stops gathering information and starts changing code. It is deliberately measurable.

**Passes and outputs**

- **Pass A — runtime quiesce plan:** every `RUNTIME_PROOF_REQUIRED` row is classified as `TRANSIENT_CANDIDATE` or `OWNED_PERSISTENT`, with its owner/stop/join/release path and barrier rule. Output: `docs/handoffs/cpp-runtime-quiesce-plan-20261112.md`.
- **Pass B — state-owner decisions:** every `STATIC_STATE_REVIEW` row has a per-symbol decision: immutable constant, generation cache, durable resident record, external resource, or callback/job target. Output: `docs/handoffs/cpp-state-owner-decisions-20261112.md`.
- **Pass C — capability map:** every `RESIDENT_ADAPTER_REQUIRED` row maps to a capability operation or an explicit resident bridge. Output: `docs/handoffs/cpp-capability-map-20261112.md` and the seed `HostCapabilitiesV1` table.
- **Packet catalog:** every packet ID, field schema, encode/decode site, handler, owner, schema hash, and error/timeout/reconnect path. Output: `docs/handoffs/cpp-packet-catalog-20261112.md`.
- **Render command inventory:** every draw primitive, GL state, shader, and resource lifetime that `CAP_RENDER` must cover. Output: `docs/handoffs/cpp-render-command-inventory-20261112.md`.
- **World-query inventory:** every query shape/site and the data `CAP_WORLD` must expose. Output: `docs/handoffs/cpp-world-query-inventory-20261112.md`.
- **Static gate rules:** the banned raw patterns (thread, callback, GL/socket/file handle) that make per-file runtime proof unnecessary. Implemented in `config/game-static-gate.json` and `devscripts/game_static_gate.py`; report `docs/handoffs/cpp-static-gate-20261112.md`.
- **Baseline measurements (owner-run):** current EXE build time, candidate `game.dll` link time, and a baseline journal.

**Definition of "enough information"**

All of these are true:

1. `STATIC_CLOSABLE_CANDIDATE = 0` and `UNRESOLVED = 0`.
2. Every row has an owner category, dependencies, state owner, async/resource owner, and a disposition.
3. Every state-review row has a per-symbol decision.
4. Every adapter row maps to a capability operation or a resident bridge.
5. Every runtime row maps to a barrier rule; no row depends on a per-file runtime test. The static gate rules and baseline are recorded; driving the gate to 0 is code-phase work, not an information-gate condition.
6. The packet catalog, render command inventory, and world-query inventory are complete.
7. One canonical manifest has exactly one owner per source.
8. The owner has a baseline build/run and a measured candidate-link time.

When these are true, stop auditing and start code changes. New unknowns found later are handled as new rows, not a reason to reopen the whole audit.

**Per-file runtime proof = 0**

The mandatory generation rules in Stage 3 make the barrier the proof mechanism. A file does not need its own runtime test; it must use `CAP_JOBS`, `CAP_CALLBACKS`, and `CAP_RESOURCES`. The barrier campaign proves the categories once: stop entries, unregister callbacks, cancel/drain/join jobs, in-flight zero, release GPU/audio/net under context, and keep the old generation on failure.

**Preserving state is efficient**

Resident records are the live state; game code accesses them by handle. A code-only swap copies nothing. Migration happens only when a schema changes, not on every reload. Reflection is not used on the hot path. Transient UI state may be preserved in resident UI records or reset; resetting is cheaper and acceptable where specified.

**When code changes begin**

After the gate: install the mandatory barrier rules and generic capabilities once, convert adapters in capability batches, move state per Pass B, run the small pre-flight probe, perform the single ownership cutover, then run one category-level runtime campaign and rollback.

Before the gate, do not change source ownership and do not claim runtime proof.

**Code-phase exit and pre-cutover gate**

Code changes start after the information gate. The pre-cutover gate requires:

- the static gate reports 0 violations across `game` sources;
- the barrier category proofs pass;
- one manifest has exactly one owner per source;
- obsolete-code removals and folds are recorded;
- one runtime campaign with rollback passes in the same process.

### Silent and long-running work

Agents and tools must make long work observable; a silent process is not proof of a hang.

Before starting a long or potentially silent operation, state:

- what it does and the expected phases;
- the expected scale/duration (minutes, not a promise);
- where progress is written (a machine-readable file and a log);
- how to distinguish running from stuck;
- the exact command and the watch path.

Standard watch paths:

- game DLL build: `build/game/progress.json` and `build/game/progress.log` (`[SKIP]` / `[CXX ]` lines);
- fast triage: `build/audit-triage-progress.json`;
- full inventory: flushed stdout phase lines; a dedicated `build/inventory-progress.json` is planned, not implemented;
- dev loop: `python devscripts/dev-loop.py --status`, `.dev/current-run.json`, and each instance's `events.jsonl`;
- EXE build: `build/progress.json` and the build log.

Do not kill a process that appears silent until checking its progress file, log tail, CPU, and command line. Silence while a tool scans files or writes a large artifact is common.

Routine ownership/triage refresh uses the fast manifest scan (`audit_triage.py --from-manifest`). The full inventory is the audit-evidence snapshot and is not required for every change. Tools must flush progress lines and write the machine-readable progress file.

The dedicated server, client, and command path must be runnable entirely from the terminal. Terminal commands are the primary control surface; the GUI is optional. New controls should be exposed as terminal commands unless they genuinely require the GUI.

### Phase 1 — Resident-host/game boundary and NPC vertical proof

- Install the resident state store (in the EXE host), versioned component records, stable game ABI, generation validation, safe call/job quiescence, and rollback path.
- Move one complete NPC decision/update path into the pilot `game.dll` target. Keep the same NPC identities, positions, health, world, and connected session in resident-owned state.
- Save a change to NPC movement code. Build and activate it without relinking the EXE or restarting the process. Confirm that the NPC behavior changes and the pre-existing NPC/world state remains.
- Record the candidate generation and the relevant before/after NPC, movement, frame-time, and server-tick evidence in the exact running process journal.

**Exit evidence:** real same-process activation, state continuity, compile error keeps the old version active, and rollback restores the prior behavior without losing world state.

### Phase 2 — One whole-game source-ownership cutover

- Complete the semantic trace for every eligible game translation unit and linked game dependency before changing ownership. Every source needs a reviewed persistent-state owner, callback/job quiescence path, external-resource lifetime, ABI crossing, and unload decision.
- Prepare one canonical manifest assigning every eligible game implementation source exactly once to `game.dll`. Resolve alternate CMake, focused-DLL, and subsystem claims in that same manifest. Retain explicit bootstrap, resident host, platform/device, tool, test/demo, and unrelated vendor exceptions.
- Prove the loader and NPC behavior as acceptance probes before ownership cutover. The NPC probe validates state continuity and rollback; it does not make NPC-only migration the destination.
- Change all eligible game-source ownership in one reviewed cutover: add them to `game.dll` and remove them from prior EXE/subsystem targets together. Preparatory loader/host work may happen beforehand while current ownership remains intact; do not leave a long-lived partial source migration.
- Generalize the incremental coordinator to the complete manifest. Compile affected objects in the background, link immutable generations, validate, and activate only after generation-owned callbacks/jobs are quiescent.
- Measure single-source, shared-header, and whole-DLL link times. Keep one DLL unless measurement justifies a split.

**Exit evidence:** the complete manifest has one owner per eligible source; the whole game runs from the active `game.dll`; representative edits across ownership groups activate without EXE relink or process restart; and state continuity, fallback, rollback, and journal evidence pass.

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

## 6. Current source status (2026-11-12)

The refreshed source map enumerated all 733 repository `.cpp` files. Of 596 `src/` translation units, 583 are currently compiled into the EXE and 12 are built as subsystem DLLs; the remaining probe module `src/game/probe/npc-target-probe-module.cpp` is intentionally outside the EXE until its validation run. `build.py` also compiles 20 Recast C++ files into the EXE. That is **603 directly compiled EXE C++ files**, not 616. The EXE also compiles 22 C files (GLAD and libjuice), links `libmanifold.a`, and embeds `mimita.rc` resources. A saved, dated 2026-09-30 Ninja manifest lists 18 Manifold `.cpp` files, but does not prove the current archive source provenance. The other repository `.cpp` files are outside the reviewed game source/link manifests. See the [per-file CSV](../../handoffs/cpp-source-inventory.csv), [full JSON](../../handoffs/cpp-source-inventory.json), and [EXE link-input inventory](../../handoffs/mimita-exe-link-inputs.md). This completes file enumeration, not semantic lifetime review.

The current `build_game_dll.py` target compiles only `src/effects/effect-part.cpp` and `src/hot-reload/quick-hit-debug-visual.cpp`, both also assigned to the EXE. `CMakeLists.txt` separately declares a six-source DLL target; the canonical `build.py` does not use that target. `HotReloadSystem` watches the focused list and invokes its build synchronously. The current `GameMemory`/`GameAPI` supports a small effect/debug feature, not a complete game/resident-host boundary. No whole-game `game.dll` or persistent-state resident-host implementation was verified.

This map is a complete file enumeration and static triage pass, not a complete semantic safety audit. Compiler depfile freshness is unknown; static/global, callback, and job matches need ownership/capture/shutdown review before any source is unloaded. The report explicitly marks the remaining evidence gaps.

The repository also has a subsystem `ModuleHost` and catalog. These are existing migration assets, not evidence that all `.cpp` files are hot. During the transition, keep current module builds working until their sources and behavior have been proven in the `game.dll` path. Do not bulk-reassign files in the catalog and claim they are reloadable without a successful runtime proof.

## 7. Acceptance gates

Do not claim the target is implemented until the relevant running-game evidence exists:

1. Saving NPC movement C++ produces a candidate game generation without an EXE relink or process restart.
2. The same NPC IDs, positions, health, world, client/server session, and editor documents survive activation.
3. The changed NPC behavior is observed in play and in the correct process's `events.jsonl`; a generation-loaded event alone is insufficient.
4. A compile error, ABI mismatch, failed schema migration, and runtime regression each leave the last-known-good generation and state usable; rollback is observed in the same process.
5. The source inventory assigns every active implementation file exactly once to bootstrap/resident-host/game ownership; a saved ordinary game `.cpp` is detected automatically.
6. Latency evidence includes build stages and generation identity; no fixed sleep is needed.
7. The 240 FPS frame-budget and 60 Hz server-tick goals are measured under stated workload and hardware, separately from code-reload success.
8. Multiplayer activation is coordinated across trusted connected clients; incompatible clients are held on the prior generation or safely disconnected, never silently divergent.
9. GUI/rendering, creative-data collaboration, and supported backend recreation each receive their own real runtime proof before being marked complete.

A build proves compilation/linking. An activation event proves code was selected. Only exercising the behavior and observing the resulting state proves runtime behavior. Visual/gameplay acceptance remains a separate human judgment.

## 8. Historical and implementation-specific notes

The remaining sections document the existing subsystem-module implementation, past module proofs, inventories, and JSON hot reload. They remain useful evidence. Where they describe subsystem DLLs as the final target, an EXE-owned kernel, or map-editor-first migration order, the architecture and phases above supersede that older target. Do not treat an old module activation or dated inventory as proof of the new resident-host + whole-game `game.dll` design.
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

Cold EXE builds are reserved for the resident mechanism itself: process lifetime, the loader/barrier, OS/device/driver access, the transport mechanism (sockets, ICE/UDP, framing, encryption, channel and reliability machinery), document storage, and the public capability table. These are cold because they are process-wide or ABI-defining, not because behavior is unimportant.

Packet schema, packet IDs, encode/decode, handlers, session/reconnect/error behavior, render primitives, draw policy, world-query composition, gameplay, GUI, audio policy, and tool behavior are hot. They live in `game.dll` and change without relinking the EXE as long as they use the existing generic capabilities.

A game/module source, module-private header, or data/config edit must not relink the EXE. The working goal is "effectively never" for behavior work, not a literal promise of zero for clean machine builds. Warm boundary builds use ccache and object caching and target seconds; clean release/install builds remain slower.

The long-term path to fewer boundaries is a generic, versioned, data-driven
event/behavior IR: new gameplay mechanisms should use existing host
capabilities and event/data schemas rather than add a new EXE call site. EXE
changes then remain limited to process, authority, device, and transport
mechanisms. See
[`module-migration-plan.md`](../../architecture/live-development/module-migration-plan.md)
and
[`self-contained-workspace.md`](../../architecture/live-development/self-contained-workspace.md).

### Cold-build escalation and debt

The default is hot. A cold build is a debt, not a normal tool.

If an edit appears to require a cold build:

1. Stop and write a prominent note: which owner/mechanism is cold, which file cannot be reached hot, and which capability or generic seam is missing.
2. Do not treat the cold build as the fix. Design the hot-capable seam first.
3. It is acceptable for this to take days with no gameplay testing; designing the generic capability is the work.
4. Record the occurrence with the exact source, why it was cold, the smallest hot-boundary change, and the build result.
5. Only the root dev-loop owner/human decides to run the one-time boundary build.
6. Every cold occurrence must name the capability that would retire it. The cold list must shrink over time; a new permanent cold boundary needs the same review as a new capability.

### Cold core and hot targets

The permanent cold core is the resident host, loader, barrier, OS/device/driver interface, transport mechanism, document storage, and the public capability table. These define process lifetime or the ABI; they cannot unload themselves.

Everything above that core is a hot target: networking behavior and packet schema, render primitives and draw policy, world-query composition, physics/collision policy, gameplay, GUI, audio policy, and tools. A mechanism moves from cold to hot by installing one generic capability once. After that install, new uses are `game.dll` changes only.

Do not delay behavior extraction because a mechanism is "cold forever." Whenever a mechanism moves, it obeys the same rule as everything else: one intentional host boundary build for the generic seam, then module-only changes.

## Hot subsystem target: networking and NPC (2026-11-09)

The target is not "some policy callbacks are hot." Entire behavior subsystems
are hot. The host exposes bounded inputs and capabilities; the module owns the
subsystem state machine and returns decisions/actions.

### Networking (hot, including server error paths)

Networking is hot and reloadable as one subsystem, not just "some policy callbacks." The resident host owns the transport mechanism: sockets, ICE/UDP, framing, encryption, channel and reliability machinery, and the opaque payload envelope. `game.dll` owns the packet catalog, packet IDs, field schema, encode/decode, handlers, map-sync, reconnect, session-health, snapshot, spawn authority, combat-validation, and outage/error behavior.

Adding, changing, or removing a packet is a `game.dll` change. The host routes `{channel, typeId, payload bytes}` without parsing the payload. Packet IDs are stable and never reused within a live session. The candidate announces its schema hash and generation identity; incompatible peers are held on the prior generation or disconnected, never silently divergent. Transport security and framing remain resident.

Because a wrong packet or reconnect path can corrupt a session, networking changes must show journal evidence for the real error path — drop, reorder, timeout, reconnect, and server error — not only the happy path. A clean build or a successful connect is insufficient evidence.

The concrete capability shape is `src/host/abi/transport-capability.h`; the staged T0-T5 plan is `docs/handoffs/cap-transport-design-20261112.md`. T0 installs the capability (ABI, host runtime, resident client adapter; compiled); T1 moves `PACKET_SERVER_COMMAND` first because the dedicated server, client, and command path must be runnable entirely from the terminal, with GUI optional.

### NPC perception and behavior

`npc-behavior` owns goal/action/targeting/combat/team policy. `npc-perception`
owns FOV/range/LOS gating, memory decay, belief, and prediction. The host owns
actor state, the immutable world/navmesh query mechanism, collision and physics
application, and the fixed tick. The `CAPABILITY_NPC_WORLD` capability exposes
only bounded line-of-sight/ray queries; the module decides when to ask and how
to gate the result. Editing perception, sensing cadence policy, targeting, or
navigation choice must not relink the EXE.

The ideal cold list is therefore only process lifetime, loader/barrier, device/driver/backend, transport mechanism, document storage, and the public capability table. Packet schema, render primitives, world-query composition, physics/collision policy, and all other behavior are hot subsystem code with versioned plain-data inputs, module-owned state, and host validation.

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

As of 2026-11-11 18:41 EST, Mode 2 agent lanes are deferred. All work uses
the canonical root checkout and root dev-loop; do not create or start lane
workers. Existing lane data remains parked and preserved. The root dev-loop is
the build authority. See
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
