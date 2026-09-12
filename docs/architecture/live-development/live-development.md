// 09 12 2026
/* purpose
* Define the live-development invariant and the cold/live build split.
* Make the running-executable guarantee explicit for humans and agents.
* this file DOES NOT define gameplay behavior or the hot module ABI
* this file DOES NOT replace the live-code feature specification
* this file DOES NOT permit killing, closing, or relinking a running game
*/

# Live development invariant

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

## Tooling

- `devscripts/live-build.py`: the documented live entry (never writes the exe).
- `build_game_dll.py`: the compiler the runtime worker also uses.
- `build_agent.py` / `build.py`: cold builds only.
- `devscripts/test-live-build-invariant.py`: asserts a live build leaves
  `mimita.exe` unchanged and the cold guard refuses while it runs.
