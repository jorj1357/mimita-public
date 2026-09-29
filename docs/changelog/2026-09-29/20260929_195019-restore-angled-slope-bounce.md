# Restore angled slope bounce

Time: 2026-09-29T19:50:19Z
Branch: `afad20a-rebuild`

## Result

Implemented the requested restoration for downward dashes and other impacts on
slopes or angled surfaces. The JSON-controlled bounce path remains the single
response owner. Real walkable slope normals are no longer converted into a
straight-up normal; only numerically flat floors are canonicalized to world-up.

## Exact changes

- `src/physics/movement/actor-triangle-solver.cpp:391`
  - Old threshold: `responseNormal.z > 0.90f`.
  - New threshold: `responseNormal.z > 0.995f`.
  - Added the reason comment: only a numerically flat floor is canonicalized;
    a real walkable slope keeps its oriented normal for shared bounce response.
- `src/physics/movement/physics-collision-stress.cpp:298`
  - Added an angled-surface response check using a normalized `(0.6, 0, 0.8)`
    normal and downward/inward movement.
  - The check requires both lateral and upward velocity when JSON bounce is
    enabled.
- `docs/regressions/2026-09-29/slope-edge-snag-REG.md:254`
  - Added Attempted Fix 7 and recorded that live acceptance is still pending.
- `docs/regressions/2026-09-20/cold-build-required-REG.md`
  - Added cold-build occurrence 23.

## Why

`respondVelocityAgainstNormal()` already reflects impact against the supplied
normal. The previous `0.90` canonicalization affected shallow walkable slopes,
so their response could become `(0, 0, 1)` and look like a straight-up bounce.
Keeping the true slope normal restores the requested sideward rebound without
adding a second collision owner, disabling JSON bounce, or suppressing landing
bounces.

## Documents and focused reviews

- `docs/ROUTER.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

Specification finding: the movement specification says down-dash against
slopes is intentionally allowed to create launches, and the collision
architecture says walkable finite slopes retain their oriented face normal.
This change aligns the solver with both requirements. No specification change
was made.

Logging review: no new unmanaged log was added. Existing collision JSONL
records already contain `surface_normal`, `response_normal`, and actor
velocity before/after response. Human review should correlate those fields
with a bookmark in the active run's `events.jsonl`.

## Validation

- `git diff --check`: source/doc patch has no new whitespace error; an existing
  CRLF trailing-whitespace warning remains in the unrelated cold-build record.
- `python build_agent.py`: first attempt had a linker-state failure while
  refreshing collision objects; rerun returned `Status: SUCCESS`.
- Executable: `.dev/builds/0462/mimita.exe`.
- `.dev/builds/0462/mimita.exe --collision-selftest`: exit code 0,
  `[COLLISION SELFTEST] PASS`, including
  `[COLLISION STRESS] angled surface keeps lateral bounce`.

## Evidence boundary

The build and self-test prove the source and deterministic response check. No
live game was launched, so slope traversal, down-dash contact selection, and
human feel remain `PASS_WITH_HUMAN_REVIEW` until reproduced in-game.
