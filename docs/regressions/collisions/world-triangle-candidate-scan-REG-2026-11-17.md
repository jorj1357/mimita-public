# World triangle candidate scan can fall back to too much work

Date: 2026-11-17
Time last updated: 2026-11-17T16:00:00Z
Status: REPORTED / NEEDS CURRENT TRACE
Owner: collision broadphase / `appendChunkTrianglesForAABB`

Related:

- `docs/architecture/collision/collision.md`
- `docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md`
- `docs/regressions/collisions/npc-template-body-REG-2026-11-03.md`

## Symptom

The user reports collision queries checking a large share of world triangles
(for example 10k, 100k, or more) when only a small local set is nearby.

## Existing recorded history

- `physical-objects-collision-REG-2026-10-01.md` records uncached world-triangle
  gathers, scanning every candidate for every body triangle with
  `worldTree=nullptr`, and the missing entity broadphase.
- `npc-template-body-REG-2026-11-03.md` records 125 safe-point candidates and
  same-solid triangle scans.
- `collisions/README.md` records the recurring rule: measure the owner, keep a
  cached broadphase, and bound or remove redundant work.

## Current code path

`appendChunkTrianglesForAABB` uses the persistent world tree only when
`worldCollisionTreesCoverAll(world)` is true. Otherwise it can fall back to the
chunk path or a full triangle scan. A small actor query can also retain many
large-triangle candidates whose AABBs span the query even when their oriented
prisms do not.

## Current evidence

The 2026-11-17 client weapon-collider bug produced a `~452 m` actor query box
and `2242` candidates. The weapon transform guard reduced the same query to
`2` candidates. That proves the query box must be measured with candidates; it
does not yet prove a full-world fallback scan is happening now.

## Required next evidence

- Log world-tree vs fallback selection from `appendChunkTrianglesForAABB`.
- Log query box size, candidate count, rejected-by-OBB count, and unique
  triangles for one representative tick per second.
- Compare candidate counts to the local query volume.
- Confirm whether a stale/mismatched world tree forces the fallback.

## Acceptance

- A small local query never scans the full world triangle set.
- Candidate count scales with local geometry, not total map size.
- A bounded diagnostic proves the chosen path in `events.jsonl`.
