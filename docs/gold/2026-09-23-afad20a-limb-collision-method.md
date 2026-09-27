# 2026-09-23 — afad20a limb collision: method and reference

Status: `GOLD / REFERENCE` — how the per-limb collision source was restored to
match `afad20a`, how the break was found from the live journal, and how to verify
it again. Companion to the `afad20a` movement oracle
(`src/physics/movement/reference/movement-afad20a-reference.*`).

## Why

MiMITA characters are Minecraft-style articulated bodies: head, torso, arms,
legs, plus the weapon. In `afad20a` those parts are **authoritative over the
player root** — an arm on a ledge holds the player, and a limb cannot enter a
wall. Judging "does my arm collide" by eye is unreliable, so the break was
localized from the append-only journal first, then the exact `afad20a` producer
was restored and verified by numbers (`limbSrc`, `limbCols`, `limbHits`,
`limb0`).

## What actually existed at afad20a

`git show afad20a:src/physics/movement/physics-collision-body.cpp`:

```cpp
static bool computeBodyPartCenter(const glm::mat4& xform, const Collider& collider,
                                  glm::vec3& outCenter, float& outRadius) {
    glm::vec3 localCenter  = (collider.localMin + collider.localMax) * 0.5f;
    glm::vec3 localExtents = (collider.localMax - collider.localMin) * 0.5f;
    outCenter = glm::vec3(xform * glm::vec4(localCenter, 1.0f));
    outRadius = std::max({localExtents.x, localExtents.y, localExtents.z,
                          BODY_SAMPLE_RADIUS});       // 0.15
    outRadius = std::min(outRadius, 0.35f);
    return true;
}
```

`collectBodyWeaponSpheres` walks `Player::physicalBody.parts` and for each part
emits **one sphere** at the collider-AABB centre transformed by the part's
**world** transform, with `sweepDelta = center - previousCenter` from
`part.previousWorldTransform`. `collectBodyWeaponContacts` does one union-AABB
gather and sweeps each sphere (`sweepSphereTriangle` / `sphereTriangleContact`).
`doBodyWeaponCollisionPhase` runs **before** the root capsule pass, after
`p.updateModelWorldTransforms()`, and applies the contacts with
`p.pos += correction`.

Key facts:

- `part.worldTransform = rootWorld * localToRoot`,
  `rootWorld = translate(movementCapsule.position) * mat4_cast(rotation)`.
  The root and model scale are already baked in, so body parts get **no extra
  size scale**.
- `updateModelWorldTransforms()` runs at the start of the body pass, so the
  spheres use the current animated transforms.

## How the break was found (live journal)

`logs/2026-09-23/20260923_011027/events.jsonl` (client, hot generation 6)
contained the throttled `movement.collision` record:

```text
branch=solved limbSrc=body.parts colliders=7 limbCols=6 limbHits=0
grounded=1 worldContact=1 bodyContact=0 contacts=1 ...
```

Read literally: the six limb colliders were being submitted
(`limbCols=6`), but none ever contacted the world (`limbHits=0`), while the root
capsule did (`worldContact=1`). So the limbs were present but **misplaced or
mis-scaled** — not missing.

## Root cause

The hot `body.parts` capability returned **root-relative** transforms, and the
hot builder re-composed them with the hot root **and multiplied by
`sizeScale`**. `afad20a` did neither. The player root also carries a `-0.138`
feet offset (`player.cpp` `kModelFeetZOffset`) that the inverse-root did not
include, so the limb centres were shifted and the radius was scaled, producing
no contacts. The journal's `limbHits=0` was the falsifiable symptom.

## Fix (one owner, exact afad20a formula)

- `src/hot-reload/game-api.h` `GameBodyPartV1` carries `worldPosition` /
  `worldRotation` / `previousWorldPosition` + part-local collider bounds, plus a
  `space` flag (`1` = world, `0` = legacy root-relative) so a new DLL stays
  compatible with an older EXE during a partial update.
- `src/live-code/live-behavior.cpp` `capBodyParts` returns
  `Player::physicalBody.parts[i].worldTransform` /
  `.previousWorldTransform` directly (the exact transforms the renderer draws)
  and sets `space = 1`.
- `src/hot-reload/modules/movement-system.cpp` `buildPlayerCollision` builds the
  `afad20a` sphere: `center = worldPosition + worldRotation * localCenter`,
  `radius = min(max(half-extents, 0.15), 0.35)` with **no size scale**, and
  `sweep = center - previousCenter`. Contacts are `BODY_AUTHORITATIVE`, so the
  collision package pushes the root (not just the capsule).

## Verification method (reusable)

1. Run the game and check the throttled record:
   `movement.collision ... limbSrc=body.parts limbCols=6 limbHits>0 limb0=(x y z r)`.
   `limbHits>0` when an arm/leg is against geometry is the pass condition;
   `limb0` names the first limb centre and radius so a wrong placement is
   visible without a debugger.
2. `space` distinguishes the source: `1` = world (new EXE), `0` = legacy
   root-relative (older EXE hot-loading a new DLL).
3. Deterministic package tests (`collision-package-selftest.cpp`) cover limb
   penetration pushing the root and the per-limb sweep crossing thin geometry.

## Evidence levels

- Source: `afad20a` producer quoted above; fix at the files listed.
- Build: hot DLL `DLL build success`; cold `Status: SUCCESS`
  (`mimita-20260922T213140.exe`).
- Deterministic tests: `--collision-selftest`, `--live-code-selftest`,
  `--afad20a-parity-selftest`, `--movement-selftest`,
  `--movement-parity-selftest` all PASS.
- Live: the journal proved the old failure (`limbHits=0`). The world-space fix is
  EXE code, so it only takes effect after restarting with the new EXE.
- Human acceptance: reported by the user — arm collisions now work.

## Lesson

A "limbs don't collide" report is not solved by inspecting the producer alone.
The journal's `limbSrc`/`limbCols`/`limbHits` triple split the failure into
"not submitted" vs "submitted but misplaced" in one line. Add the counter that
distinguishes those two before writing any fix; the counter is what made the
root cause obvious (`limbCols=6 limbHits=0` ⇒ placement/scale, not wiring).

## Still open (not covered by this document)

- Q down-dash and normal dash repeat semantics are not yet confirmed to match
  `afad20a`; see the follow-up session.
- Animation is not yet 1:1 with `afad20a` (idle/walk/return-to-idle, dash/freeze
  pose overlays, exact springs), only hot-reloadable with a C++/JSON selector.
