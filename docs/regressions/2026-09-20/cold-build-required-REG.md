# Cold Build Required

Time created: 2026-09-20T15:11:13Z
Time last updated: 2026-09-20T15:29:00Z

Status: COLD-BUILD DEBT

This persistent record tracks intentional cold builds required because a
changed owner cannot yet cross the live-reload boundary. A cold build is not
automatically proof of a user-visible behavior regression.

Related specification:
`docs/features/live-code-development/live-code-development.md`

Related workflow:
`docs/operations/build-and-exe/build-and-exe.md`

Related changelog:
`docs/changelog/2026-09-20/20260920_152300-live-collaboration-foundation-and-commands.md`

---

## Cold-build occurrence 1

Time:
`2026-09-20T15:13:04Z`

Related changelog:
`docs/changelog/2026-09-20/20260920_152300-live-collaboration-foundation-and-commands.md`

### Reason the cold build was required

The v2.1.0 live-collaboration foundation added revision storage, server
arbitration, packet definitions, terminal commands, and JSONL probe support.
The requested result was a complete executable containing those new owners so
the code could be compiled and the existing live-code self-test could run.

### Exact cold owner or boundary

The changed owners were compiled into the EXE by the canonical build:

```text
src/network/server-live-collaboration.cpp
src/network/live-collaboration-client.cpp
src/network/multiplayer-packets.cpp
src/network/multiplayer-tick.cpp
src/network/server-packets.cpp
src/network/packets.h
src/terminal/live-code-commands.cpp
src/debug/structured-log.cpp
src/debug/structured-log.h
```

The hot manifest currently builds replaceable modules from
`src/hot-reload/modules/` and `src/hot-reload/packages/`. It does not make the
network transport, packet dispatch, terminal registration, or logger mechanism
replaceable. Therefore `python devscripts/live-build.py` could not install all
of this change into the running EXE.

### Why this is still cold

The EXE owns the socket transport, packet decoding/dispatch, terminal command
registry, logger file handle, and the stable hot-reload bridge. These are
mechanisms and are currently initialized by the EXE. A live DLL can emit a
generic event only where an existing bridge already exposes that event; it
cannot add a new packet receive case or replace the terminal registry today.

### Result obtained from the cold build

```text
Executable: C:\mimita-priv-v8\mimita-20260920T111304.exe
Build: SUCCESS
Self-test: mimita-20260920T111304.exe --live-code-selftest -> PASS
```

The build compiled 315 files and skipped 355 unchanged files. No running
MiMITA process was present, so no active session was closed or replaced.

### Smallest hot-boundary change needed

Keep the following cold mechanisms:

- socket I/O and packet framing;
- packet byte serialization/deserialization;
- the JSONL file writer and logger thread safety;
- terminal input parsing and command registration.

Move the editable policy behind existing generic seams:

1. Add a generic `live.operation` event/capability whose POD payload contains
   the resource, revision, operation, and content hash.
2. Let hot code validate and decide revision policy through that event.
3. Keep the EXE as a thin packet-to-event and event-to-packet bridge.
4. Let hot code request queue inspection, rollback, and activation through the
   same generic operation surface.
5. Keep `LiveProbe` calls in hot C++ and keep probe configuration in the
   existing hot-reloaded logger configuration path.

After that migration, changing revision rules, queue policy, rollback policy,
or which values a hot gameplay function exposes should require only a DLL
generation. The EXE should not gain resource-specific packet cases.

### How to avoid this cold build next time

Before editing, run:

```text
hotreload classify
```

If a required file is `COLD`, do not edit it as the first implementation step.
Instead:

1. Find an existing generic event, capability, command, resource provider, or
   operation queue.
2. Put the frequently edited rule in a hot module.
3. Keep only the stable mechanism in the EXE.
4. Add a POD payload and a generic bridge if the mechanism has no doorway.
5. Build with `python devscripts/live-build.py`.
6. Confirm the same EXE PID, world, entities, session, and old generation
   remain alive while the new generation activates.
7. Read `events.jsonl` for `compile_started`, `candidate_loaded`, validation,
   activation, generation, and result records.

Use a cold build only when the work changes a genuine kernel mechanism, ABI,
packet framing contract, OS resource, or initial installation boundary. Record
that exact reason here instead of treating the cold build as a normal fix.

### Migration/falsification step

Create a hot `live.operation` self-test that submits two revisions against the
same resource, proves stale-base draft preservation, activates one revision,
and rolls back without deleting the other. Then run a two-process test where a
client proposes the operation through the real packet bridge and both peers
report the same active revision in JSONL. If that passes, the revision-policy
files can be classified HOT; packet transport remains COLD mechanism only.

### Build result

`SUCCESS`

### Human review

The compile and existing live-code self-test passed. Full two-client revision
transport, simultaneous-edit preservation, packet activation, and live
rollback still require runtime/human acceptance.

---

## Cold-build occurrence 2

UTC time: 2026-09-23T03:23:49Z

Related changelog:
`docs/changelog/2026-09-23/20260923_032900-dash-down-dash-no-buffer-hot-edge.md`

### Why the cold build was required

The dash / down-dash press edge lived in the input layer (`src/input/*`,
`src/sim/simulate-tick.cpp`, `src/engine/engine-tick-net.cpp`), which is EXE
code. Removing the 150 ms press buffer and feeding the raw key-down state into
the movement intent changed those EXE files. A hot build alone could not apply
it.

### Exact cold source / boundary

- `src/input/input-frame.h` (added `dashHeld`, `downDashHeld`)
- `src/input/input-poll.cpp` (raw held + raw pressed, no buffer)
- `src/input/input-commands.cpp` (`isDashPressed`/`isDownDashPressed` no buffer)
- `src/sim/simulate-tick.cpp` (intent uses held fields)
- `src/engine/engine-tick-net.cpp` (network uses raw pressed)

### Result needed from the new executable

Rapid Q / Shift presses each produce a fresh edge with no 150 ms merge.

### Why it could not be applied through the live path

The input sampling and the intent write are in the EXE, not in the hot module.

### Smallest change that would make this hot

The hot movement already owns the edge. Only the raw key sampling is EXE-side.
The remaining cold surface is small and stable: the frame's held booleans and the
intent write. A future option is a generic `input.read` raw-state capability so
hot code can compute edges without any EXE edit; the frame fields themselves are
stable and should not need further edits.

### Build result

`SUCCESS` -> `mimita-20260922T232349.exe` (an earlier `mimita-20260922T231811.exe`
was the one-shot buffer variant).

### Human review

Pending. Rapid-tap behavior needs a human playtest.
