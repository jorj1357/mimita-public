// 2026-09-30T19:26:20Z (display: 2026-09-30 15:26:20 EDT)
/* purpose
* Record the first automatic-fracture implementation: a cut that disconnects or
* unbalances a destructible object splits it into independent rigid bodies that
* inherit material, mass, and motion, and that replicate deterministically to
* clients. Deliberately a best attempt: the balance heuristic and budgets are
* code-tuned and need runtime human acceptance.
*/

# Task

- Summary: Implement automatic fracture inspired by
  `docs/specs/destructible-world/destructible-world.md` sections 13/14/20/21/24/
  27/28/31/44. Trigger on disconnected components or an off-center remaining
  piece; spawn detached pieces as Dynamic bodies; replicate deterministically.
- Status: CODE_COMPLETE / BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VISUAL_AND_MULTIPLAYER_VALIDATION_REQUIRED (fracture tuning pending)
- Branch: `afad20a-rebuild`; base commit `61cd8e92`. Builds on the previous
  uncommitted sessions; those were preserved.

# Changes

## Wrapper (`src/impact/boolean-mesh.{h,cpp}`)

- `BooleanPiece` + `booleanDecomposePieces`: reconstructs the canonical base
  minus the full cut history and returns each positive-volume shell as a closed
  mesh with volume and centroid, largest first. Interior cavities are skipped.
- `buildBooleanBoxMeshAt` (offset box) and `booleanUnion`: author compound
  closed bases (needed for a thin-neck test shape and generally useful per
  `destructible-world.md` 6.2's `addBox`/`addSphere`).

## Geometry owner (`src/impact/destructible-geometry.{h,cpp}`)

- `FractureReason` (None / DisconnectedComponent / UnbalancedSupport),
  `FractureDecision`, and `FractureTuning` (aggressive defaults).
- `evaluateFracture`: component disjointness, plus an unbalanced single-piece
  trigger (`maxRemainingFraction`, `comOffsetFraction` on the horizontal
  center-of-mass offset).
- `DestructibleGeometrySystem::decomposePieces` wraps the wrapper decomposition.
- `rebuild` records the fracture decision on `DestructibleGeometry`
  (`lastFractureReason`, `lastImbalance`).

## Impact owner (`src/impact/impact-system.{h,cpp}`)

- New public `ImpactSystem::applyFracture(entity, result, serverDriven)` as the
  single fracture owner: keeps `pieces[0]` on the hit entity (stable id/network
  id), spawns the rest as Dynamic entities that inherit material/density/friction/
  restitution/damping/texture/collidesWithActors and mesh-derived mass, and
  seeds velocity `v + omega x (r - com)` at each centroid. Deterministic
  fragment network ids `0x40000000 | (parent << 4) | pieceIndex`.
- `submit` calls it after a successful cut when the rebuild flagged fracture;
  `ImpactResult` gained `fractured`, `fragmentCount`, `fragmentEntityIds[]`.

## Networking (`src/network/`)

- `mpProcessEntityCutEventPacket` runs the same `applyFracture(..., true)` after
  applying a replicated cut, so client mirrors match the server without sending
  triangles (spec section 28). Children are `serverDriven` mirrors.

## Tests and docs

- `src/impact/destructible-selftest.cpp`: dumbbell base (valid client of
  `booleanUnion`), uncut-does-not-fracture, neck-cut triggers component fracture,
  decompose ordering, off-center cut triggers support fracture, and an
  end-to-end dynamic fracture (children are Dynamic with mass/collision, primary
  keeps the material).
- `src/network/destruction-replication-selftest.cpp`: server fractures a
  dumbbell; the client applying the same ordered cut reproduces the piece with
  the same deterministic network id, serverDriven.
- `docs/specs/manifold-destructible-integration-plan.md`: 13.4 rewritten from
  design to implemented, with the tuning limits called out; risks and next-agent
  steps updated.

# Reasoning

- Spec-aligned trigger: `destructible-world.md` 20 (detached connected regions)
  and 21/24 (support and center of mass) say a disconnected or unsupported region
  becomes a moving physical object.
- Reuse the boolean's existing `Decompose()` output instead of a new geometry
  source; mass/COM/inertia already have an owner (13.1).
- Replicate the cause (the cut), not the pieces (28): both sides derive identical
  pieces from identical inputs, so fracture costs no new packet.
- Budget and safety (`44`): cap fragments per event, keep tiny slivers welded,
  keep the hit entity's identity stable.

# Validation

- Build: `python build.py build-only` -> BUILD SUCCESS.
- `mimita.exe --destructible-selftest` -> PASS (all checks incl. fracture).
- `mimita.exe --moving-crate-selftest` -> PASS (no regression).
- `mimita.exe --destruction-replication-selftest` -> PASS (incl. fracture
  reproduction by deterministic network id).
- `python tools/check-debug-logging.py`: new files use `printf` like the rest of
  `src/network`; no new findings in `src/impact/`.
- Live visual and two-client acceptance were NOT performed. `FractureTuning`
  (`comOffsetFraction = 0.12`, `maxRemainingFraction = 0.98`) is intentionally
  aggressive and is the first thing to retune.

# Pre-existing work

- Manifold vendoring, the wrapper, mass properties, incremental rebuild, GLB
  import, and replication are uncommitted work from previous sessions and were
  extended, not re-authored.
- Pre-existing uncommitted edits across `config/*`, `devscripts/*`,
  `src/combat/client-collision-world-view.*`, `src/entities/aimbody-config.*`,
  `src/main.cpp`, `src/network/*` (other files), `src/procedural/*`,
  `src/ragdoll/*`, and dated docs under `docs/` were left untouched.

# Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/destructible-world/destructible-world.md`
- `docs/specs/manifold-destructible-integration-plan.md`
- `docs/specs/moving-physical-objects/moving-physical-objects.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`

# Human review still needed

- Spawn a crate, shoot repeated holes on one side, and confirm it eventually
  breaks apart rather than only denting — and that a single shot does NOT.
- Confirm the detached pieces fall, collide, and look like the source material.
- Confirm a dumbbell-shaped GLB (`object_spawn`) severs when shot through its
  neck.
- Two clients must see the same pieces in the same places.
- Retune `FractureTuning` from the observed feel; it is the intended next step.