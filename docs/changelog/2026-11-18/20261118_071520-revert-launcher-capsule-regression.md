# Revert launcher capsule regression

- Task ID: session-20261118-071445
- Session ID: session-20261118-071445
- Owner: Codex
- Date: 2026-11-18 02:15 EST (2026-11-18T07:15:20Z)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 43b89938b8aa29e3bffab982a544972f8dad7968
- Status: PASS_WITH_HUMAN_REVIEW

## Request

Revert the rocket-launcher capsule change after it led to getting stuck inside walls, document the regression, make the weapon hitbox visible, and keep the actor/world slope snag separate.

## Changes

- `config/weaponcollisions.json`: restored the rocket launcher `source: "boxes"` proxy and prior half-size `[0.2, 0.2, 1.6]`; set its existing `visible` option to `true`. The rest of the config diff is only this visibility change. The live game screenshot shows the cyan wireframe box around the held launcher.
- `src/physics/movement/physics-collision-body.cpp`: removed the added server-side config poll and restored capsule sampling from 17 to 8; the file content now matches the base version.
- `docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md`: appended Occurrence 10 / Attempt 14. It records the user-reported launcher wall embedding after the capsule change, the revert, and the fact that the exact contact cause is not yet proven.
- `docs/regressions/collisions/README.md`: updated Entry 7 summary/status.
- `docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md`: recorded the user's clarification that ordinary actor/world movement feels fine while uphill slope snagging remains unresolved.

## Evidence and validation

- Inspected the focused diff and ran `git diff --check` on the changed config and regression records; no whitespace errors were reported.
- Computer-use screenshot of the live `mimita.exe` visibly showed the rocket launcher box hitbox wireframe in cyan.
- The screenshot confirms the configured visual is present; it does not prove wall collision behavior. No post-revert wall route was exercised.
- The root dev-loop reported game DLL generation 331 published, run `20261118_062057`, client PID 19804 and server PID 18992. This session did not request a build, rebuild the EXE, or restart the game.
- The earlier capsule source change was reverted before this session ended. No runtime evidence identifies its exact embedding mechanism.

## Regression status and remaining review

- Attempt 14 is reverted; box proxy restored; hitbox visibility enabled.
- The user's report ties wall embedding to the capsule change, but the specific offending contact/transform is untraced.
- The slope snag remains a separate open regression. The exact route and first blocking contact still need a live trace.
- Human review still needed: retest the restored box against the same wall and confirm the actor remains outside; then record the uphill route separately.

## Pre-existing work

The checkout contained unrelated modified and untracked files before this session. They were preserved. Only the rocket launcher visibility setting and the three collision regression documentation paths above were changed by this session, in addition to this changelog.
