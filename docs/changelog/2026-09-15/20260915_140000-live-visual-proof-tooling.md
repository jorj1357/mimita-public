# Live visual proof tooling for the hot pose -> render.mesh chain

Date: 2026-09-15 14:00 EST (UTC 2026-09-15T18:00:00Z)
Branch: `8292026stash` (worktree, uncommitted)
Result: source complete; LIVE VISUAL PROOF NOT PERFORMED (agent has no screen)

## Why this is not claimed as proven

This agent cannot run or observe a visible client. Per the project rule, visual
proof is only claimed when directly observed. So:

- LIVE VISUAL PROVEN: no
- LIVE HOT-POSE EDIT PROVEN: no
- HUMAN VERIFICATION NEEDED: yes (steps below)

## What was added

1. Temporary extreme debug pose (`modules/presentation/pose-generation.cpp`):
   `posedebug 1|0` command sets `g_debugExtreme`; when on, every animated entity
   gets left arm +90°, right arm -90°, torso tilt 0.6 rad, head yaw 0.8 rad.
   Default off. This is intentionally obvious, not pretty.
2. `hotactor` command (`modules/presentation/debug-presentation.cpp`): spawns a
   typeless local entity running the FULL generic chain - Transform + Velocity
   (moving, so the move clip is selected) + `AnimationState` + `PresentationState`
   (`mesh.actor`). This lets the generic actor path be checked without a remote
   NPC; it does not replace the real remote-NPC proof.

## Human verification steps (needed)

1. Build is already green; start the game (server + a client) so a remote NPC
   exists on screen.
2. In the client terminal run `hotactor`. Confirm the actor entity appears and
   the console logs `[HOT ACTOR] entity=...`.
3. Run `posedebug 1`. Confirm the actor's (and the remote NPC's) arms/torso/head
   visibly change to the extreme pose.
4. Confirm there is exactly one body per entity (no duplicate / no typed second
   copy) and the typed remote-NPC renderer does not reappear.
5. Run `posedebug 0` to return to normal.
6. Live edit: change the extreme angles in `pose-generation.cpp` (for example
   `1.5708f` -> `0.5f`), save, and confirm a new generation activates and the
   visible pose changes with the same PID/session/EntityId. Introduce a syntax
   error and confirm the previous pose stays (last-good), then fix it.
7. Matrix-order check: at zero pose the actor should be in the expected rest
   pose. If parts orbit/explode, the composition `entity * bone * bind` needs
   revisiting (no NPC-specific offsets).

## Evidence (automated only)

- `python build_agent.py` -> `Status: SUCCESS`.
- `--hot-combat-selftest` -> PASS (real actor GLB parts; EntityId ->
  SkeletonInstances consumption; static fallback).
- Full suite (9 selftests) -> PASS.

## Classification

- SELFTEST PROVEN: part-aware real actor GLB; EntityId skeleton consumption;
  static fallback.
- COMPILED INTEGRATION: `posedebug`; `hotactor`.
- LIVE VISUAL PROVEN: no.
- LIVE HOT-POSE EDIT PROVEN: no.
- HUMAN VERIFICATION NEEDED: steps above.

## Files changed

`src/hot-reload/modules/presentation/pose-generation.cpp`,
`src/hot-reload/modules/presentation/debug-presentation.cpp`,
`docs/architecture/live-development/hot-cold-audit.md`,
`docs/architecture/live-development/hot-kernel-next-steps.md`.

## Next (only after visual proof)

Add jump/attack pose policy using generic movement/action state, then a minimal
blend model, then generation-aware skeleton/clip resources.
