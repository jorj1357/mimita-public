# RMB physical arm aim — full-pitch validation

- EST timestamp: 2026-10-02 09:38:41
- Branch: `afad20a-rebuild`
- Result: `PASS_WITH_HUMAN_REVIEW`
- Scope: validate the existing RMB arm aim-strength correction requested for
  full camera up/down movement.

## Source finding

The current committed implementation in
`src/ragdoll/ragdoll-mode.cpp`, `applyRightArmPointMotor`, already keeps the
camera-ray target independent of `aim_strength`. It applies strength to the
physical motor's torque gain and angular damping instead of multiplying the
controller blend weight. The right arm also skips its normal swing and
rotation-limit passes while `rightArmPointingBlend > 1e-3`, so the normal arm
range is not an intentional pitch clamp during RMB aiming.

The current user-tuned `config/aimbody.json` value was preserved:
`right_arm_pointing.aim_strength` is `5.0`; the parser default remains `10.0`
when the field is absent.

## Validation

- `python build_agent.py`: `Status: SUCCESS`, return code `0`; the build
  reported no source changes and reused the current executable state.
- `git status --short`: clean before this changelog was added.
- `git diff --check`: passed in the prior source validation for this code path.
- `tests/physical-aim-torque-test.cpp`: not run because no `g++`, `clang++`, or
  `cl` compiler is available in this environment; no test executable was
  created.

## Still required

Human gameplay review is still required: hold RMB and sweep from full upward
to full downward camera pitch at several `aim_strength` values, while moving,
to verify the arm reaches the complete camera range without collision/joint
contact producing a physical stop. If it still stops only when the arm hits
the body/world, that is a physical-contact issue rather than a JSON aim-range
limit.
