# Candidate DLL build authority audit

## Findings

- `config/modules/modules.json` is absent. `ModuleHost::startup()` calls
  `loadLocalModules()` directly, and local mode makes `poll()` return before
  watching module inputs. The active NPC behavior implementation therefore
  stays folded into the game DLL.
- The dormant `ModuleHost::requestBuild()` path tries to launch
  `devscripts/module-build-worker.py`, but that file is absent. Restoring this
  process launch would violate the current rule that root dev-loop owns every
  build.
- The root dev-loop request list supports `build`, `game-dll`, `batch-build`,
  `agent-build`, and `cold-link-trial`; it has no module-DLL request. Existing
  `build/modules/npc-behavior` data is historical output, not a current route:
  generation 2 was written 2026-11-11T15:47:43Z, with a 492,699-byte DLL.
  Its result reports 135 dependencies, one translation unit compiled and one
  reused. This does not establish current build latency or live activation.
- The generic ModuleV1 candidate loader already has a safe activation boundary
  with bounded state export/import, peer agreement, and rollback. The NPC
  behavior module currently exports zero state bytes, while NPC instances live
  in the resident system; arbitrary object-layout migration is not solved.

## Recommended experiment

Add a root-dev-loop-owned request and worker operation for one bounded
`npc-behavior` candidate. It should consume the existing module manifest and
object cache, produce a versioned/hash-identified DLL, publish an immutable
candidate artifact, and report every stage. The live ModuleHost should request
the candidate through the root control channel, activate it at the fixed-tick
safe boundary, and retain the current whole-game MinGW DLL path as fallback.
Keep the module candidate limited to stable ABI inputs; avoid linking the 644
whole-game objects. First measure a one-function edit under the target runtime,
then test a 15–20-file change contained within that domain. Expand into vehicle
or core modules only after same-PID activation, behavior change, state
continuity, rollback, and latency are proven.

## Scope and evidence boundary

This audit was read-only except for the linked issue discussion and this
changelog. No worker, compiler, build, process control, or runtime command was
started. Findings and the owner coordination request are in
`issue-136d4553ff92`. This identifies an architectural prerequisite; it does
not show a candidate DLL currently rebuilds or activates live.
