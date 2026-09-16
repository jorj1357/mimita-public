# Real ECS tool-entity continuity: trace + plan (no runtime change)

- EST timestamp: 2026-09-15 22:22:54 EDT (UTC 2026-09-16T02:22:54Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `AUDIT` (real Tool Entity continuity NOT proven this pass; no runtime change)

## Report
- SUBSYSTEM: real equipped Tool Entity continuity across a resource version change.

- REAL ECS TOOL ENTITY: **not constructed** this pass. Traced the real production
  path a hot tool uses to create/own its tool entity:
  - `ctx->entityCreate(host, realm, &outEntity)` — generic entity creation
    capability.
  - `ctx->relationshipAdd(host, gameHash("relationship.owns-tool"), userEntity,
    toolEntity, toolKindHash)` — generic relationship capability.
  (see `src/hot-reload/modules/tools/banana-launcher.cpp:55-61`). The rocket tool
  likewise writes `present.meshResourceId = HOT_MESH_ROCKET`
  (`rocket-tool.cpp:90`).

- BEFORE STATE (would be captured next): toolEntityId E, actor EntityId P,
  `owns-tool` relationship P->E, tool logical id, `PresentationState.meshResourceId
  = mesh.rocket`, provider active hash A + handle HA, one gameplay field (ammo or
  cooldown).

- A→B PUBLICATION: the canonical content path is already proven
  (`--glb-consumer-selftest`); not re-run with a real entity attached.
- ENTITY CONTINUITY: **not asserted** (no real Tool Entity this pass).
- PRODUCTION RENDER PATH: `handleOf(HOT_MESH_ROCKET)` observing B is proven in
  isolation; observing B for a specific real Entity E is **not** asserted.
- MALFORMED C / RUNTIME PREP FAILURE: proven in the consumer path (Round 64), not
  with a real entity attached.
- RETIREMENT SAFETY: **not audited.**
- F→G WITH B ACTIVE / D AFTER G: not run with an entity (the provider is a cold
  singleton with no DLL pointer; post-swap publish D is proven).
- UNKNOWN LOGICAL RESOURCE / UNRESOLVED FALLBACK: not run.
- GLB CONSUMER COMPLETE ENOUGH? **No** — real Tool Entity continuity is the
  remaining gate.
- DID ANY STEP REQUIRE REBUILDING/KILLING mimita.exe? **No** (audit only).

- NEXT LARGEST REAL COLD BOUNDARY: none identified here; the remaining work is
  verification (attach a real Tool Entity to the already-proven seam), not new
  architecture.

- LIVE-PROOF DEBT: real Tool Entity EntityId/owner/equip continuity, production
  resolution for that exact entity, retirement safety, F→G with an entity + B
  active, unknown logical resource, unresolved fallback, PNG/WAV consumers,
  resource late join.

## Files changed
- docs only.

## Next (auto-selected)
Build a headless selftest that creates a real Tool Entity + `owns-tool`
relationship via the generic `entityCreate`/`relationshipAdd` capability context,
writes `HOT_PRESENTATION_COMPONENT` (`meshResourceId = mesh.rocket`), then
publishes A→B through the canonical content path and asserts the EntityId,
relationship, and gameplay component are unchanged while the production resolver
(`handleOf`) observes B; then malformed C and F→G with the entity attached.
