# Fast local server start

- Status: `PASS_WITH_HUMAN_REVIEW`
- Recorded: `2026-10-24T03:03:39Z` (2026-10-23 23:03:39 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD: `2103321b012f546c07bb418add26269c66bbace0`
- Request: start local authoritative gameplay immediately, then publish the room to the coordinator when registration finishes.

## Result

The GUI Start Server action and the preset start/join helper now queue the
client's local direct UDP connection immediately. The dedicated server starts
TURN credential retrieval, candidate gathering, and room registration on a
background thread. Once registration succeeds, the server thread adopts the
ICE listener, publishes shared room state, emits `network.room.registered`, and
writes the room file for remote room-code joins. A new
`network.server.local-gameplay-ready` event identifies when the authoritative
UDP loop is ready while coordinator registration is pending.

This removes coordinator/ICE setup from the local gameplay critical path. It
does not make map loading or NPC collision preparation instantaneous. A
coordinator registration failure leaves the local dedicated server running but
does not retry registration in this change.

## Exact source and specification changes

### `src/gui/gui-main.cpp`

`launchServerPresetAndJoin()` at lines 365-429:

Before:

```cpp
auto queueRoomJoin = [](const std::string& roomCode,
                        const MimitaNet::ServerLaunchSettings& launchSettings) {
    gPendingConnect = {};
    gPendingConnect.shouldConnect = true;
    gPendingConnect.roomCode = roomCode;
```

After:

```cpp
auto queueLocalJoin = [](const MimitaNet::ServerLaunchSettings& launchSettings) {
    gPendingConnect = {};
    gPendingConnect.shouldConnect = true;
    gPendingConnect.directAddress =
        "127.0.0.1:" + std::to_string(launchSettings.port);
```

The new-server, matching-server reuse, and listen-server fallback branches all
use `queueLocalJoin()`. The GUI Start Server action at lines 1037-1092 also
queues `directAddress`, the selected map, and server name immediately and
leaves the server screen. Room-file completion at lines 724-743 preserves an
active local connection; it queues an ICE room-code fallback only if there is
no active client connection.

Before the GUI Start Server action only launched the child and printed:

```cpp
printf("[ONLINE MENU] Use 'Connect Localhost' or run: --client --connect 127.0.0.1:%u --name <name>\n",
       gServerLaunchSettings.port);
```

After, it sets `gPendingConnect.shouldConnect = true`, sets the local address
to `127.0.0.1:<port>`, copies the map and server name, and switches to
`GAME_PLAYING`.

### `src/network/server.cpp`

`runServer()` at lines 674-718 and 744-805 replaces the synchronous startup
call:

```cpp
if (!initServerIceListener(dedicatedIceState)) {
    logServerExit(1, "coordinator_ice_registration_failed", "", 0);
    return 1;
}
serverCode = dedicatedIceState.serverCode;
```

with a worker that initializes a separate `ListenServerState`, signals
completion under a mutex, and leaves the main server thread free to run. The
main thread moves the completed ICE listener into `dedicatedIceState`, calls
`publishServerIceListener()`, writes `--room-file`, and logs room registration.
On failure it logs that local service remains available. The new
`network.server.local-gameplay-ready` event is emitted at lines 709-717.
Registration worker cleanup is joined at lines 1174-1175.

### `src/network/server-ice.cpp` and `src/network/server.h`

`initServerIceListener()` at line 181 no longer mutates shared hosted-room and
server-coordinator globals from the registration worker. The previous inline
publication started with:

```cpp
hostedRoomSession().active = true;
hostedRoomSession().roomCode = hostResult.roomCode;
setServerCoordinatorState(hostResult.roomCode, hostResult.joinToken);
```

The new `publishServerIceListener(const ListenServerState&)` at lines 290-304
performs those mutations on the server owner thread after successful
registration. Its declaration was added at `server.h:1537`; the synchronous
in-process listen-server caller also invokes it after initialization.

### Specifications

`docs/specs/networking/networking.md:4135-4192` now defines local host direct
UDP gameplay while coordinator registration runs in parallel, with room-code /
ICE still used by remote clients. Its clarification at lines 4202-4210 states
that local acceptance does not prove external reachability. This follows the
user's requested startup order and supersedes the earlier local-host
room-code-before-play wording in this section.

`docs/specs/gamemodes/gamemodes.md:68-72` aligns the disconnected `startmelee`
contract with the same local-ready / background-registration order.

## Pre-existing work and preservation

The checkout already contained extensive uncommitted changes, including the
networking and gamemode specifications, `gui-main.cpp`, `server.cpp`,
`server-ice.cpp`, `server.h`, and many gameplay/configuration files. Their
inventory is recorded in
`docs/changelog/2026-10-24/20261024_022552-server-startup-delay-investigation.md`;
those unrelated edits were preserved. The later-modified
`src/engine/engine-tick-ui-overlays.cpp` was also left untouched.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/networking/networking.md`,
  `docs/specs/gamemodes/gamemodes.md`, `docs/specs/performance/performance.md`,
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/workflows/runtime-scenario-validation.md`,
  `docs/operations/build-and-exe/build-and-exe.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/skills/logging-checker-v1.md`, and
  `docs/skills/efficiency-checker-v1.md`.
- `spec-behavior-review-v1`: PASS after updating the conflicting local-host
  startup wording to match the user's explicit requested behavior; remote
  coordinator registration and ICE joins remain required.
- `logging-checker-v1`: PASS; startup readiness and registration completion
  have owner-level structured events, with registration stages already
  duration-logged.
- `efficiency-checker-v1`: PASS for the focused source review; blocking
  coordinator requests and ICE gather no longer execute on the authoritative
  server loop. No post-change timing claim is made.
- No regression entry was added: this change implements the user's newly
  specified local-host startup order and does not alter the dev-loop's remote
  room-code acceptance path.

## Validation and remaining evidence

- `git diff --check` passed for the changed source and specification files.
- The dev-loop published build 2998 from source generation 60 with
  `latest_build_stale=false`. Its executable identity was checked with
  `--versioninfo`: `C:\mimita-v9\.dev\builds\2998\mimita.exe`,
  `EVENTS_JSONL_PATH=logs/10-23-2026/20261023_230445/events.jsonl`.
- An earlier canonical request on source generation 56 failed at the separate
  modified `src/engine/engine-tick-ui-overlays.cpp:316`: its `drawColumns`
  lambda expected 9 arguments but a call provided 8. The dev-loop later
  published build 2998 successfully after source generation 60; that file was
  not changed as part of this task.
- No gameplay scenario was run. The existing dev-loop pair remains alive on
  build 2989 (`run_id=20261024_024705`, server PID 32420, client PID 4760), and
  `auto_restart=false`. The live pair was preserved; build 2998's
  `--versioninfo` run is identity evidence, not runtime behavior proof.
- Human confirmation is still needed that Start Server enters local gameplay,
  server input packets are accepted during coordinator registration, and a
  remote client can join after the room code is published.
