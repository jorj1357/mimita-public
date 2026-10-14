# Avatar Limb Render Origin Regression

Time created: 2026-10-14T00:30:56Z
Time last updated: 2026-10-14T00:45:00Z

Status: ATTEMPTED FIX (3)

Related specification:
`docs/specs/debug-logging/AI-fix-live.md` (avatar transform evidence profile),
`docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md`

Related changelog:
`docs/changelog/2026-10-14/20261014_003000-avatar-limb-render-origin.md`

---

## Regression Occurrence 1

### Observed

Time: `2026-10-14T00:16:27Z`

In the live client the monster avatar `monsterstabber`
(`assets/entity/player/monsters/stabber/mimita-stabber-v7.glb`) rendered with
its arms and legs displaced from the torso and head, and the limbs rotated
about the middle of the limb rather than the shoulder/hip. The arms sat too
low, near mid-torso, the legs hung below the hips, and the head was offset
forward relative to the torso.

Active run: `C:\mimita-v9\logs\2026-10-14\20261014_001808\events.jsonl`
(build ~2246).

### Expected Behavior

Each rendered body part appears at the authored Blender node position for the
current physics pose, and a limb rotates about its Blender node origin (the
shoulder or hip), not about the capsule center of mass.

### Actual Behavior

Two independent defects combined:

1. The rendered node origin for attached parts came from the physics joint
   anchor `childLocalAnchor` (the capsule end nearest the parent) instead of the
   authored node-origin offset `meshLocal[3]`.
2. The physics joint anchor itself was the capsule end, which for this avatar
   sits near the middle of the limb, so the solver pinned the limb at its middle
   and the authored shoulder/hip orbited that pinned point.

### Why This Is Bad

The limbs are drawn detached from their joints and swing about their middle,
which is the user-visible symptom and makes the avatar unusable.

### Specification

`docs/specs/debug-logging/AI-fix-live.md`

Relevant requirement: for an avatar GLB the transform chain must distinguish
the authored node transforms from the physics body, and the rendered node frame
must reproduce the imported authored node position. `docs/gold/2026-10-06-...`
records the rule that attachment anchors derive from the avatar's actual bind
pose, not from a generic capsule guess.

### Wrong Code

File:

`src/ragdoll/ragdoll-mode.cpp`

```cpp
static glm::vec3 visualPartOrigin(const RagdollModePart& part,
                                  const glm::vec3& position,
                                  const glm::quat& orientation)
{
    glm::vec3 nodeOrigin = position;
    if (part.parentIndex >= 0)
        nodeOrigin += orientation * part.childLocalAnchor;
    else
        nodeOrigin += orientation * glm::vec3(part.meshLocal[3]);
    return nodeOrigin;
}
```

```cpp
// Default anchor: the capsule end nearest the parent body.
Capsule cap = capsuleOf(child.body);
glm::vec3 anchor = (glm::length(cap.a - parent.body.position)
                    < glm::length(cap.b - parent.body.position)) ? cap.a : cap.b;
```

### Confirmed Cause

`meshLocal = inverse(bodyBindWorld) * nodeWorld`, so `meshLocal[3]` is exactly
the COM-to-node-origin bind translation. The capsule end is a collision-artifact
point, not the authored joint. For `monsterstabber` the difference is 0.86
(arms), 2.02 (legs), 0.20 (head) model units.

Evidence: bind-stage `AIMBODY_AVATAR_BIND_PART` records in
`20261014_001808/events.jsonl`. The previous shoulder-pivot fix's
`final_node_pivot_error ≈ 0` was circular (it compared the final node against
`visualPartOrigin` itself) and could not detect the offset.

### Attempted Fix 1

Time: `2026-10-14T00:16Z` — the uncommitted torso-only render branch.
Result: torso fixed; arms, legs, and head still displaced.

### Attempted Fix 2

Time: `2026-10-14T00:31Z` — `visualPartOrigin` returns
`position + orientation * meshLocal[3]` for every part (build 2251).
Result: positions correct (`bind_node_pivot_error ≈ 0`), but limbs still
rotated about their middle because the joint anchor remained the capsule end.
Measured `|arm_node - torso_parent_anchor|` mean 0.86 (arms) / 1.19 (legs) /
0.20 (head) in `20261014_002904/events.jsonl`.

### Corrected Code

File:

`src/ragdoll/ragdoll-mode.cpp`

```cpp
static glm::vec3 visualPartOrigin(const RagdollModePart& part,
                                  const glm::vec3& position,
                                  const glm::quat& orientation)
{
    return position + orientation * glm::vec3(part.meshLocal[3]);
}
```

```cpp
// Default anchor: the child's authored node origin from the bind pose
// (shoulder/hip).
glm::vec3 anchor = child.body.position
    + child.body.orientation * glm::vec3(child.meshLocal[3]);
```

With `childLocalAnchor == meshLocal[3]`, the joint solver pins the authored
node origin to the parent anchor, and the render origin is that same point, so
the mesh rotates about its shoulder/hip.

### Fix

Render origin and joint anchor both use the authored Blender node origin
(`meshLocal[3]`). Physics bodies, collision capsules, COM offsets, and network
state are unchanged.

### Proof

Human review:

Not yet performed. A visible in-game screenshot of `monsterstabber` is still
required to confirm the mesh silhouette and hinge behavior.

Automated proof:

- Builds: `2251` (attempt 2) and `2258` (attempt 3).
- Fresh run `C:\mimita-v9\logs\2026-10-14\20261014_003832\events.jsonl`.
- `bind_node_pivot_error ≈ 1e-5` for all parts.
- `|rendered node - torso parent anchor|` mean dropped from 0.86/1.19/0.20
  (build 2251) to 0.02/0.02/0.04 for arms/legs/head (build 2258): the rendered
  shoulder/hip is now the pinned joint, i.e. the limb hinges at the
  shoulder/hip.
- Full-ragdoll corpse path stayed stable (`ragdoll.corpse.pose_step_trace`
  parts finite and clustered; no crash; client uptime stable).

### Solution

`ATTEMPTED FIX (3)` — runtime joint invariant confirmed; awaiting human visual
confirmation before `SOLUTION AS OF`.
