# Development Loop Bypassed the GUI Room-Code/ICE Join Path

Time created: 2026-09-27T01:16:26Z
Time last updated: 2026-09-27T01:16:26Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/networking/networking.md`

Relevant section: `48. Server startup` requires starting the server, receiving
a server code, and then joining it as a client. The normal GUI path must use
the coordinator/room-code join flow; a localhost shortcut is not an
acceptable replacement for multiplayer development.

Related changelog:
`docs/changelog/2026-09-26/20260926_210526-development-loop-ccache.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-09-26 local development session; recorded 2026-09-27T01:16:26Z`

The development loop launched a server and client from `C:\mimita-v9`, but the
client was started with a direct localhost address. The room code was not
shown. Human review also reported a server-position error of approximately
`8.14` while the player had not moved, instead of a value near zero.

### Expected Behavior

The development loop must reproduce the GUI server-start sequence:

1. Start the dedicated server with a room-file output path.
2. Wait for the server to register with the coordinator and write its room
   code.
3. Show that room code once.
4. Start the client with that room code so it enters the normal asynchronous
   ICE join path.

The test must not substitute `127.0.0.1` direct UDP for the room-code/ICE
path.

### Actual Behavior

The old development loop used:

```text
mimita.exe --server --bind 127.0.0.1:1357 ...
mimita.exe --connect 127.0.0.1:1357 --map coolplace --name "NPC Dev"
```

The server received no `--room-file`, so there was no file from which the
development loop could read or display the coordinator-issued room code. The
client therefore bypassed the GUI's room-code/ICE join branch.

### Why This Is Bad

The local direct-UDP path is not the real multiplayer startup path. It can hide
room registration, coordinator lookup, ICE negotiation, join-token handling,
and the normal authoritative initial-state synchronization. It therefore
cannot be used to judge multiplayer correctness or server-position behavior.

### Specification

`docs/specs/networking/networking.md`, section `48. Server startup`:

> Press Start, receive a server code, then join the server as a client when
> Connect to this server is selected.

The implementation evidence also follows the existing GUI owner in
`src/gui/gui-main.cpp`: it creates a room file, passes `--room-file` to the
server, waits for the written code, and places that code in
`MultiplayerConnectInfo.roomCode`.

### Wrong Code

File:
`devscripts/dev-loop.py`

```python
server_bind = self.profile.get("server_bind", "127.0.0.1:1357")
client_connect = self.profile.get("client_connect", server_bind)
...
server_args = [str(exe), "--server", "--bind", server_bind, ...]
...
client_args = [
    str(exe), "--connect", client_connect,
    "--map", str(self.profile.get("map", "coolplace")),
    "--name", str(self.profile.get("client_name", "NPC Dev")),
]
```

### Confirmed Cause

The cause of the missing room code is confirmed: the dev-loop server command
did not pass `--room-file`, and the dev-loop client command selected
`--connect`, which is explicitly the direct-UDP branch.

Evidence:

- The captured child command line was
  `mimita.exe --server --bind 127.0.0.1:1357 ...`.
- The captured client command line was
  `mimita.exe --connect 127.0.0.1:1357 ...`.
- `src/network/server.cpp` writes a room code only when
  `options.roomFilePath` is non-empty.
- `src/engine/engine-tick-state.cpp` selects direct UDP when
  `MultiplayerConnectInfo.directAddress` is non-empty and selects ICE when
  only `roomCode` is present.

The exact cause of the reported `8.14` server-position error is not yet
confirmed. It must be re-tested after the correct room-code/ICE path is active;
the direct path is not valid evidence for that measurement.

### Attempted Fix 1

Time:
`2026-09-27T01:16:26Z`

Change:

- Added `--room` launch parsing to `LaunchOptions`.
- Added room-code boot wiring in `src/main.cpp` so `--room` populates
  `MultiplayerConnectInfo.roomCode` without setting `directAddress`.
- Changed `devscripts/dev-loop.py` to create a temporary room file, pass it as
  `--room-file`, wait for the server-written code, print `ROOM CODE` once, and
  launch the client with `--room <code>`.
- Changed the profile bind to `0.0.0.0:1357`, matching the GUI server launch
  shape.

Result:

The source compiles and the dev-loop Python syntax check passes. The new path
has not yet received human runtime confirmation through a completed server
registration and client ICE join, so this record remains an attempted fix.

### Corrected Code

Files:

`devscripts/dev-loop.py`

```python
room_fd, room_file_name = tempfile.mkstemp(
    prefix="mimita-dev-room-", suffix=".txt"
)
os.close(room_fd)
self.room_file_path = Path(room_file_name)
self.room_file_path.write_text("", encoding="utf-8")
...
"--room-file", str(self.room_file_path),
...
room_code = self.room_file_path.read_text(encoding="utf-8").strip()
...
print(f"[DEV] ROOM CODE: {room_code}")
...
client_args = [str(exe), "--room", room_code, ...]
```

`src/network/net_mode.cpp`:

```cpp
else if (std::strcmp(argv[i], "--room") == 0 && i + 1 < argc)
{
    options.roomCode = argv[++i];
    options.roomCodeExplicit = true;
}
```

`src/main.cpp`:

```cpp
if (launchOptions.connectExplicit || launchOptions.roomCodeExplicit)
{
    GAME_STATE = GAME_PLAYING;
    MultiplayerConnectInfo info;
    info.shouldConnect = true;
    info.directAddress = launchOptions.connect;
    info.roomCode = launchOptions.roomCode;
    info.mapName = launchOptions.mapName;
    setPendingMultiplayerConnect(info);
}
```

### Fix

The dev loop now follows the GUI-owned room-file handshake and passes the
coordinator-issued room code into the existing ICE join path. Direct localhost
UDP remains available only through the explicit `--connect` harness path and
is no longer used by the NPC development profile.

### Proof

Automated proof:

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python build.py build-only`: passed; `mimita.exe` linked successfully.
- The build compiled the changed `src/main.cpp`, `src/network/net_mode.cpp`,
  and the relevant connection consumers.
- A controlled dev-loop start from `C:\mimita-v9` printed the status menu once
  instead of redrawing it every idle poll.

Human review:

Still required: run the new profile, observe a non-empty room code, confirm
the client logs `[ROOM JOIN START]`, confirm ICE connects, and repeat the
no-input server-position check. Do not mark this regression solved until that
is observed.

### Solution

Not confirmed yet.
