# Ragdoll Phase B-E: rigid links, per-axis limits, controlled extension, third person

- Task ID: ragdoll-stiffness-limits-camera
- Summary: Make ragdoll links and grabs effectively rigid with a 3D velocity
  solve, add per-axis rotation limits, turn arm extension into an internal
  angular motor that cannot pull the player, and add a third-person allowed
  setting.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T16:38:52Z` (2026-09-10 12:38:52 EDT)
- Branch: `8292026stash`
- Base commit: working tree; no commit created
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the FFA/kill-event/countdown network and
  gamemode changes already present in the working tree, and the other files
  under `docs/changelog/2026-09-10/`. Not claimed here.
- Prior ragdoll work in this session is recorded in
  `20260910_155338-ragdoll-mode-rigid-body-core.md` and
  `20260910_163323-ragdoll-collision-solid.md`.

## Requested behavior

- Shoulder/attachment links must hold to roughly 0.01 m under strong force;
  grab must hold solidly and gravity must not accumulate without bound while
  hanging.
- Add tunable per-axis rotation limits in `config/ragdoll.json`.
- Arm extension must hold the arm out along camera-forward without pulling the
  player forward.
- Add a ragdoll setting that allows third person (respect `camera.thirdPerson`);
  when disallowed, force first person.

## Specification alignment

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`: RAG-016/017/018 (arm
  extension follows camera-forward, obeys collisions, pushes the body through
  reaction), RAG-025 (grabs effectively unbreakable), RAG-035/036 (head aim
  physically influences the body), RAG-046 (config-driven tuning).
- `docs/specs/moving-physical-objects/moving-physical-objects.md` §13
  (force/torque/angular momentum) and §33-36 (constraints solved as physical
  primitives).
- `docs/skills/spec-behavior-review-v1.md`: PASS_WITH_HUMAN_REVIEW.

## Exact implementation changes

### Phase B - rigid links and grabs
- `src/physics/physical-body.h` / `.cpp`:
  - Added `effectiveInverseMassMatrix(body, r)` and rewrote
    `solvePointJointVelocity` to a full 3D point-joint velocity solve using
    `invMass` and isotropic `invInertia` (cancels relative velocity in all three
    axes, not just along the error direction).
  - Added `solvePointToWorldVelocity(body, anchor)`: cancels the anchor velocity
    of a grabbed point so gravity cannot keep accumulating while hanging.
  - Added public `depenetrateWorld(body, world, passes)` for a final
    position-only solidity pass.
- `src/ragdoll/ragdoll-mode.cpp`:
  - `solveGrabs` now solves velocity first, then position, each iteration.
  - Tick order: integrate -> joints -> grabs -> world collision -> self
    collision -> joints (iterations/2) -> grabs (iterations/2) -> final
    `depenetrateWorld` -> sync.
  - Joint position projection `beta` raised 0.6 -> 0.9; default
    `solver_iterations` raised 8 -> 24. The near-0.01 m link stiffness comes
    from convergence and inverse-mass distribution, not a hard clamp.

### Phase C - per-axis rotation limits
- `RagdollModeAttachmentConfig`: added `hasRotationLimits`, `rotMinDeg`,
  `rotMaxDeg`.
- `ragdoll-mode-config.cpp`: parses `"rotation_limit_deg": { "x": [min,max],
  "y": [...], "z": [...] }` per attachment.
- `RagdollModePart`: stores the bind relative rotation, set in `initParts`.
- `RagdollModeSystem::solveRotationLimits`: converts the parent-local relative
  rotation to a rotation vector, clamps each axis, and applies the rejected
  excess as an inverse-inertia-weighted angular correction to child and parent
  (reaction included). Runs each position iteration alongside the cone limit.
- `config/ragdoll.json`: added arm limits `x/y [-80,80]`, `z [-60,60]`; leg
  limits `x [-70,70]`, `y/z [-40,40]`. Head intentionally unlimited so camera
  aim can still turn.

### Phase D - extension is an internal motor
- `processExtend` no longer applies a linear impulse at the hand. It steers the
  arm's angular velocity so the hand points along `camera.front`, preserving
  spin about the steering axis. Changing only angular velocity adds no net
  linear momentum, so holding LMB/RMB can no longer pull the player forward;
  the shoulder joint transmits the reaction to the torso.
- New `config/ragdoll.json` `arms.extend_strength` and `arms.extend_max_speed`.

### Phase E - third person
- `RagdollModeConfigData`: added `thirdPersonAllowed` (default true), parsed
  from `camera.third_person_allowed` (or top-level `third_person_allowed`).
- `src/engine/engine-tick-camera.cpp`: while ragdolled, if
  `third_person_allowed` is false the camera is forced to first person
  (`camera.thirdPerson = false`); otherwise the existing third-person follow is
  preserved when `camera.thirdPerson` is set. First person keeps the smoothed
  head camera.
- `config/ragdoll.json`: `camera.third_person_allowed: true`.

## Diagnostics

- No new log sites. Existing `StructuredCategory::Ragdoll` tick events remain
  the low-volume owner.

## Validation

- Build: `python build_agent.py` -> `Status: SUCCESS`, return code 0, duration
  13.62s; `mimita.exe` relinked 2026-09-10 12:38:40. Compiled `physical-body.cpp`,
  `ragdoll-mode.cpp`, `ragdoll-mode-config.cpp`, `ragdoll-commands.cpp`,
  `simulate-tick.cpp`, `engine-tick-camera.cpp`, `engine-tick-render.cpp`,
  `engine-tick-setup.cpp`, `server-projectiles.cpp`. No errors or warnings.
- Config: `config/ragdoll.json` parses; verified `solver_iterations`, `arms`,
  `camera.third_person_allowed`, and arm rotation limits.
- Runtime: not performed this session. Link stiffness, grab strength, no-runaway
  gravity, per-axis limits, extension pull, and third-person behavior all still
  need human play testing.

## Regression review

- No append-only entry added; continues uncommitted ragdoll feature work.

## Human acceptance

- Gameplay review required: grab a wall with one arm and hang; the arm should
  barely move, speed should not build up over time, and the shoulder should stay
  attached.
- Gameplay review required: hold LMB/RMB and confirm the arm points forward
  without pulling the player.
- Gameplay review required: tune `rotation_limit_deg` in `config/ragdoll.json`.
- Visual review required: with `third_person_allowed: true`, entering ragdoll
  while in third person stays in third person; setting false forces first person.

## Phase F (deferred to human testing)

- Torso tipping is expected to emerge now that contacts and links are rigid;
  no specific torso code was added. If the torso still reads as world-Z locked,
  the next step is a torso inertia scale in `config/ragdoll.json` and a check
  that nothing re-locks its orientation.
