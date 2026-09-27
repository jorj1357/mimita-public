# afad20a limbs — world-space body-part collision source

Date: 2026-09-23
Status: hot + cold built; deterministic tests pass; live limb fix requires the new
EXE; human acceptance pending

Related regression: `docs/regressions/2026-09-22/limb-contact-ability-reset-REG.md`

Reference commit: `afad20a` ("npc stuff its cool", 2026-09-11).

## How limbs worked at afad20a (from `physics-collision-body.cpp`)

- One sphere per `Player::physicalBody.parts` entry.
- Sphere centre = collider AABB centre transformed by the part's **world**
  transform:
  `outCenter = part.worldTransform * vec4((localMin+localMax)*0.5, 1)`.
- Radius = `max(local half-extents, BODY_SAMPLE_RADIUS=0.15)` clamped to `0.35`.
- Sweep delta = `center - previousCenter`, where previous uses
  `part.previousWorldTransform`.
- `part.worldTransform = rootWorld * localToRoot` with
  `rootWorld = translate(movementCapsule.position) * mat4_cast(rotation)`. The
  root and model scale are already baked in, so **no extra size scale** is
  applied to body parts.
- `p.updateModelWorldTransforms()` runs at the **start** of the body/weapon
  pass, so the spheres use the current animated transforms.
- Contacts are solved with `solveBatchedCorrection` and `p.pos += correction`, so
  limbs push the root.

## Live evidence that found the bugs

From `logs/2026-09-23/20260923_011027/events.jsonl` (client, generation 6):

- Ability reset: `movement.contact_ability` shows
  `touch=1 grounded=1 collided=0 ... ability_down_dash_before=0 after=1 restored=1`.
  The grounded-reset fix was already active, so ground Q down-dash is restored
  every grounded tick (afad20a synthetic Ground contact).
- Limbs: `movement.collision` shows `limbSrc=body.parts colliders=7 limbCols=6
  limbHits=0` while grounded. The six limb colliders were submitted but never
  contacted, i.e. they were placed/scaled wrong.

## Root cause

The `body.parts` capability returned **root-relative** transforms, and the hot
builder re-composed them with the hot root **and multiplied by `sizeScale`**.
afad20a did neither; the part world transform already contains the root and
model scale. The player root also includes a `-0.138` feet offset that the
inverse-root did not, so the limb centres were shifted and the radius was scaled
incorrectly, producing no contacts.

## Fix

- `src/hot-reload/game-api.h` `GameBodyPartV1`: fields are now
  `worldPosition` / `worldRotation` / `previousWorldPosition` plus bounds, and a
  `space` flag (`1` = world, `0` = legacy root-relative) with a reserved word.
- `src/live-code/live-behavior.cpp` `capBodyParts`: returns the part **world**
  transform directly (`part.worldTransform`, `part.previousWorldTransform`) and
  the part-local collider AABB, and sets `space = 1`.
- `src/hot-reload/modules/movement-system.cpp` `buildPlayerCollision`: computes
  the afad20a sphere centre `worldPosition + worldRotation * localCenter`, radius
  `min(max(half-extents, 0.15), 0.35)` with **no size scale**, and sweep
  `center - previousCenter`. A legacy `space == 0` branch composes root-relative
  values with the hot root and scale so a new DLL still works against an older
  EXE during a partial update.
- Limb diagnostics: `movement.collision` now also logs
  `limb0=(x y z r=...)` for the first limb collider.

## Evidence

Build evidence:

- Hot DLL: `python build_game_dll.py` -> `DLL build success`.
- Cold build: `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260922T213140.exe` (the `body.parts` capability is EXE code, so a
  cold rebuild is required for the limb fix).

Test evidence:

- `--afad20a-parity-selftest`, `--movement-selftest`,
  `--movement-parity-selftest`, `--live-code-selftest`, `--collision-selftest`:
  PASS.

Runtime evidence:

- The grounded-reset behavior is confirmed by the journal above.
- The limb world-space fix was not yet observed live; the new EXE must be run.

Human acceptance:

- Pending. Run `mimita-20260922T213140.exe`, walk arms/legs into a wall, and
  confirm `limbHits > 0` and that the root is pushed out.

## Limits

- Body parts are one proxy sphere each (afad20a); the visible mesh can still
  overlap geometry before the proxy contacts.
- The limb fix is EXE code: a still-running older EXE hot-reloading the new DLL
  uses the legacy `space == 0` path and keeps the old (broken) limb placement
  until the game is restarted with the new EXE.
- Freeze availability is still not a persisted field (only the freeze timer
  resets on contact).
