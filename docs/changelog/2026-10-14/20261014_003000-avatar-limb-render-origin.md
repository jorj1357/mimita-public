# Avatar limb render origin fix

Time: 2026-10-14T00:30:56Z

## Task

Investigate why `mimita-stabber-v7.glb` rendered with arms and legs displaced
from the torso and head in the live game, and fix it following
`docs/specs/debug-logging/AI-fix-live.md`.

## Routed documents read

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/debug-logging/AI-fix-live.md` (avatar transform evidence profile)
- `docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md`
- `docs/architecture/player-npc-systems/` (ragdoll/aim body ownership)
- `docs/changelog/2026-10-14/20261014_001800-avatar-root-bind-alignment.md`
- `docs/changelog/2026-10-13/20261014_000500-avatar-shoulder-pivot-live-fix.md`
- `docs/regressions/README.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`

## Root cause

`RagdollModeSystem::visualPartOrigin` (`src/ragdoll/ragdoll-mode.cpp`) built the
rendered node frame for attached parts from the physics joint anchor
`childLocalAnchor`. `meshLocal = inverse(bodyBindWorld) * nodeWorld`, so
`meshLocal[3]` is the authored COM-to-node bind translation, while
`childLocalAnchor` is the capsule end nearest the parent used by `solveJoints`.
Using the joint anchor as the render origin moved each attached mesh by a
constant body-frame offset.

Evidence from `logs/2026-10-14/20261014_001808/events.jsonl`
(`AIMBODY_AVATAR_BIND_PART`, stage bind), origin error
`|child_local_anchor - mesh_local_translation|`:

| part | error |
|---|---|
| head | 0.20 |
| leftArm / rightArm | 0.86 |
| leftLeg / rightLeg | 2.02 |
| torso | 0.03 |

The previous shoulder-pivot fix's `final_node_pivot_error ≈ 0` was circular:
it compared the final node against `visualPartOrigin` itself, so it could not
detect the displacement. The uncommitted root-only change repaired the torso
but left arms/legs/head displaced.

## Source changes

File: `src/ragdoll/ragdoll-mode.cpp`

- `visualPartOrigin` now returns `position + orientation * meshLocal[3]` for
  every part (root and attached). The `parentIndex`/`childLocalAnchor` branch is
  removed. `visualPartWorld` is unchanged and physics/collision/joint anchors
  are untouched.
- `logAimTransformSnapshot` (`AIMBODY_AVATAR_BIND_PART`) now records
  `expected_visual_pivot_world_position` and `bind_node_pivot_error`
  (rendered origin vs imported rest node position). At stage bind the skeleton
  is still at rest, so this is a non-circular check.

## Pre-existing edits preserved

- Uncommitted working-tree change to `src/ragdoll/ragdoll-mode.cpp` (torso root
  branch) from `20261014_001800` — superseded by this fix, not reverted.
- Uncommitted `config/analytics.json` `launch_count` bump — untouched.

## Build and runtime evidence

- Dev loop reused (no competing daemon). Builds published: `2250`, then `2251`
  from source generation 6.
- Live run: `C:\mimita-v9\logs\2026-10-14\20261014_002904\events.jsonl`.
- Executable: `C:\mimita-v9\.dev\builds\2251\mimita.exe`; server and client
  build identities matched (`builds_match=true`).
- Scenario via the shared command path: `avatar.load monsterstabber`,
  `agent.snapshot`, `agent.aim 1 0 0.2`, `agent.snapshot`.
- `avatar.model.transform-source`: avatar `monsterstabber`,
  `body_frame_policy=player_canonical`, `source=model`.
- `AIMBODY_AVATAR_BIND_PART` `bind_node_pivot_error` after the fix is `~1e-5`
  for head, both arms, and both legs (was 0.20 / 0.86 / 2.02).
- `AIMBODY_LIVE_SAMPLE` confirms rendered node positions now sit at the
  authored locations: head above torso, arm origins at shoulder height, leg
  origins at hip height.

## Separate claims

- Source change: applied at the single owner, smallest patch.
- Build: `2251` published successfully from current source generation.
- Runtime journal: fresh run proves the bind origin matches the authored node
  position for all parts.
- Human visual acceptance: NOT performed this session. A screenshot of
  `monsterstabber` in the visible client is still required.

## Regression

Recorded: `docs/regressions/2026-10-14/avatar-limb-render-origin-REG.md`
(`ATTEMPTED FIX (1)`, pending human visual confirmation).

## Human review still needed

Confirm visually in-game that `monsterstabber` arms, legs, and head align with
the torso, and that the weapon mount still looks correct at several camera
pitches.
