# Stretched teleport outline

Time created: 2026-10-28T01:52:53Z
Time last updated: 2026-10-28T01:52:53Z

Status: ATTEMPTED FIX (1)

Related specification:
`docs/specs/gamemodes/teamdeathmatch.md`

Related changelog:
`docs/changelog/2026-10-28/20261028_015253-outline-priority-perf-contact.md`

Related run evidence:
`logs/2026-10-28/20261028_014618/events.jsonl`
`logs/2026-10-28/20261028_014759/events.jsonl`

## Regression Occurrence 1

### Observed

Time: `2026-10-28T01:46:44Z`.

TDM showed large flat red and green sheets that covered the world instead of
outlines around the actors. The `player_visuals.actor` diagnostics reported
sane current part origins but a stale previous pose:

- actor `200005` (BLUE 2, `monsterstabber`)
- `bounds_max_offset=1.153`
- `bounds_max_previous_offset=255.648`
- `camera_distance=75.242`

### Expected Behavior

An actor outline follows the actor's current mesh. Render-pose interpolation
must never bridge an old spawn/teleport position to the current position.

### Actual Behavior

`renderPartTransform()` interpolated the previous world transform even when the
fixed tick had moved a part hundreds of metres. The expanded outline shell
therefore stretched every triangle between the old and new poses.

### Why This Is Bad

The huge colored sheets obscure the map, hide the actor body, and make team
identification impossible in TDM.

### Specification

`docs/specs/gamemodes/teamdeathmatch.md` requires green teammate and red enemy
player outlines so players can identify their team. An outline that is not
aligned to the actor does not satisfy that requirement.

### Wrong Code

File: `src/entities/player-render.cpp`, function `renderPartTransform`.

```cpp
if (!settings.renderPoseInterpolation || alpha >= 0.999f ||
    !part.hasPreviousWorldTransform)
    return part.worldTransform;
```

The function then decomposed and interpolated `previousWorldTransform` without
checking whether it represented a valid fixed-tick step.

### Confirmed Cause

A respawn, avatar swap, or teleport can leave `previousWorldTransform` at the
old location. Runtime evidence is the `max_previous_offset=255.648` value above
while the current pose remained within 1.2 m of the actor root.

### Attempted Fix 1

File: `src/entities/player-render.cpp`, function `renderPartTransform`.

```cpp
const glm::vec3 currentPosition(part.worldTransform[3]);
const glm::vec3 previousPosition(part.previousWorldTransform[3]);
const float step = glm::length(currentPosition - previousPosition);
if (!std::isfinite(step) || step > 8.0f)
    return part.worldTransform;
```

The same change also made the outline and wireframe paths compute actual part
geometry bounds, reject non-finite/oversized geometry, and log the first
divergence. TDM now uses outlines with wireframes disabled.

### Proof

Build `3549` succeeded. The dev loop launched TDM NPC Elite on `atdm`, run
`20261028_014759`. After the change, no outline geometry was rejected as
non-finite or oversized, and `player_visuals.actor` recorded sane current and
previous offsets. A captured TDM window showed no giant red/green sheets.

Human visual acceptance of the corrected TDM outline remains pending. The
confirmed external cause is the stale previous pose; the visible fix still needs
a human to confirm the new outline silhouette is clear at normal play distance.
