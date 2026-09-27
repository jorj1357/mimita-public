# One canonical resource authority (provider unification)

- EST timestamp: 2026-09-15 21:38:18 EDT (UTC 2026-09-16T01:38:18Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--content-resource-selftest` 20/20; transport content section PASS)

## Report
- SUBSYSTEM: logical-resource authority unification.

- RESOURCE AUTHORITY AUDIT
  - `PresentationResourceProvider` (pre-existing, used by the real render path):
    owns logicalId -> contentHash/generation -> opaque handle, loader + retire
    callbacks, last-good on loader failure, atomic swap. Resolved at use via
    `handleOf(logicalId)`.
  - `ResourceRegistry` (introduced Round 60): had independent active/candidate/
    last-good state — DUPLICATE authority.

- DUPLICATION REMOVED?
  - ResourceRegistry role after change: transport/acquisition METADATA only
    (announceCandidate, pendingLogicalIdForHash, candidateHashOf, descriptorOf,
    acknowledgePublished). `resolve` is a thin read of the provider.
  - PresentationResourceProvider role: the SINGLE authoritative publication
    authority (active mapping, handle, last-good, retirement).
  - single source of truth? YES — asserted by the "one canonical authority" test.

- CONTENT -> PROVIDER SEAM: `publishContentArtifact(logicalId, contentHash,
  bytes, size, err)` = supersede check -> hash verify -> kind validator ->
  `PresentationResourceProvider::apply(logicalId, contentHash)`. Transport/cache
  remain ignorant of rendering; no OpenGL/mesh coupling in the transport.

- STALE/SUPERSEDE SEMANTICS: preserved — a publish for a superseded candidate hash
  is rejected ("not the advertised candidate"); newer candidate publishes.
- LAST-GOOD SEMANTICS: owned solely by the provider now; malformed candidate ->
  validator rejects before `apply` -> provider `current` stays B; loader failure
  would keep the previous generation too.

- REAL GLB TOOL SWAP: **not proven in-world this pass.** The seam is now unified:
  the production render path (`handleOf(meshResourceId)`) resolves the SAME
  authority content publication writes to, so a published B is observable by the
  production resolver. A hash A/B, real-socket path, provider apply, and
  production `handleOf` observed B on an actual equipped Tool Entity remain to be
  asserted in the live object graph.

- ENTITY CONTINUITY (Tool EntityId / owner / equip / tool state): not asserted.
- MALFORMED GLB LAST-GOOD: proven at the canonical-provider level; not yet in the
  real render path.
- RETIREMENT SAFETY: provider retires the previous handle on committed swap; a
  frame may still reference it — retirement/fence audit NOT done.

- CODE/RESOURCE INDEPENDENCE: not run this pass (F->G with B active; D after G).
- UNKNOWN LOGICAL RESOURCE (`mesh.user.test-object`): not run.
- UNRESOLVED FALLBACK: not audited (`handleOf` on an unresolved id).

- GLB CONSUMER COMPLETE ENOUGH? **No** — unification complete; in-world swap proof
  remains.
- DID ANY STEP REQUIRE REBUILDING/KILLING mimita.exe? **No** (build only).

- LIVE-PROOF DEBT: live Tool Entity GLB swap + entity/equip continuity, malformed
  last-good in the real render path, retirement audit, F->G with resource active,
  unknown logical resource, unresolved fallback, PNG/WAV consumers, resource
  late join.

## Files changed
- `src/hot-reload/content-artifact.h`: ResourceRegistry shrunk to transport
  metadata; `publishContentArtifact`/`publishContentArtifactFromCache` bridge;
  `resolve` reads the provider.
- `src/network/multiplayer-tick.cpp`: content publication via the bridge.
- `src/hot-reload/content-resource-selftest.cpp`: provider-based assertions.
- `src/network/transport-generation-selftest.cpp`: provider-based content checks.
- docs.

## Next (auto-selected)
Prove the live GLB swap on an equipped Tool Entity (same EntityId/owner/equip) with
production `handleOf` observing B, malformed last-good in the real render path,
retirement safety, then F->G with the resource active; then PNG/WAV consumers
through the SAME bridge; then resource late join.
