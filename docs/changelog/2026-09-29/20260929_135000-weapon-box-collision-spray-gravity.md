# Weapon box collision and spray gravity

Time: 2026-09-29 19:56:02 EDT
Branch: current working branch
Status: PASS_WITH_HUMAN_REVIEW

## Request

Investigate why weapon-to-world collision was less accurate than limb collision,
make `config/weaponcollisions.json` control editable weapon hitboxes with boxes,
restore knife wall-stop/rebound behavior through the shared collision response,
and add configurable spray gravity in `config/impact_decals.json`.

## Finding

The active actor-triangle solver already sweeps body-part triangles and merges
their contacts into one manifold. Configured weapon capsules were instead
sampled into spheres by `collectBodyWeaponSpheres`, while render-mesh triangles
were fallback-only. This made the weapon contact coarse and round compared with
limbs. The solver's weapon contact response already calls the shared bounce path
with the `weapon` actor label, so the missing piece was accurate JSON geometry,
not a second bounce system.

## Changes

- `src/combat/weapon-collision-config.h/.cpp`
  - Added `WeaponCollisionBoxConfig` and parsing for `source: "boxes"`.
  - `center`, `half_size`, `scale`, and `rotation_degrees` are weapon-local.
  - JSON boxes are generated as local triangles and supplied to the same actor
    triangle solver used by limbs.
- `src/entities/player.h`
  - Added runtime ownership marker `usesJsonMesh`.
- `src/physics/movement/actor-collision-mesh.cpp`
  - Keeps JSON-generated weapon triangles in the actor mesh collection and does
    not replace them with the render-mesh fallback.
- `src/physics/movement/physics-collision-body.cpp`
  - Preserves the JSON box mesh through the per-tick weapon refresh.
- `config/weaponcollisions.json`
  - Changed `spyknife` to two editable boxes: `blade` and `handle`.
  - Existing capsule definitions for other weapons remain unchanged.
- `src/config/impact-decals-config.h/.cpp`, `src/effects/effect-part.h`,
  `src/effects/effect-part-blood.cpp`, and
  `src/effects/effect-part-particles.cpp`
  - Added `blood.spray.gravity` and apply it to newly spawned blood particles.
  - Negative/invalid configured gravity is clamped to zero.
- `config/impact_decals.json`
  - Added `blood.spray.gravity: 2.5` as the preserved default.

## Usage

Example weapon box:

```json
"source": "boxes",
"boxes": [
  {
    "name": "blade",
    "center": [0.25, 0.0, 0.0],
    "half_size": [0.55, 0.20, 0.12],
    "scale": [1.0, 1.0, 1.0],
    "rotation_degrees": [0.0, 0.0, 0.0]
  }
]
```

Increase `blood.spray.gravity` for faster downward drop while keeping the spray
speed high. The value is captured per spawned particle, so reloads affect new
spray without teleporting existing particles.

## Evidence

- JSON syntax check: PASS for `config/weaponcollisions.json` and
  `config/impact_decals.json`.
- `git diff --check`: PASS.
- Repository config self-test: FAIL on pre-existing unrelated weapon/avatar
  schema diagnostics, including `config/weapons.json:comment` and many avatar
  normalization fields. It did not provide focused proof for this change.
- Full executable build: NOT RUN. Two `mimita.exe` processes are running from
  `.dev/builds/0496/mimita.exe`; the build instructions prohibit relinking or
  stopping a running executable. The documented `devscripts/live-build.py` is
  absent in this checkout, and the available hot DLL builder does not include
  these cold collision/config owners.
- Runtime knife wall-stop/rebound and live JSON tuning: pending human review.

## Documents and focused review

Read and applied:

- `docs/ROUTER.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/movement/movement.md`
- `docs/architecture/collision/collision.md`
- `docs/specs/networking/networking.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/architecture/live-development/live-development.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/README.md`

Focused result: `PASS_WITH_HUMAN_REVIEW`. Source and configuration paths are
aligned with the shared fixed-tick actor collision owner; compilation and human
feel acceptance remain outstanding.
