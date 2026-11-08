// 09 12 2026
/* purpose
* Define the live-development invariant and the cold/live build split.
* Make the running-executable guarantee explicit for humans and agents.
* this file DOES NOT define gameplay behavior or the hot module ABI
* this file DOES NOT replace the live-code feature specification
* this file DOES NOT permit killing, closing, or relinking a running game
*/

## Current policy (2026-10-01)

The no-cold-build rule is disabled as of 2026-10-01. Cold builds are fine for
now. The invariant and cold/live restrictions below are retained as historical
context and do not block the current workflow.

## Agent build authority (2026-11-08)

Agents edit and observe; they never build. Every agent uses an isolated mode 2
lane (`devscripts/agent-lane.py create/ensure/status/run`) with its own
worktree, artifacts, module builds, run IDs, and journals. The lane dev-loop
auto-detects saved edits; the running game's `ModuleHost` builds module
generations. A machine-wide scheduler allows one EXE build and two module
builds across all lanes. Agents must not run `build.py`,
`module-build-worker.py`, a build request, or a compiler directly. Only a human
may use `MIMITA_BUILD_OVERRIDE=1`. See
[`docs/specs/dev-loop/agent-lanes-build-authority.md`](../../specs/dev-loop/agent-lanes-build-authority.md).

# Live development invariant

## Current capability and mode 3 direction

The desired module and source-save contract is defined in
[`docs/specs/hotreload/hotreload.md`](../../specs/hotreload/hotreload.md).
This architecture document records the live/cold boundary and must be read
with that source-checked specification.

The mode 1/2 build workflow and C++ live reload are different things. The
dev-loop watches compiler inputs, debounces saved changes, incrementally
compiles and links, then publishes a new executable artifact. It preserves an
already-running game; publication alone does not replace that game's code.

The current hot DLL is a narrow exception, not a universal C++ reload path.
`build_game_dll.py` compiles the focused source set in the active checkout, and
`HotReloadSystem` watches those specific sources, rebuilds the DLL, validates
its `GameAPI`, and loads a unique copy. Its compile step is currently invoked
synchronously from the game process. It does not cover arbitrary `.cpp` or
header edits.

Mode 3 is the future goal: a file save triggers a debounced background
dependency-aware compile for that agent's lane, followed by module linking and
validated activation only when the affected code is safely replaceable. Edits
made during a compile are coalesced into a check of the latest saved source;
the same lane's incremental outputs are never built concurrently. Compile
errors are reported to that lane and the last valid running generation stays
active. Until a source is migrated behind a stable replaceable interface,
automatic checking does not make it live: a full executable build is still
required to run that edit.

The long-term goal is to make every C++ edit eligible for this flow. That
requires expanding the replaceable boundary, stable ABI and state ownership,
dependency-aware module linking, compatibility validation, and rollback. Even
when most development no longer waits for a complete executable build, the
reviewed combined source still requires final integration validation and a
stable release build. Mode 3 is not implemented as an all-C++ service today.

> Historical policy below; disabled as of 2026-10-01.

## The invariant

> If `MiMITA.exe` is already running, it must remain running.

No coding, editing, testing, or hot-reload operation may require closing,
restarting, relinking, replacing, or unlocking the running executable.

A correct live result reports:

> "The full executable was never linked because live development did not require it."

This is not partial success:

> "Everything compiles except the final executable link because MiMITA is open."

If the linker tries to replace `MiMITA.exe`, the wrong build path was used.

## Cold build versus live build

```text
COLD BUILD
- creates or replaces MiMITA.exe
- only when the executable is intentionally not running
- entry: python build_agent.py  (or build.py)
- refuses to link while mimita.exe is running
- MIMITA_FORCE_COLD=1 is the nuclear last-resort override, not a normal step

LIVE BUILD
- never writes MiMITA.exe
- compiles only replaceable code
- produces a new immutable generation
- entry: python devscripts/live-build.py
- runs indefinitely while MiMITA.exe stays open
```

The normal loop never links the executable after it has started:

```text
edit hot source -> save -> detect affected module -> compile candidate only
-> new generation -> load -> validate/self-test
-> valid: activate at a safe boundary / invalid: keep old generation
-> continue the same running session
```

## Generation model

- Generation files are immutable: `build/hotreload/mimita-live-g000001.dll`,
  `mimita-live-g000002.dll`, and so on. Never overwrite the active file.
- The runtime copies a candidate to a unique temp before loading it, validates
  the descriptor/ABI and a deterministic self-test, swaps the function table at
  the top of the fixed tick, marks the new generation ACTIVE, and retires the
  previous generation only after no thread can still execute it.
- The previous generation's file stays on disk for instant rollback.

## Persistent state

The stable runtime owns persistent state: entity ids, components, player/NPC
state, projectiles, world state, network connections, match state, scores, and
relevant RNG state. Reloading code changes the rules operating on that state; it
does not recreate the world.

```text
THE LAWS CHANGED
THE UNIVERSE DID NOT RESTART
```

## HOT_RELOAD_BOUNDARY_VIOLATION

A development result returned when an edit affects something that cannot be
activated without relinking `MiMITA.exe`.

The response is never "close MiMITA". It is:

```text
identify why this implementation is still cold
-> move appropriate gameplay behavior across the hot boundary
   or
-> mark it as a genuine cold/runtime-kernel change
```

The runtime records a `hot_reload_boundary_violation` journal event when a
curated cold-kernel source changes. `hotreload classify` reports whether
changed files are HOT, COLD, or UNKNOWN.

Prefer moving actively developed gameplay policy behind the hot ABI. The cold
kernel should contain mechanisms, not frequently edited gameplay policy.

## Cold restart severity

`cold_restart_pending` is a severe live-development violation, not a normal
progress reminder. It means the current edit cannot enter the running process
and would require losing the live session. The event must identify the exact
cold source and why its owner remains outside the hot boundary. Do not kill,
close, or silently restart `MiMITA.exe` to clear it. The preferred next step is
to move the editable behavior behind the stable hot ABI; only a genuine kernel,
ABI, or runtime-mechanism change may remain cold until an intentional,
no-process installation window.

The detailed observability and AI-evidence direction is recorded in
`docs/gold/2026-09-12-live-jsonl-ai-observability.md`.

## Tooling

- `devscripts/live-build.py`: the documented live entry (never writes the exe).
- `build_game_dll.py`: the compiler the runtime worker also uses.
- `build_agent.py` / `build.py`: cold builds are currently allowed; the old
  cold-only restriction was disabled as of 2026-10-01.
- `devscripts/test-live-build-invariant.py`: asserts a live build leaves
  `mimita.exe` unchanged and the cold guard refuses while it runs.
