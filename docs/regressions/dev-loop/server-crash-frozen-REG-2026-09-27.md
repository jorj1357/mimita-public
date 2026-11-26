# Dev loop looked frozen after the server crashed before room-code startup

Time created: 2026-09-27T01:45:00Z
Time last updated: 2026-09-28T23:18:00Z

Status: ATTEMPTED FIX (2)

Related specification:
`docs/operations/task-completion/task-completion.md`

Related changelog:
`docs/changelog/2026-09-27/20260927_014500-dev-loop-server-crash-status.md`

Related changelog for Occurrence 2:
`docs/changelog/2026-09-28/20260928_231500-dev-loop-server-crash-fix.md`

## Observed

The build linked successfully and published build 11, then the server exited
before writing a room code:

```text
[DEV] server exited before room-code handshake: 3221225477
```

Windows code `3221225477` is `0xC0000005`, an access violation. The loop then
returned to idle without a visible failure status, making it appear frozen.

## Expected behavior

The loop must clearly report that the launched server crashed, persist the
failure state, and remain available for a deliberate `[1]` retry after the
other agent has finished changing or rebuilding the code.

## Confirmed cause

The server child exited before room-code publication. The dev loop's failure
branch printed one line, stopped its tracked children, and returned without
updating `last_message`, saving `server_failed`, or redrawing the control menu.
The underlying access violation in the game binary is not yet diagnosed and
must not be guessed from this orchestration symptom.

## Attempted fix 1

`devscripts/dev-loop.py` now records `server_failed`, prints the exit code, and
prints the status menu after both early server exit and room-code timeout.

The loop does not automatically retry an access-violation crash, because that
would create a crash/relaunch storm while another agent may still be editing.

## Proof status

- Python syntax validation is required after this edit.
- Human review must confirm that a crashed server produces a visible failure
  message and that pressing `[1]` retries once the build is ready.
- The game-side `0xC0000005` remains unresolved until the server's crash output
  or dump identifies its first failing owner.

---

## Regression Occurrence 2 — fly check crashed the dev-loop server

Time:
`2026-09-28T23:07:00Z`

### Observed

The dev loop published a server and client, but the client stayed in:

```text
ICE: waiting for server answer...
[NET TICK] sock=INVALID_SOCKET state=NatNegotiating connected=0 active=1 transport=0
```

The server log stopped before answering the join request. Windows recorded an
access violation (`0xc0000005`) in the dev-loop server executable.

### Expected behavior

The server must remain alive while simulating its startup NPCs, poll the
coordinator, answer the room-code/ICE join, and let the client enter the game.

### Confirmed cause

The new fly movement branch in `src/physics/physics-mini.cpp` used:

```cpp
if (MP_CONTEXT.active && MP_CONTEXT.flyEnabled)
```

`MP_CONTEXT` is the macro `(*gpMpContext)`. A dedicated server simulates NPC
physics but has no client `gpMpContext`, so the first NPC physics tick
dereferenced a null pointer and crashed the server. The client then waited for
an ICE answer from a process that no longer existed.

Evidence:

- Windows Application Error: faulting executable was
  `C:\mimita-v9\.dev\builds\0355\mimita.exe`, exception `0xc0000005`, fault
  offset `0x00000000002fe5e6`.
- `addr2line` mapped that offset to `physicsMainUpdate_Internal` at
  `src/physics/physics-mini.cpp:196`.
- The server log showed ICE registration and room-code publication, then
  stopped at server tick 181 without any `[ICE HOST REQUEST]` response.
- The client log showed `begin-join accepted` followed by repeated
  `server answer timeout`.

### Attempted fix 2

The fly branch now checks the optional pointer directly:

```cpp
if (gpMpContext && gpMpContext->active && gpMpContext->flyEnabled)
```

This preserves flying for a real client and makes headless server/NPC physics
skip the client-only branch safely.

### Prevention

- Any code that uses `MP_CONTEXT` must first prove `gpMpContext` exists when it
  can run during dedicated-server or NPC simulation.
- A server startup test must keep `--npcs 1` enabled, wait for the room code,
  and verify the server continues emitting heartbeats after client join.
- A dev-loop ICE test must distinguish “server answer timeout” from “server
  crashed”; preserve the child exit code and Windows crash evidence.

### Proof status

Status remains `ATTEMPTED FIX (2)` until a fresh dev-loop build starts the
server, publishes a room code, completes ICE, and accepts the client in a live
human test.

---

## Regression Occurrence 3 — server access violation after generation 507 hot swap

Time observed: `2026-11-22 17:46:02Z` crash artifact; dev-loop exit observation at `2026-11-22 17:47:40.921Z`.

### Observed

Run `20261122_174002` was an FCS session with server PID 9196 and client PID 34012. The server journal logged `game.generation.activated` for `build/game/generation-507/game-g507.dll` at `17:46:02.130Z`. The crash report `.dev/crashes/20261122_174002/crash-2026-11-22_12-46-02-9196.txt` records `ACCESS_VIOLATION (0xc0000005)`, a read access at `game-g507.dll+0x1649ae`; the matching `.dmp` is 206,640 bytes. The dev-loop later recorded server exit code `3221225477` (`0xc0000005`) and marked the server offline while client PID 34012 remained alive/frozen. The server journal ends at `17:46:02.132Z`, at the hot-swap boundary. The crash report's stack includes `GetMimitaGameServerPolicyV1+0xc1aa` and host frames.

### Attribution status

The published DLL contains DWARF `.debug_info` and `.debug_line` sections. `addr2line` needs the PE preferred image base added to the crash RVA: `0x180000000 + 0x1649ae` maps the faulting instruction to `std::__new_allocator<glm::vec<3, float>>::construct` at MSYS2 `bits/new_allocator.h:203`, through `std::vector<glm::vec3>::push_back`. The crash report's caller frame is `GetMimitaGameServerPolicyV1+0xc1aa`; the export RVA is `0x3d1902`, so its link-time address `0x1803ddbac` maps to `mimita::game::serverRuntimeSimulateWorld(void*)` at `src/game/server-runtime-module.cpp:326`. That line is immediately after the call to `MimitaNet::simulateSharedNpcs(...)` at lines 322-325. This localizes the active path to shared NPC simulation and vector construction, but does not identify the exact project expression that supplied the invalid destination/reference. The DWARF path points into a pruned earlier source snapshot; the current source has the same lines, but the exact generation-507 snapshot is not present. Causation by hot swap or a particular source edit remains unproven. The crash artifact timestamp and the dev-loop's process-exit observation differ by about 99 seconds; preserve both rather than presenting the later monitor observation as the exact fault time.

### Required follow-up

- Preserve the minidump and exact generation-507 DLL; do not overwrite the crash evidence.
- Preserve the same DLL's DWARF sections and analyze the minidump against it; capture the source snapshot before pruning on future candidates so the mapped path resolves to the exact build input.
- Trace the `std::vector<glm::vec3>::push_back` call path inside `MimitaNet::simulateSharedNpcs` and identify the exact vector and ownership/lifetime contract before changing code.
- Compare the generation-507 source snapshot and live persistent state before changing code. Do not infer a layout-migration bug solely from the timing.
- Reproduce only through the authorized root dev-loop when the active runtime owner permits it; verify candidate load, sustained server health after swap, and that the client reconnects or exits cleanly.

This occurrence is unresolved. It does not negate the observed candidate publication or activation, but it prevents claiming a healthy sustained post-swap runtime.

## Regression Occurrence 4 — Vehicle Free Play server crashed after readiness

Observed: `2026-11-26`, run `20261126_145549`.

### Observed

The dev-loop launch report returned `client_connect_timeout` after the server emitted server gameplay readiness. The server had loaded `arena1`, bound UDP `127.0.0.1:59031`, and spawned the vehicle-arena world state. The client PID 29616 remained alive but never reached Connected; the server was offline in the run registry and the dashboard did not show a live server. The user reports this as a repeat of the launch failure, but the earlier user-reported occurrence is not yet correlated to a run ID.

### First failing step and evidence

The first failed step is server process survival after gameplay readiness, before client connection. The dev-loop recorded server PID 10544 exiting with code `3221225477` (`0xc0000005`) at `2026-11-26T14:56:35.419Z`. Crash artifact:

- `.dev/crashes/20261126_145549/crash-2026-11-26_09-55-49-10544.txt` and matching `.dmp`.
- The report identifies a read access violation at address `0x6c` in `libstdc++-6.dll` (`ZSt11_Hash_bytesPKvyy+0x40`).
- Symbolization against published game DLL generation 688 resolves the game frames through `VehicleSystem::profileFor(const VehicleState&) const`, called by `tickArenaBoostPads`, `MimitaNet::serverTickVehicles`, and `mimita::game::serverRuntimePublish`. The resolved profile lookup code is `VehicleProfileRegistry::find(state.profileId)` at the launch build's source snapshot `src/vehicles/vehicle-system.cpp:521`; the caller is `src/network/vehicle-networking.cpp:374` in its recorded source snapshot.
- Two additional preserved dumps from the same launch window show the same access violation and stack: `.dev/crashes/20261126_145453/crash-2026-11-26_09-54-59-2704.txt` and `.dev/crashes/20261126_145549/crash-2026-11-26_09-56-12-12892.txt`. The latter is a second GUI-launched server in the same run, consistent with the user’s report that a server terminal opens and closes; that linkage is an inference from timing and mode, not proven. Client-01 PID 29616 emitted `network.gui-server.launch` at `2026-11-26T14:56:11.806Z` for child PID 12892, port 1357, map `arena1`, mode `vehicle_freeplay`. The event records empty `client_events_jsonl` and `server_events_jsonl`, and `run.json` has no instance for PID 12892; this launch bypassed dev-loop instance registration. The event does not identify whether a human, agent, or automation initiated it. Its matching dump records the same profile lookup crash. The dashboard launch separately created managed server PID 10544, whose own journal is `server-01/events.jsonl`, and it crashed on the same path.
- Run journal: `logs/2026-11-26/20261126_145549/dev-loop/events.jsonl`; launch report identifies `client_connect_timeout`.

The stack identifies the failing lookup path, but the invalid object/string lifetime or state that caused the bad hash input is not yet proven. Do not treat the call site alone as root cause.

### Required fix and proof

Coordinate with the active owners of `src/vehicles/vehicle-system.cpp` and `src/network/vehicle-networking.cpp` before editing. Trace the profile ID's construction and registry lifetime through server tick initialization and boost-pad processing. Make the smallest owner-level correction, rebuild through the root dev-loop, and run Vehicle Free Play on `arena1` through that same loop. Proof must show the exact build/generation, server remains alive beyond the connection timeout, the client reaches Connected, and the dashboard/run registry reports a live server without a fresh crash artifact. Preserve all three dumps and journals.

### Coordinated candidate repair and preliminary runtime evidence

The active `rocket-league-crashfix` agent recorded DLL change note `d461f52d1888` and changed the vehicle profile lookup path to publish `shared_ptr<const VehicleProfile>` snapshots under a `shared_mutex`, removed the duplicate `VehicleSystem::pollReload()` from `serverTickVehicles`, and added `vehicle.profile.registry` publication evidence. The note calls out the tick-0 reload as the launch-crash trigger; the original dumps prove the profile lookup crash, while the exact overlapping thread operation is still being confirmed.

Run `20261126_154720` started managed server PID 3664 on `vehicle_freeplay` / `arena1` at `2026-11-26T15:47:21Z`. The server published profile registry revision 1/count 1, spawned both authored vehicles, and the client reached Connected at `15:47:28.578Z`; server heartbeat and tick events continued. Game DLL generation 692 activated in both processes at `15:50:01Z`. The client continued applying snapshots through `15:50:14.534Z`, and the server emitted heartbeats through `15:50:18.037Z`. Dev-loop then recorded an explicit AI stop request at `15:50:14.134Z`; this run did not close itself. The pair was healthy for about 2m53s overall and remained alive for at least 12 seconds after generation 692 activation.

This is preliminary evidence only: generation 692 activated by hot swap after the server had already started, `run.json` reports `build_match=false`, and one run does not satisfy the agent's requested five-launch series. Require fresh launches pinned to the final candidate, each with matching EXE/DLL identity, Connected state, and at least 45 seconds of server heartbeats after startup; distinguish any explicit test stop from a spontaneous exit. The complete journals remain at `logs/2026-11-26/20261126_154720/{server-01,client-01}/events.jsonl` and the dev-loop lifecycle at `logs/2026-11-26/20261126_154720/dev-loop/events.jsonl`.

### Confirmed cause (2026-11-26, `rocket-league-crashfix`)

The profile lookup crash is a **live container rebuild read by another thread**,
not a bad profile id.

`VehicleProfileRegistry::loadDirectory` assigned `mProfiles = std::move(profiles)`,
destroying every live element of the `unordered_map`. `find()` returns a pointer
into that map, and `tickArenaBoostPads` calls it through
`VehicleSystem::profileFor`. `serverTickVehicles` called
`VehicleSystem::instance().pollReload()` whenever `tick % 60 == 0`
(`src/network/vehicle-networking.cpp`), and **`tick 0` satisfies that**, so every
server start rebuilt the registry on its first server tick while the main thread
was still doing its own first load. Two threads, one singleton, one rebuild in
flight. The recorded fault — `std::_Hash_bytes` reading `0x6c` in
`libstdc++-6.dll` — is the freed bucket array, which is why the call site looked
like "profile lookup" and the id itself was never at fault.

### Fix

- `VehicleProfileRegistry` now stores `std::shared_ptr<const VehicleProfile>`
  values behind a `std::shared_mutex`; `loadDirectory` builds a new map and swaps
  it under the write lock, and `find` returns a snapshot taken under the read
  lock. A reload can no longer free a profile that a caller is using.
- `VehicleSystem::profileFor` returns that snapshot; the vehicle, HUD and
  networking call sites hold it for the duration of use.
- The server-tick reload is **kept** (a dedicated server never runs the engine
  config-polling path, so this is its only profile reload) and is now safe.
- New evidence event `vehicle.profile.registry` records `revision`, `count` and
  `directory` on every publish.

### Regression introduced while fixing it (recorded deliberately)

The first version read the map key from `profile.id` in the same expression that
did `std::move(profile)`. Evaluation order is unspecified, the moved-from string
was used as the key, and all three profiles collapsed onto the empty key:
`vehicle.profile.registry count=1`, `vehicle.profile.applied=0`, the car had no
arcade profile and could not be driven for five launches. Fixed by capturing the
id before the move. The new registry event is what made this visible — a
registry that loads fewer profiles than files now shows up immediately.

### Proof

Game generation 693, five consecutive launches (runs `20261126_161031`,
`161111`, `161152`, `161232`, `161313`), each: `0xC0000005` count 0,
`dev-loop.instance.exited` count 0, `vehicle.profile.registry` count 3,
`vehicle.profile.applied` 2, client `vehicle.entered` 1.

Reload stress in run `20261126_161313`: touching
`config/vehicle-profiles/rl_soccar_freeplay.json` while the server ran produced
`vehicle.profile.registry` revision 1 then revision 2, both count 3, with zero
crashes and the server still running — the case that used to crash.

### Rule to avoid it in future

1. A configuration registry publishes **immutable snapshots**; a reload may never
   destroy or mutate data another thread can be reading. Never hold a raw pointer
   into a registry container across a reload.
2. Config reload may be triggered from any tick (dedicated servers have no engine
   poll), but the rebuild itself must be lock-published.
3. A registry must log what it published (`revision`, `count`) so "loaded 1 of 3
   files" is visible instead of presenting as "the car does not drive".
4. When moving an object into a snapshot, read any key/id from it **before** the
   move.

### Status

Resolved with proof on generation 693; keep the three original dumps as the
pre-fix evidence.

### Status

Unresolved; one coordinated candidate run connected and remained healthy until an explicit dev-loop stop, but fresh-build and repeated-launch proof is pending. The repeated crash pattern is confirmed for this launch window. Whether it shares a cause with the earlier user-reported occurrence remains unknown.
