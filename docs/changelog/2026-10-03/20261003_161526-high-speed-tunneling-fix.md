# Stop high-speed tunneling on the authoritative server

Time (UTC): `2026-10-03T16:15:26Z`
Branch: `afad20a-rebuild`

## Result

A client moving at extreme speed (bhop1, ~1000 m/s) no longer sends the whole
body through walls and no longer sticks a permanent server position
disagreement. The server stops treating the always-on hybrid/physical aim body
as a root-authoritative ragdoll, so its root now goes through the validated
movement path (which includes the swept blocking-geometry check). The local
client additionally splits its fixed tick into enough collision sub-steps that
no single sub-step can outrun the swept narrowphase.

## Root cause

Two independent owners let a fast body skip collision:

1. **Server accepted the client's aim-body root as authoritative.**
   In hybrid/physical aim-body mode the client replicates its body pose every
   tick with mode `RAGDOLL_NET_HYBRID` / `RAGDOLL_NET_PHYSICAL`. The server set
   `ServerPlayer::hasRagdollPose = true` for ANY active pose
   (`src/network/server-packet-handlers.cpp`), and
   `simulatePlayer` used that flag to overwrite the authoritative root from the
   pose with only a 4 m/tick clamp and no geometry test
   (`src/network/server-players.cpp`, old lines 750-797). That bypassed
   `crossesBlockingGeometry`, which the normal movement-report validation
   already runs (`src/network/movement-validation.cpp:624`). The server
   therefore accepted a through-wall position and then emitted a disagreement
   correction the client could not honor.
2. **Client actor sweep could exhaust its budget at extreme speed.**
   `physicsMainUpdate_Internal` always used a fixed `subSteps = 6`
   (`src/physics/physics-mini.h:27`). At ~1000 m/s one sub-step moves ~2.8 m,
   which inflates the swept actor narrowphase candidate/sample counts and can
   reach the `kMaxTriangleTests` early return
   (`src/physics/movement/physics-collision-mesh.cpp:526`) before the leading
   body part is tested. Fixed sub-step count is a sampling budget, not a
   distance bound, so it is speed-fragile.

## Changes

### Server: only a real ragdoll may own the root (committed `d2e2b9ba`)

`src/network/server-players.cpp` (`simulatePlayer`, ragdoll branch):

- old: `if (p.hasRagdollPose && !p.ragdollHistory.empty())`
- new: `const bool ragdollRootAuthoritative = p.hasRagdollPose && !p.ragdollHistory.empty() && p.ragdollHistory.back().pose.mode == RAGDOLL_NET_RAGDOLL;` and `if (ragdollRootAuthoritative)`
- moved the 4 m clamp onto a mutable `target` and added a swept guard:
  `if (MimitaNet::crossesBlockingGeometry(&world, before, target, 0.35f)) target = before;`

- `src/network/movement-validation.h`: declared the previously file-local
  `bool crossesBlockingGeometry(const HeadlessWorld*, glm::vec3, glm::vec3,
  float)` so the server can reuse the exact same wall test.
- `src/network/movement-validation.cpp`: moved `rayTriangle` and
  `crossesBlockingGeometry` out of the anonymous namespace into `namespace
  MimitaNet` (logic unchanged) so the declaration links.

### Client: speed-bounded collision sub-stepping (committed `d2e2b9ba`)

- `src/config/collision-config.h/.cpp`: added `maxSubStepDistance` (default
  0.35) and `maxSubSteps` (default 128), hot-reloadable, clamped in load.
- `src/physics/physics-mini.cpp` (`physicsMainUpdate_Internal`):
  - old: `const int steps = std::max(1, subSteps);`
  - new: `int steps = ...`, then when `moveSpeed * dt > maxSubStepDistance`,
    `steps = clamp(max(steps, ceil(totalTravel / maxSubStepDistance)), 1,
    maxSubSteps)`.
  Normal/low speeds keep the configured 6 sub-steps; only extreme speed adds
  sub-steps, keeping each sub-step under 0.35 world units.

### Config mitigation (committed `d2e2b9ba`)

- `config/collision.json`: added `"maxSubStepDistance": 0.35` and
  `"maxSubSteps": 128` with comments. Lower the distance for more safety at
  extreme speed; raise it for less cost. Set `0` to disable extra sub-stepping.

### Single-player / local-only code marked for deletion (uncommitted)

Added `TODO-DELETE (single-player)` markers to the clearest offline owners:
`src/network/net_mode.cpp`, `src/engine/engine-tick-state.cpp` (local duel and
local default-world fallback), `src/combat/death-system.cpp` (offline duel
tracking/respawn), `src/combat/weapon-rocket-launcher.cpp`,
`src/combat/weapon-system.cpp`, `src/network/local-gameplay-readiness.h`
(non-networked gameplay gate). This is a bounded first pass, NOT the full
audit the request asked for; client-side prediction is part of the
client-server architecture and was deliberately not marked.

## Documents and skills

- `AGENTS.md`, `docs/ROUTER.md` (route: multiplayer/server behavior).
- `docs/specs/networking/networking.md` (server authority; clients predict,
  server confirms collision).
- `docs/architecture/collision/collision.md`.
- Skill `docs/skills/spec-behavior-review-v1.md`: result
  `PASS_WITH_HUMAN_REVIEW`. Finding: the networking specification states the
  server confirms final collision results, so accepting a client root without a
  geometry test was a spec-code disagreement, severity high. No spec-spec
  conflict. Runtime/human acceptance still required.

## Validation

- Source: four changed translation units recompiled (objects newer than
  sources); reviewed diffs.
- Build: `python build_agent.py` -> `BUILD SUCCESS`, 51 compiled, 466 skipped,
  return code 0.
- Not proven: in-game play at >300 m/s on bhop1. Human review must confirm the
  body stops at walls at extreme speed in hybrid mode on a networked server.
  Also confirm actual ragdoll mode (G) is unaffected and that
  `maxSubStepDistance` tuning does not add unacceptable cost.

## Notes

- The server geometry test is `crossesBlockingGeometry` (center ray,
  near-vertical triangles). It still does not check non-vertical floor/slope
  triangles by design. A fuller swept-capsule server resolve for
  `resolveWorldCollision` (currently discrete push-out) remains future work.
- The mid-session commit `d2e2b9ba` already contains the committed half of this
  work; pre-existing unrelated edits were left untouched.
