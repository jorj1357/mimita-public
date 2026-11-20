# Recheck Arcade capsule slide proof

# Task

- Task ID: Continue Arcade collision and elite AI live proof
- Session ID: work-cb6e5f2853dc
- Owner: Codex
- Focus: issue-da337d1fe2f3
- Summary: Rechecked Arcade world contacts, the root capsule sweep and slide path, rejected step-up fallthrough, bounce dispatch, and actor-pair capsule ownership. No source was changed in this activation. The latest published DLL remains generation 410, while isolated runs with the current EXE have failed before logger.started; the startup owner remains active, so no overlapping run was started.
- Status: waiting_test_startup_owner
- Date, time, timezone: 2026-11-20T19:45:46Z / 2026-11-20 14:45 EST (America/New_York)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 6d92afa35b434790e1b42ebb9667be69a216cde2
- Final commit: 6d92afa35b434790e1b42ebb9667be69a216cde2

# Pre-existing changes

- Shared checkout already contains many unrelated agent edits. Preserved them; no source files were edited by this activation.
- Collision source changes and the player AI patch predate this activation. `src/actor/player-ai.cpp` has a coordination conflict and was not edited.

# Requested behavior

Arcade collision should avoid catching, bounce and slide across surfaces, prevent movement into world geometry, use only the root capsule against the world, and use capsule-versus-capsule for actor pairs.

# Specification alignment

- Specifications: `docs/specs/movement/movement.md`; `docs/architecture/collision/collision.md`; `docs/workflows/runtime-scenario-validation.md`; `docs/specs/dev-loop/agent-proof-and-verification.md`.
- Focused review: `docs/skills/spec-behavior-review-v1.md`.
- Finding: Arcade's source path routes static world contacts through the root capsule sweep/slide and skips body/weapon world collision. The failed-step-up branches now restore the original pose and fall through to slide resolution. Distinct actor pairs use the shared root-capsule resolver. Runtime behavior at the reported Dust2 ramp and other surfaces remains unverified.

# Exact implementation changes

- None in this activation.
- Reviewed `physics-collision-glb-main.cpp` (Arcade solver routing), `physics-collision-glb-sweep-slide.cpp` (`doGLBSweepSlide`, rejected step-up fallthrough, `respondVelocityAgainstNormal` at swept TOI), `actor-pair-collision.cpp` (`makeActorCapsule` / `capsuleDistance`), and `actor-final-pose.cpp` (Arcade pose correction guard).
- Existing generation 410 includes the earlier capsule fallback correction, published 2026-11-20T19:11:10.136Z from revision `28fa397bc3491bcdc69a4a26408af7c712deb734d0e83bc04761169444695ca3`.

# Validation

- Source review: PASS for the static routing and fixed-step solver ownership stated above.
- Build: no new build request in this activation. During review the dev-loop reported a new DLL build in progress; prior status identified generation 410 as published, but no claim is made about that active attempt.
- Runtime: NOT TESTED for this activation. Earlier pinned runs `20261120_191302`, `20261120_191710`, and `20261120_191948` failed before `logger.started`; they cannot prove the collision change. The active owner is diagnosing startup journaling, so this activation did not launch a competing test.
- Human acceptance: not performed. Need pinned Dust2 ramp playback, unscripted movement over cylinders/seams/slopes/corners, capsule actor-pair evidence, and visual confirmation of no penetration or snapping.

# Unresolved warnings and next step

Wait for `reconnect-root-cause` to resolve the isolated-run logger startup failure. Then use the exact current EXE and newly published collision DLL in a fresh pinned Arcade run; inspect server and client journals and continue through the user-requested AI movement and weapon proof. Do not report collision behavior complete from source review or the prior generation 405 run.
