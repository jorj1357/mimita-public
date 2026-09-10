# Ragdoll: bind limbs to the capsule from the rest pose, not the animated pose

- Task ID: ragdoll-activation-rest-bind
- Summary: On entering ragdoll mode, reset the skeleton to the model's rest
  pose before deriving `meshLocal`, so a limb's current animation rotation is no
  longer inherited and stuck relative to its capsule.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: `2026-09-10T22:09:58Z` (2026-09-10 18:09:58 EDT)
- Branch: `8292026stash`
- Base commit: `322e5db` (working tree; no commit created)
- Final commit: none (uncommitted)

## Pre-existing changes

- Not created by this session: the large uncommitted corpse-ragdoll / impact
  decal work currently in the tree (`src/ragdoll/ragdoll-mode.*`,
  `src/effects/*`, `src/config/impact-decals-config.*`, deleted `ragdoll.cpp`,
  `death-ghost.cpp`, etc.) and its changelog
  `docs/changelog/2026-09-10/20260910_220023-impact-decals-ragdoll-corpse.md`.
  Not claimed here. This session only changed the two bind sites named below.
- `config/ragdoll.json`, `config/impact_decals.json`, `config/analytics.json`
  hand/runtime edits were not touched.

## Diagnosis (human playtest)

Reported: if the walking animation holds an arm rotated about the shoulder (for
example ~30 degrees) when ragdoll is toggled, the arm stays stuck at that angle
relative to its ragdoll capsule.

Owner: `RagdollModeSystem::initParts`
(`src/ragdoll/ragdoll-mode.cpp`). It binds the mesh with

```cpp
const glm::mat4& nodeWorld = player.perfectPoseSkeleton.nodes[bp->nodeIndex].worldTransform;
...
part.meshLocal = glm::inverse(bodyBindWorld) * nodeWorld;
```

`activate` refreshed the skeleton to the **current animated pose** immediately
before this, so the limb's animation rotation (shoulder swing) was baked into
`meshLocal`. Physics then moved the body, and the mesh stayed rigidly off-axis
from the capsule by that inherited rotation.

## Implementation change

`src/ragdoll/ragdoll-mode.cpp`.

`activate` (`RagdollModeSystem::activate`), old:

```cpp
// Refresh the mesh skeleton so the bind frames reflect the current pose.
player.updateModelWorldTransforms();
```

new:

```cpp
// Bind from the model's rest pose, not the currently animated pose. ...
{
    const size_t n = std::min(player.perfectPoseSkeleton.nodes.size(),
                              player.perfectPoseSkeleton.restLocalTransforms.size());
    for (size_t i = 0; i < n; ++i)
        player.perfectPoseSkeleton.nodes[i].localTransform =
            player.perfectPoseSkeleton.restLocalTransforms[i];
}
player.updateModelWorldTransforms();
```

`reinitPreservingState` (live `ragdoll.json` reload) had the same class of bug:
it re-bound `meshLocal` from the live ragdoll pose. It now resets the skeleton
to rest before `initParts` and still restores the saved body poses/velocities
afterwards, so `meshLocal` is the true constant body-to-mesh bind offset.

Corpse spawning was intentionally left alone: `spawnCorpse` calls
`initParts(victim, ...)` directly on the victim's frozen death pose, which is
the desired corpse spawn pose.

## Documents and skills

- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md` — visible body stays 1:1
  with the physics capsules.
- `docs/skills/spec-behavior-review-v1.md` — result: `PASS_WITH_HUMAN_REVIEW`.
  Finding: the bind inherited the animation frame, disagreeing with the
  "mesh matches the capsules" requirement. Code path:
  `RagdollModeSystem::activate` / `initParts`. Fixed by binding from rest.
- `docs/operations/task-completion/task-completion.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`.

## Validation

- Build: `python build_agent.py` -> `BUILD SUCCESS`, return code 0; only
  `src/ragdoll/ragdoll-mode.cpp` recompiled; `mimita.exe` relinked.
- Config: unchanged.
- Runtime: not performed this session.

## Human acceptance

- Walk/idle so an arm is mid-swing, then toggle ragdoll: the arm must snap to
  the capsule's bind orientation, not stay stuck at the animated angle.
- Confirm the same for legs mid-stride.
- Confirm live-editing `config/ragdoll.json` while ragdolled no longer twists
  the limbs relative to the capsules.
- Confirm corpse ragdolls still spawn in the victim's death pose.
