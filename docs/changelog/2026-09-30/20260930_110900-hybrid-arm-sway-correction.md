# Hybrid arm sway correction

Time: 2026-09-30T11:09:00-04:00 (America/New_York)
Branch: `afad20a-rebuild`

## Result

Implemented the requested hybrid-mode arm behavior. Arms now preserve the
animation/aim pose relative to the torso after joints and collision run, while
`physical.hybrid.arms_follow_force` controls how quickly movement-induced sway
is corrected. Higher values reduce sway; `0` leaves the arm fully physical.

## Exact changes

- `src/ragdoll/ragdoll-mode.cpp`
  - Removed the arm-only multiplier from the pre-physics hybrid rate in
    `RagdollModeSystem::applyHybridSprings`. The old value was multiplied into
    an already saturated rate before physics.
  - Added `RagdollModeSystem::stabilizeHybridArms` at lines 375-402. It builds
    an arm target relative to the current torso and applies an exponential
    correction after integration, joints, self-collision, and world collision.
  - Calls the new correction from `RagdollModeSystem::updateAim` at lines
    551-552.
- `src/ragdoll/ragdoll-mode.h`
  - Added the private `stabilizeHybridArms` declaration.
- `src/ragdoll/physical-aim.h`
  - Updated the `hybridArmsFollowForce` contract to describe its new
    post-physics sway-rate meaning.
- `config/ragdoll.json`
  - Updated the `arms_follow_force` comments to match the new contract.
  - The active values changed during the session from `follow_force=100.0`
    and `base_rate=50.0` to `follow_force=10.0` and `base_rate=10.0`; this was
    preserved as current worktree state and was not intentionally changed by
    this fix.

## Documents and focused review

Read and followed:

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/live-development/ragdoll-live-network.md`
- `docs/architecture/live-development/live-development.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/README.md`

Finding: the old implementation used an absolute world-space arm target and
applied `base_rate * follow_force * arms_follow_force` before physics. With the
old observed values this reached 250,000/s, so changing the arm value was
effectively saturated and could not control post-joint sway.

## Validation

- `git diff --check`: passed.
- First `python build_agent.py`: returned `SUCCESS` but `Nothing changed`; it
  skipped the changed ragdoll object and was not accepted as compile proof.
- Removed only the exact stale build artifacts
  `build/obj-debug/ragdoll_ragdoll-mode.o` and `.d`, then rebuilt.
- Second `python build_agent.py`: `BUILD SUCCESS`, `Compiled: 1`,
  `Skipped: 496`, return code `0`; linked `C:\mimita-v9\mimita.exe` at
  2026-09-30 11:07:52.
- The attempted `--ragdoll-slice-selftest` invocation launched the full client
  instead of a headless self-test, so no self-test pass is claimed. That test
  client was stopped; the two pre-existing `.dev\builds\0515\mimita.exe`
  processes were left running.

## Human review still required

Launch the newly built executable, use hybrid aimbody mode, and move left and
right. Confirm that the arms still move naturally but remain closer to the
centered torso-relative angle, and tune `arms_follow_force` (currently `50.0`).
Build success is not visual acceptance.

Pre-existing unrelated work was preserved: `config/accounts/default.json`,
`config/analytics.json`, `docs/specs/20260930plan.md`, and the pre-existing
`docs/changelog/2026-09-30/20260930_121500-destructible-crate-audit.md`.
