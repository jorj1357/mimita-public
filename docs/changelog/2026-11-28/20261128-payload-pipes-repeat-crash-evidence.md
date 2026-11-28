# Repeated Payload on Mimita Pipes crash investigation

Date: 2026-11-28
Issue: issue-c9b65fb9948e

## Direct evidence

- Current process discovery (`evidence-watch.py list --json`) returned no running MiMITA processes.
- Run `20261128_061051`, client-01 PID 23916, used EXE build `2026-11-27T17:45:19.700118Z:758a748d853e7b8b5bed98c614f8b243c0c9e5b7e8b9d9455fd04d6d9b096fdc` and game DLL generation 808.
- Its journal logs `network.mode.active`, mode `payload`, map `mimita-pipes`, at `2026-11-28T06:26:52.009Z`, with complete authoritative snapshots continuing through `06:27:00.733Z`.
- Crash report `.dev/crashes/20261128_061051/crash-2026-11-28_01-26-56-23916.txt` reports `ACCESS_VIOLATION (0xc0000005)`, read at `0xffffffffffffffff`. Stack includes `RtlUTF8StringToUnicodeString`, `MultiByteToWideChar`, world rendering API frames, and `game-g808.dll+0xf97c0`.
- `addr2line` against the exact generation-808 DLL maps the game frame to a `std::_Function_handler<...>::_M_invoke` callback trampoline (`bits/std_function.h:297`), not the callback's underlying application function. Exact bad string and source owner remain unresolved.
- The run's dev-loop journal reports client PID 23916 exited at `2026-11-28T06:27:41.271Z` with code `3221225477` (`0xc0000005`). This confirms a client access violation during the active Payload/Mimita Pipes session; it is not a join rejection because the client was already active and applying snapshots.
- Same `game DLL +0xf97c0` signature is present in older crash reports across generations 574, 755, 805, and 808, including two clients in run `20261128_054617_01`. This is a recurring crash signature.
- Separate run `20261128_062046` captured a 56.9-second snapshot gap and client reconnect/recovery on Payload/Mimita Pipes. This is a distinct liveness event and is not asserted to be the same access violation.

## Assessment

The crash is confirmed and occurs on the client after Payload has become active. The stack places the fault during UTF-8 to wide-character conversion in a world-rendering callback path, reading an invalid `0xffffffffffffffff` address. That suggests invalid text/string data or lifetime/length corruption, but the callback trampoline prevents naming the actual caller; this remains an inference rather than a proven root cause. The exact build/run source validation was not pinned and reports stale daemon/source changes during build, so treat generation 808 as the artifact that crashed, not as a clean source baseline.

The repeated occurrence and concrete evidence were appended to issue-c9b65fb9948e with the next step: resolve the callback target and conversion arguments from a dump with symbols or targeted diagnostics, then inspect the string producer at the renderer owner. No source changes, build, runtime control, or fix claim were made. The live processes were left untouched; process discovery found none at review time.
