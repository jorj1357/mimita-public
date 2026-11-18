# Rocket launcher rounded collision

- Task ID: session-20261118-065733
- Owner: Codex
- Date: 2026-11-18 01:57 EST (2026-11-18T06:57:45Z)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base/final commit: 43b89938b8aa29e3bffab982a544972f8dad7968 (working tree edits only)
- Status: PASS_WITH_HUMAN_REVIEW

## Request

Replace the rocket launcher's sharp world movement box with a capsule that is rounded all around. Keep the attack hitbox unchanged and allow the server to hot-reload the same collision config.

## Specification and review

- Read `docs/specs/movement/movement.md`: body and weapon collision are gameplay and must retain shared sweep/slide/world interaction.
- Read `docs/architecture/collision/collision.md`: reuse the shared collision owner and preserve cached broadphase behavior.
- Read `docs/architecture/json-configuration/json-configuration.md`: only call configuration live when its reload path is observed.
- Read `docs/specs/hotreload/hotreload.md` and `docs/specs/dev-loop/agent-lanes-build-authority.md`: game-owned `.cpp` edits go through the game DLL; agents do not build or request a build.
- Focused skill: `docs/skills/spec-behavior-review-v1.md`. Source/config alignment reviewed; live behavior and human feel review remain open.

## Changes made

1. `config/weaponcollisions.json`, `rocket_launcher`: changed from a box centered at `[0.15, 0, 0.2]` with half-size `[0.2, 0.2, 1.6]` to `source: "capsule"`, local endpoints `[0.15, 0, -1.2]` and `[0.15, 0, 1.6]`, radius `0.2`. The capsule's outer bounds match the prior box; its cross-section and ends are rounded. No attack/damage hitbox config changed.
2. `src/physics/movement/physics-collision-body.cpp`, `recomputeWeaponCapsule`: poll `WeaponCollisionJsonConfig` before applying it. The client already polls from `WeaponSystem`; this adds polling at the shared movement owner so server simulation can adopt the live config too.
3. `src/physics/movement/physics-collision-body.cpp`, `collectBodyWeaponSpheres`: increase configured capsule samples from 8 to 17 to reduce scalloping along the capsule. Contacts still use the existing shared actor/world response path.

## Pre-existing work

The checkout had many unrelated modified and untracked files before this session. They were preserved. Only the three paths listed in this changelog were edited by this task; the changelog itself is the required session record.

## Validation and runtime evidence

- Reviewed the focused diff. No tests were requested or run.
- No build or build request was made by this session; the EXE was not rebuilt.
- `python devscripts/dev-loop.py --status` reported the existing run `20261118_062057`, mode 57, visible client PID 19804 and server PID 18992. The root dev-loop already showed `game_building` before and after these edits, with generation pending. The reported in-progress source revision predates this source edit, so that build is not evidence that this change compiled or activated.
- `physics-collision-body.cpp` is marked `owner: game` in `build/source-manifest.json`; a subsequent game DLL generation is the expected no-EXE-rebuild path. Same-PID activation and runtime use have not yet been observed.
- The weapon collision config loader polls every 250 ms. Config hot reload, same-config adoption by server, visible collider shape, and snag reduction still need confirmation in the live journal/game.

## Regression and acceptance

No regression record was appended because the post-change behavior is unverified. The separate uphill slope issue remains unresolved in `docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md`.

Human gameplay review remains required: walk into walls and around corners with the rocket launcher, compare against the prior snag, and confirm the client and server use the same capsule after config reload and DLL activation.
