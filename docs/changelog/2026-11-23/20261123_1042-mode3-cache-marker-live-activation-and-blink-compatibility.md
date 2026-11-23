# Mode 3 multi-file DLL latency and Blink compatibility

## Outcome

Continued the active <=10-second edit-to-live goal. The live 15-file DLL
measurement completed and generation 555 activated in the existing 67-NPC
Juggernaut Horde client/server pair. It did not meet the end-to-end target.
No compiler, runtime, Mode 3, or game architecture changes were made during
this documentation pass.

## Matched build and live evidence

- Run `20261123_101135`: `jugger_horde`, `dust2cyberiav4`, one client, 67 live
  NPC actor IDs; server PID11256 and client PID22492. The root dev-loop remains
  alive and reports Mode 3 `batch`, no pending batch, latest DLL current.
- The 15 changed `.cpp` inputs were comment-only markers. They saved at
  10:17:44.522–10:17:44.694Z. Root dev-loop request
  `2026-11-23T10:22:00.532Z-271cb1ee` began after intentional measurement and
  preparation wait. Generation555 activated on server at10:22:18.639Z and
  client at10:22:18.776Z in rotated runtime journals.
- Last-save-to-client-activation was274.082s, which includes the planned
  pre-request hold. Request-to-client-activation was18.244s. These measure
  different intervals; neither is <=10s.
- Attempt `game-dll-20261123T102200810724Z-dcef23`: request-to-worker4.746s;
  total17.612s; 16 compiled (15 changed `.cpp` files plus generated identity
  provider), 627 skipped; 10 jobs. Changed-source compile wall7.255s, LLD
  link4.103s over642 object inputs /483,418,586 bytes, publish0.254s.
- Safe source-only object marker succeeded: scan37.589ms, marker validation
  20.919ms, 627 reused, 0 dependency tokens checked. The old 84k-token cache
  prepass is not present in this successful source-only path.
- The live runtime activation proves a DLL generation swap and continued
  simulation. Since only comments changed, this is not proof of altered
  gameplay behavior.
- Detailed timings: `build/game/last-attempt.json`,
  `build/game/progress.json`, and
  `.dev/build-timings/game-dll-20261123T102200810724Z-dcef23.json{,l}`.
  Resource sample: `.dev/perf-captures/mode3-live-15file-20261123-03.csv`.
  Compiler/linker PID attribution and top competing processes remain unknown.

## Stage interpretation correction

`game_build_worker.py` labels the 6.865-second batch that actually compiles
the changed source files as `changed_source_preflight`. The later `compile`
phase shown in phase durations is only about0.400s for the remaining identity
provider; aggregate `worker_metrics.compile_seconds` correctly includes both
and reports7.255s. The GUI's phase label can therefore look like compilation
has not started, even though source compilation is underway. The issue record
now recommends surfacing this as changed-source compilation and not counting
it twice.

## Blink compatibility finding

Upstream Blink README requires a matching Microsoft PDB and describes an
MSVC `/ZI` workflow. In `external/blink/source/blink.cpp`, the target image is
initialized from `GetModuleHandle(nullptr)`, and debug data is read for that
image and its imported modules. MiMITA's `ModuleHost` later loads game DLL
generations via `LoadLibraryW` at `src/hot-reload/module-host.cpp:2196`.
Therefore the current dynamically loaded game DLL is not automatically a
Blink target merely by producing MSVC/PDB artifacts. A Blink prototype would
need explicit selection/attachment to that loaded module, valid PDB identity,
and a controlled single-function experiment. Keep the current MinGW DLL path
as fallback; do not infer arbitrary object-layout migration from Blink.

CR-style DLL reload does not remove changed-source compilation or monolithic
linking, while Flecs can help make game state data-driven but does not compile
or patch C++ functions. They do not address the measured link floor by
themselves.

## Next discriminating work

1. Fix phase presentation/accounting so changed-source compilation is named
   and timed as such; preserve one total from accepted request to activation.
2. For the <=10s 15-file target, investigate a live patch or modular candidate
   path that avoids the measured full 4.103s link and 4.746s pre-worker path;
   cache scanning is already negligible for safe `.cpp`-only edits.
3. Run a real behavioral NPC edit under the same live workload, recording
   save time, request time, build stages, activation, and a journal event that
   demonstrates the intended behavior. Measure host and process CPU/RAM with
   compiler/linker IDs and top competitors in the build interval.
4. Keep Blink as an isolated toolchain experiment, not a current-path
   replacement, until dynamic game-DLL PDB compatibility and latency are
   measured.

## Files and records

- `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md`:
  appended exact timing, compile-phase attribution correction, Blink dynamic
  module constraint, and next discriminator.
- This changelog is a new file; no prior text existed.
- No compiler inputs were edited; no build was requested by this pass.
