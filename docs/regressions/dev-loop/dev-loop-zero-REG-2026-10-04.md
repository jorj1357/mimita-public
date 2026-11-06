# Dev-loop Published EXE Crash

Time created: 2026-10-04T20:19:34Z
Time last updated: 2026-10-04T20:19:34Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/operations/build-and-exe/build-and-exe.md`

Related changelog:
`docs/changelog/2026-10-04/20261004_201934-dev-loop-exe-validation.md`

---

## Regression Occurrence 1

### Observed

Time:
`2026-10-04T20:11:27Z`

The NPC development loop launched published build 1176 with the dedicated
server arguments for `dust2cyberiav4` and the server exited before the room-file
handshake with Windows status `3221225477` (`0xC0000005`).

### Expected Behavior

The published executable must start the headless server, load the selected map,
register its room, and write the room code before the client is launched.

### Actual Behavior

Build 1176 crashed immediately, even before it could print the normal boot line.
The same current root executable started the server successfully with the same
map and NPC arguments.

### Why This Is Bad

The dev loop presented an unusable published artifact as the latest successful
build, so every server launch failed before the client could connect.

### Specification

`docs/operations/build-and-exe/build-and-exe.md`

The build workflow requires the produced executable to be inspected after the
build and launched from the project root/runtime environment.

### Wrong Code

File:
`devscripts/dev-loop.py`, `publish_build`

```python
shutil.copy2(source_exe, destination / "mimita.exe")
```

### Confirmed Cause

The published 1176 file had valid PE headers but its entrypoint bytes were
zero-filled. Windows therefore raised an access violation before application
startup. The root executable had nonzero entrypoint bytes and a different
SHA-256 hash.

Evidence:

- 1176: `_executable_snapshot_is_valid` reports `entrypoint bytes are
  zero-filled`.
- Current root executable: validation reports `ok`.
- A clean replacement published as 1177 started, bound UDP, loaded the map,
  spawned one NPC, registered room `NCY9XJT`, wrote the room file, and exited
  normally only because the test timeout was reached.

### Attempted Fix 1

Time:
`2026-10-04T20:19:34Z`

Change:

`devscripts/dev-loop.py` now validates PE headers, executable sections, and the
entrypoint; reads the source twice to detect changes during copying; copies to
a temporary file; verifies the destination hash; and atomically replaces the
published executable. Startup discovery also ignores invalid published EXEs.

Result:

Automated validation passed for the clean 1177 artifact and the server startup
probe passed. Human dev-loop restart/relaunch review remains outstanding.

### Corrected Code

File:
`devscripts/dev-loop.py`, `_copy_verified_executable`

```python
_copy_verified_executable(source_exe, destination / "mimita.exe")
```

### Fix

The dev loop no longer treats file existence and nonzero size as proof that a
published executable is runnable. It rejects unstable or zero-filled PE
artifacts before they can be launched or selected as the latest build.

### Proof

Human review:

Not yet performed. The fix still needs one normal developer-loop launch and
client join observed by a human.

Automated proof:

- `python -m py_compile devscripts/dev-loop.py` passed.
- Broken 1176 rejected; clean 1177 accepted.
- `python devscripts/dev-loop.py --help` passed.
- Published 1177 headless server startup passed through room registration and
  room-file creation, then shut down cleanly at the bounded test timeout.

### Solution

Not yet human-confirmed. Current status remains `ATTEMPTED FIX (1)`.
