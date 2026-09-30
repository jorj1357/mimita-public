// 2026-09-30T19:07:37Z (display: 2026-09-30 15:07:37 EDT)
/* purpose
* Record moves 2 and 3 of the destructible follow-up plan plus the one-server
* multiplayer replication slice: incremental boolean rebuild, contact surface
* caching, watertight GLB import as destructible objects, physical-entity
* spawn/cut/state/despawn replication, and a deterministic replication
* self-test. Automatic fracture is documented as the next milestone.
*/

# Task

- Summary: Make boolean destruction cheap enough for repeated shots (incremental
  rebuild + a cached contact surface), let arbitrary watertight GLBs become
  destructible objects, and replicate destruction from one server to all
  clients via four new packets. Stop before runtime/human acceptance.
- Status: CODE_COMPLETE / BUILD_VERIFIED / SELFTEST_PASS /
  RUNTIME_VISUAL_AND_MULTIPLAYER_VALIDATION_REQUIRED
- Branch: `afad20a-rebuild`; base commit `61cd8e92`. This builds on the previous
  two uncommitted sessions (Manifold wrapper, mass properties); those were
  preserved, not re-authored.

# Changes

## Move 2 — performance

- `src/impact/boolean-mesh.h/.cpp`: added `booleanSubtractIncremental(sessionId,
  base, cutters)`, `booleanSessionRelease`. A bounded, mutex-guarded session
  cache holds the running in-memory `Manifold`, so a rebuild whose cut history
  only grew subtracts the new cutters instead of replaying all of them. A shrunk
  history (rollback) rebuilds from the canonical base. Behavior is identical to
  `booleanSubtractAll`.
- `src/impact/destructible-geometry.h/.cpp`: `booleanSessionId` owns the session;
  `rebuild` uses the incremental path; `initialize`/`initializeFromMesh` start a
  fresh session; new `release` drops it. `PhysicalEntitySystem::remove`/`clear`
  release sessions so the wrapper does not retain geometry.
- `src/physics/physical-entity.cpp`: `collectActorEntityContacts` caches each
  entity's world-space triangle expansion per `(entity id, geometryRevision,
  transform, triangle count)` and swaps it in/out instead of rebuilding it every
  solver iteration.

## Move 3 — GLB geometry breadth

- New `src/impact/destructible-mesh-loader.h/.cpp`: GL-free GLB import using the
  shared tinygltf accessor helpers. Bakes node transforms, reads POSITION +
  TEXCOORD_0, carries the per-primitive material index, flips inside-out winding,
  recenters to the local AABB, and validates the result as a closed manifold via
  `booleanValidate`. Results (including rejections) are cached per resolved path.
- `src/impact/boolean-mesh.cpp`: `toMeshGL` now emits exact-position merge
  vectors, so a triangle soup with seam-duplicated vertices still imports as one
  manifold without collapsing distinct UVs.
- `src/impact/destructible-geometry.h/.cpp`: new `initializeFromMesh` for an
  authored closed mesh; fills render/collision triangles and mesh-derived mass
  properties immediately (`massFromMesh`). `src/impact/impact-system.h/.cpp`:
  new `initializeEntityFromMesh`.
- New `src/terminal/object-commands.h/.cpp`: `object_spawn <glb> [distance]
  [texture]`. Rejects non-watertight models with the exact reason; registers in
  `src/main-systems.cpp`.

## Multiplayer — one server replicates destruction

- `src/network/packets.h`: `PACKET_PHYSICAL_ENTITY_SPAWN/DESPAWN/STATE` and
  `PACKET_ENTITY_CUT_EVENT`, plus the wire structs and `PhysicalEntitySource`.
  `PROTOCOL_VERSION` 38 -> 39.
- `src/physics/physical-entity.h/.cpp`: `networkId` + `serverDriven`,
  `findByNetworkId`, `addReplicated`, `removeByNetworkId`. `serverDriven`
  entities are skipped by the fixed tick, entity-vs-entity contacts, player push,
  and `ImpactSystem::submit`.
- New `src/network/physical-entity-replication.cpp` +
  `serverReplicatePhysicalEntities`: reliable spawn on first sight, ordered
  reliable cut events as the history grows, 30 Hz unreliable transform state, and
  reliable despawn. Wired into both the listen and dedicated server loops.
- New `src/network/multiplayer-physical-entities.cpp`: client apply functions.
  Spawn rebuilds the same canonical base; cut appends the same ordered cut and is
  idempotent by `(networkId, cutId)`. Branches added to `mpTick`'s `processPacket`.
- `src/network/multiplayer-projectiles.cpp`: the local shooter no longer predicts
  a cut on a server mirror.

## Tests and docs

- New `src/network/destruction-replication-selftest.h/.cpp` +
  `--destruction-replication-selftest` in `src/game/game-cli.cpp`: drives the real
  apply functions, no sockets.
- `src/impact/destructible-selftest.cpp`: authored octahedron import + cut,
  real watertight GLB import (sword1), non-watertight GLB rejection, missing-file
  rejection.
- `docs/specs/manifold-destructible-integration-plan.md`: section 9 marked
  implemented; new 13.4 fracture evaluation; risks, acceptance list, and
  next-agent steps updated.

# Reasoning

- Incremental rebuild preserves the canonical-base + ordered-history truth while
  removing the O(cuts^2) replay cost the previous session flagged.
- GLB import reuses an existing verifier (`booleanValidate`) rather than trusting
  arbitrary assets; reject-and-log keeps invalid geometry out of gameplay.
- Replication sends causes (ordered cuts), not triangles: every client rebuilds
  the identical canonical base, so destruction stays deterministic and cheap,
  matching the plan's section 9. Mirrors are `serverDriven` so a client can never
  become an authority.

# Validation

- Build: `python build.py build-only` -> BUILD SUCCESS (link error during
  development fixed by forward-declaring `PhysicalEntityReplicationState`).
- `mimita.exe --destructible-selftest` -> PASS (35 checks; box/octahedron mass
  properties, imported-mesh cut, GLB import/reject).
- `mimita.exe --moving-crate-selftest` -> PASS (22 checks; no regression from the
  contact-surface cache or `serverDriven` guards).
- `mimita.exe --destruction-replication-selftest` -> PASS (9 checks): mirror
  matches server geometry/volume/mass, re-sent cut ignored, state moves the
  mirror, despawn leaves the authority intact. The self-test initially crashed by
  holding a `PhysicalEntity*` across `addReplicated` (vector reallocation); fixed
  by copying the cut history first.
- `python tools/check-debug-logging.py` -> no new findings in changed files.
- Live visual and two-client multiplayer acceptance were NOT performed.
- Note: `tests/ice-full-server-path-test.cpp` contains a stale
  `static_assert(PROTOCOL_VERSION == 27)`. It is not part of `build.py` and was
  already stale; not touched.

# Pre-existing work

- The Manifold dependency, `boolean-mesh.*`, mass properties, and the handoff doc
  are uncommitted work from the two previous sessions and were extended, not
  re-authored.
- Pre-existing uncommitted edits in `config/accounts/default.json`,
  `config/aimbody.json`, `config/analytics.json`,
  `config/procedural-world/rooms/procedural-mimitasizing5.json`,
  `config/ragdoll.json`, `devscripts/*`, `src/combat/client-collision-world-view.*`,
  `src/entities/aimbody-config.*`, `src/main.cpp`, `src/network/*` (other files),
  `src/procedural/*`, `src/ragdoll/*`, and dated docs under `docs/` were left
  untouched.

# Documents and skills reviewed

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/manifold-destructible-integration-plan.md`
- `docs/specs/moving-physical-objects/moving-physical-objects.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/efficiency-checker-v1.md`

# Human review still needed

- Two real clients on one server: shoot a crate with the projectile rifle and
  confirm both see the same hole at the same place and the `[BOOLEAN]`/`[PHYS
  CLIENT]` logs agree.
- `object_spawn assets/objects/things/cosmetics/sword1.glb` in-game: confirm it
  renders (as generated geometry) and cuts like a crate.
- Long burst near a crate: confirm the incremental rebuild keeps the frame rate
  acceptable; walk near the crate and confirm no rebuild.
- Fracture and dedicated-server crate spawn remain unimplemented by design.