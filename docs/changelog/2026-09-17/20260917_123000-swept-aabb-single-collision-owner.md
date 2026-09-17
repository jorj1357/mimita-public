# Swept-AABB gather and single collision owner

- EST timestamp: 2026-09-17 12:30:00 -04:00
- Branch: `8292026stash`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Goal

Fix the reported fall-through ("I fall down to the void, no collisions") and the
server-position divergence caused by the collision package reporting a successful
solve while doing nothing. Per human decision: exactly one collision owner, and
a true swept-AABB broadphase gather.

## Root cause

- `collision.main` gathered the candidate triangle list once from the start
  position plus a `0.2` padding and reused it for every substep. The actor moved
  outside that region during a fall or dash, so floor/wall triangles were never
  tested and the actor passed through.
- The world-unavailable path integrated plainly and returned `handled = 1`, and
  the non-finite path zeroed velocity and returned `handled = 1`. Both presented
  a non-solve as a valid authoritative result, so the server never corrected the
  divergent position.
- `movement.main` also fell back to a second in-DLL solver when the capability
  was absent, i.e. two possible collision owners.

## Changes

- `src/hot-reload/packages/collision/collision-package-solver.cpp`
  - Added `sweptUnionAABB`: one region covering every collider swept across the
    whole tick move, padded by `kSweepMargin` (`0.2`).
  - The solve now gathers candidates once over that swept region and shares the
    list across all substeps and passes. A mid-sweep re-gather runs only when the
    per-tick move exceeds `kMaxSweepReGatherDistance` (`6.0`) as a safety net.
  - World-unavailable now returns without `handled`, so a missing world can never
    be mistaken for a collision result.
  - Non-finite input returns without `handled` as well.
  - Added throttled phase timing (`kTimingLogIntervalSeconds = 1.0`) printing
    `[COLLISION PACKAGE] solves/broadMs/narrowMs/totalMs/avgCandidates/large/
    avgContacts` at most once per second. Never per solve.
- `src/hot-reload/modules/movement-system.cpp`
  - `resolveCollisions` now has exactly one owner: `collision.main`. Removed the
    in-DLL fallback solver. If the capability is absent the actor keeps its
    plain-integrated velocity for that tick instead of two systems mutating it.
- `src/hot-reload/packages/collision/collision-package-selftest.cpp`
  - New test 8: fast fall (`z=5`, `vz=-240`) must ground and must not tunnel.
  - New test 9: with no world geometry the solve must decline (`handled == 0`).
- Removed `src/hot-reload/hot-collision-kernel.h` (the now-superseded duplicate
  solver) and its manifest entry and self-test hook, completing the Section 8
  "remove duplicate ownership" step for this path.
- `src/hot-reload/hot-movement-collision.h` header comment now states it is the
  ragdoll/consumer geometry helper, not the movement collision owner.

## Evidence

- Hot build: `python devscripts/live-build.py` produced
  `build/hotreload/mimita-live-g000035.dll`; no `MiMITA.exe` is written. An
  intermediate generation-33 failure (`hot-collision-kernel.h` include left in
  `movement-system.cpp`) was fixed and the record kept here.
- Headless load/validation: `mimita-20260917T151237.exe --live-code-selftest`
  reported `[LIVE CODE SELFTEST] PASS` with `[ok] GameAPI load + ABI + self-test`,
  which now includes the swept-AABB fall test and the decline-on-no-world test.
- `git diff --check` passed (exit 0).
- No `.exe` build was required: no manifest `cold` source changed, and no running
  process was killed, restarted, or replaced.

## Human verification still required

- Fall from height, walk off ledges, dash into walls: the actor must not pass
  through blocks and must not reach the void.
- Confirm the server-side position matches the client (no divergence report) for
  normal movement, falling, and landing.
- Watch the throttled `[COLLISION PACKAGE]` line to confirm per-solve cost is
  small and candidate counts stay local rather than scaling with map triangles.

## Not done (later phases)

- Entity-vs-entity collision, NPCs, remote players, projectiles, weapons, props,
  ragdolls, triggers.
- Dedicated-server `world.collision` binding timing: if the headless world is
  bound after the first server solve, that first solve now declines instead of
  integrating uncollided. Confirm no actor is left unsolved across that window.
