# Candidate NPC behavior DLL build path review

## Goal

Continue the Mode 3 latency objective: a coordinated 15–20 C++ source edit
should become active in a running MiMITA client/server pair within 10 seconds,
with continuous build resource measurements and generation proof.

## Current state checked

- Root dev-loop had no active build and no pending batch at the time of the
  status check. It reported server PID 25840 and client PID 23360 alive and
  build-matched in run `20261124_150432`.
- The available runtime evidence is not an acceptance workload: the server
  journal identifies Bloodtest with two NPCs and zero players; client telemetry
  is stale; the host game DLL reports generation 594 while the NPC behavior
  module reports generation 593. No new run, build, or process control was
  performed.
- The CPU agent's completed logger-category A/B found no CPU or tick-stall win:
  host use stayed near 99%, tracked MiMITA whole-host CPU shifted from 39.24% to
  38.67%, and each condition still had a root-server tick stall over 100 ms.
  See the CPU issue and
  [the logger A/B changelog](20261124_1630-collision-logger-config-ab-result.md).

## Candidate build-path evidence

- `devscripts/game_build_worker.py` already accepts an explicit `--sources-file`
  and maps `src/npc/npc-behavior-module.cpp` to its module export symbol. The
  NPC behavior source exposes a versioned module API, so a small candidate
  source set is technically plausible.
- The root dev-loop has no candidate `module-dll` request target. The module
  catalog at `config/modules/modules.json` and the legacy
  `devscripts/module-build-worker.py` are absent.
- The custom-source worker path still compiles `src/glad.c` and links the
  identity/GLAD objects, Manifold archive, GLFW search path, and global game
  link libraries. This is confirmed source behavior; the time those inputs add
  to a small candidate link has not been measured.
- The resident ModuleHost activation and generation-refresh path still needs
  coordination with its active owner. The main game DLL remains the fallback;
  a candidate must publish to a separate immutable output and must never
  overwrite or masquerade as `build/game/current.json`.

## Next proof steps

1. Coordinate ownership of the ModuleHost activation/rebind paths.
2. Add a root-dev-loop-only candidate target with a separate artifact pointer,
   attempt timings, and the existing host/process resource sampler.
3. Measure the small NPC behavior candidate before removing any default link
   inputs; preserve the monolithic game DLL path unchanged.
4. Prove same-PID activation, module/host generation agreement, visible NPC
   behavior change, rollback, and then test a 15–20-source candidate batch in a
   fresh connected Juggernaut Horde pair under measured resource pressure.

No source, compiler, runtime, or build changes were made in this pass.
