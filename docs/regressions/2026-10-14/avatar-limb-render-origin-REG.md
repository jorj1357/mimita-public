# Avatar Limb Render Origin Regression

Time created: 2026-10-14T00:30:56Z
Time last updated: 2026-10-14T00:30:56Z

Status: ATTEMPTED FIX (1)

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
its arms and legs displaced from the torso and head. The arms sat too low,
near mid-torso, and the legs hung well below the hips; the head was offset
forward relative to the torso.

Active run: `C:\mimita-v9\logs\2026-10-14\20261014_001808\events.jsonl`
(build ~2246).

### Expected Behavior

Each rendered body part must appear at the authored Blender node position for
the current physics body pose, exactly as the `.glb` stores it. At bind, the
rendered node origin must equal the imported node world position.

### Actual Behavior

The render origin for every attached part (head, arms, legs) was computed from
the physics joint anchor `childLocalAnchor` (the capsule end nearest the
parent) instead of the authored node origin offset `meshLocal[3]`. Because the
capsule end is not the Blender node origin, each attached mesh was shifted by a
constant offset in the body frame:

| part | mesh_local_translation | child_local_anchor | origin error |
|---|---|---|---|
| head | (-0.100, 0, -0.196) | (-0.14, 0, 0) | 0.20 |
| leftArm / rightArm | (0, ∓0.067, 1.451) | (0, 0, 0.592) | 0.86 |
| leftLeg / rightLeg | (0.100, ∓0.111, 1.418) | (0.1, 0, -0.6) | 2.02 |
| torso | (0, 0, -0.031) | (0, 0, 0) | 0.03 |

### Why This Is Bad

The limbs are drawn detached from their joints and the head is offset from the
torso. This is the exact user-visible symptom (arms near the middle of the
body, legs splayed below the hips) and makes the avatar unusable/unreadable.

### Specification

`docs/specs/debug-logging/AI-fix-live.md`

Relevant requirement: for an avatar GLB the transform chain must distinguish
the authored node transforms from applied overrides, and the first divergence
must be named from `avatar.pose.sample` / bind records. The rendered node frame
must reproduce the imported authored node position.

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

This was introduced by commit `8ae271c5`. The root (torso) branch was added
afterward by the uncommitted change recorded in
`docs/changelog/2026-10-14/20261014_001800-avatar-root-bind-alignment.md`,
which fixed only the torso and left the attached parts on `childLocalAnchor`.

### Confirmed Cause

`meshLocal = inverse(bodyBindWorld) * nodeWorld` (`ragdoll-mode.cpp`), so
`meshLocal[3]` is exactly the center-of-mass-to-node-origin bind translation.
`childLocalAnchor` is the physics joint anchor computed in `initParts` from the
capsule end nearest the parent and is consumed correctly by `solveJoints`. The
two anchors are not interchangeable; using the joint anchor as the render
origin moves the authored mesh.

Evidence: bind-stage `AIMBODY_AVATAR_BIND_PART` records in
`20261014_001808/events.jsonl` show `mesh_local_translation` and
`child_local_anchor` differ by 0.20 (head), 0.86 (arms), and 2.02 (legs),
matching the visible displacement. The previous fix's `final_node_pivot_error ≈ 0`
was circular: it compared the final node against `visualPartOrigin` itself, so
it could not detect the offset.

### Attempted Fix 1

Time: `2026-10-14T00:16Z` (the uncommitted torso-only change) — repaired the
torso root only; arms, legs, and head remained displaced.

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

`visualPartWorld` is unchanged. `childLocalAnchor` continues to own the physics
joint in `solveJoints`; only the render origin rule changed.

### Fix

The render origin for every part, root and attached alike, is
`body.position + body.orientation * meshLocal[3]`, which reproduces the
authored Blender node origin. Physics bodies, collision capsules, joint
anchors, and network state are unchanged.

### Proof

Human review:

Not yet performed. A visible in-game screenshot of `monsterstabber` is still
required to confirm the mesh silhouette.

Automated proof:

- Fresh build `2251` (run `C:\mimita-v9\logs\2026-10-14\20261014_002904\events.jsonl`).
- Scenario: `avatar.load monsterstabber`, `agent.snapshot`, `agent.aim 1 0 0.2`,
  `agent.snapshot`.
- New non-circular `AIMBODY_AVATAR_BIND_PART` field `bind_node_pivot_error`
  (rendered origin vs imported rest node position) is `~1e-5` for head, both
  arms, and both legs (was 0.20 / 0.86 / 2.02 before the fix).

### Solution

`ATTEMPTED FIX (1)` — runtime numeric invariant confirmed; awaiting human
visual confirmation before `SOLUTION AS OF`.
