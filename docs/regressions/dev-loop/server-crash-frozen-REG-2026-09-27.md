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
