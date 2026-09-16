# Tool render-path trace + logical-resource registry collision (audit)

- EST timestamp: 2026-09-15 21:32:27 EDT (UTC 2026-09-16T01:32:27Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `AUDIT` (no new runtime claim; GLB in-world consumer NOT proven this pass)

## Report
- SUBSYSTEM: hot resource consumer — render-path trace.

- REAL TOOL RENDER PATH (traced)
  - `PresentationState.meshResourceId` is the authoritative identity (a logical
    id, e.g. `HOT_MESH_*`), set in `presentation-entities.cpp` (L111/L180/L335/L410)
    and carried in the presentation command (`command.meshResourceId`,
    `presentation-render.cpp:658`).
  - Resolution happens at USE via
    `PresentationResourceProvider::instance().handleOf(meshResourceId)`
    (`presentation-render.cpp:631,660,690`; `presentation-entities.cpp:318`).
    So the render path ALREADY keeps the logical id and resolves a concrete
    handle at use — the required invariant largely holds.

- RESOURCE RESOLUTION
  - per frame / per use? YES — `handleOf(logicalResourceId)` is called on the draw
    path, not stored permanently as identity.
  - version-aware? PARTIALLY — `PresentationResourceProvider` tracks
    `{logicalId, contentHash, generation}` and has `apply(logicalId, contentHash)`
    (same hash = no-op), `current`, `handleOf`, `generationOf`, and
    loader/retire callbacks (`project/presentation-resource.h`).
  - raw-handle caching? Handles are cached inside the provider keyed by logical id
    and refreshed by `apply`; the render path does not cache a raw pointer as
    identity.

- **ARCHITECTURE COLLISION (important):** a generic logical-resource resolver
  ALREADY EXISTS — `MimitaRuntime::PresentationResourceProvider`
  (`project/presentation-resource.h`) — with logicalId -> contentHash -> opaque
  handle + loader/retire. My Round 60/61 `MimitaRuntime::ResourceRegistry`
  (`hot-reload/content-artifact.h`) DUPLICATES it. Per the "one owner / do not
  create a second mesh registry" rule this must be UNIFIED, not paralleled:
  the `ContentArtifactV1` descriptor + wire should drive
  `PresentationResourceProvider::apply(logicalId, contentHash, ...)`, and the
  content-artifact layer should keep only descriptor/version/last-good semantics
  (or the two should be merged).

- GLB SOCKET PUBLICATION: A/B hashes, real transport, real prepare, real publish
  were proven at the registry/wire level (Round 61). The in-world path is NOT
  proven this pass.
- REAL CONSUMER RESULT: **not proven** — the render path was not yet routed
  through the unified resolver with a live published hash.
- ENTITY CONTINUITY (Tool EntityId / actor / equip / tool state): **not asserted**
  this pass.
- MALFORMED GLB LAST-GOOD: proven at registry level; **not** in the real render path.
- RESOURCE RETIREMENT SAFETY: not audited (`PresentationResourceProvider` has a
  retire callback; whether a frame can still reference a retired handle is
  unverified).
- CODE GENERATION + RESOURCE SAFETY (F->G with B active, D after G): not run.
- UNKNOWN LOGICAL RESOURCE TEST (`mesh.user.test-object`): not run.
- UNRESOLVED FALLBACK: `Provider::handleOf` behavior when unresolved was not
  audited (placeholder/skip vs null).

- GLB CONSUMER COMPLETE ENOUGH? **No.**
- KEY QUESTION — "Can a hot-created unknown Tool use an unknown logical mesh
  without rebuilding mimita.exe?" — **Likely YES in principle** (logical-id
  render path + generic provider + content wire exist), but **not proven**; the
  exact first boundary to verify is the unification seam: a `ContentArtifactV1`
  publication must flow into `PresentationResourceProvider::apply` so the existing
  `handleOf` resolution observes the new hash. No rocket-specific branch was
  found in the resolution call sites.

- COLD-REBUILD MATRIX (this round): EDIT HOT CPP **NO** (proven earlier); others
  NOT MEASURED this pass.
- DID ANY TEST REQUIRE REBUILDING/KILLING mimita.exe? **No** (build only).
- NEXT LARGEST REAL COLD BOUNDARY: the duplicate-resource-registry seam. Unify
  `ContentArtifactV1`/`ResourceRegistry` with `PresentationResourceProvider`
  (`apply`/`handleOf`), then prove the live GLB swap + malformed last-good +
  F->G-with-B-active + unknown `mesh.user.test-object`.

- LIVE-PROOF DEBT: in-world GLB consumer change, entity/equip continuity under
  publication, retirement safety, unresolved fallback, PNG/WAV consumers, resource
  late join, rocket/unknown-tool falsifications.

## Files changed
- docs only (this is an audit/trace pass; no runtime change).

## Next (auto-selected)
Unify the content-artifact primitive with the existing
`PresentationResourceProvider` (single logical-resource owner); route descriptor
publication through `apply(logicalId, contentHash)`; then prove a live GLB swap on
an equipped Tool Entity (same EntityId/equip), malformed GLB last-good in the real
render path, F->G with the resource active, and an unknown `mesh.user.test-object`.
