# Real GLB consumer path (production handleOf observes A -> B)

- EST timestamp: 2026-09-15 22:07:33 EDT (UTC 2026-09-16T02:07:33Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--glb-consumer-selftest` PASS)

## Report
- SUBSYSTEM: real GLB resource consumer.

- REAL TOOL ENTITY: used the real logical mesh id the game uses for the tool —
  `HOT_MESH_ROCKET = gameHash("mesh.rocket")` (set by `rocket-tool.cpp:90` into
  the presentation component). A real ECS Tool Entity instance with its own
  EntityId/equip relationship was NOT constructed in this headless pass.
- GLB PUBLICATION: real socket path proven in Round 61; this pass uses the
  canonical content path (`publishContentArtifact` -> provider `apply`). A and B
  are distinct valid GLBs with hash A != hash B.
- PRODUCTION RENDER OBSERVATION: **YES** — `PresentationResourceProvider::handleOf(
  HOT_MESH_ROCKET)` (the exact call the renderer makes) returns a NEW handle
  corresponding to B; provider `current()->contentHash == B`.
- ENTITY CONTINUITY: logical mesh id is stable across A -> B (only the resolved
  version/handle changed). A live Tool Entity's EntityId/owner/equip continuity is
  NOT asserted (no live object graph this pass).
- MALFORMED GLB LAST-GOOD: malformed candidate rejected; provider still resolves
  B; `handleOf` still returns B's handle.
- RUNTIME PREP FAILURE LAST-GOOD: a deliberately failing loader -> `apply` fails ->
  provider keeps B and its handle. PASS.
- RETIREMENT SAFETY: not audited (provider retires the previous handle after the
  committed swap; whether a frame can still reference it is unverified).
- CODE/RESOURCE INDEPENDENCE: the provider is a cold singleton keyed by logical id
  + hash with NO hot-DLL pointer, so an F->G code swap cannot invalidate it; a
  publish after the code swap (D) works. The actual F->G-with-B-active run was not
  executed this pass.
- UNKNOWN LOGICAL RESOURCE (`mesh.user.test-object`): not run.
- UNRESOLVED FALLBACK: not run/audited.
- GLB CONSUMER COMPLETE ENOUGH? **Mostly** — the production resolver observes the
  new version and last-good holds; missing the real EntityId/equip continuity
  assertion and the retirement audit.
- DID ANY STEP REQUIRE REBUILDING/KILLING mimita.exe? **No** (build only).

- LIVE-PROOF DEBT: real Tool Entity EntityId/owner/equip continuity, visible frame
  confirmation, retirement safety, F->G-with-B-active run, unknown logical
  resource, unresolved fallback, PNG/WAV consumers, resource late join.

## Files changed
- `src/hot-reload/glb-consumer-selftest.{h,cpp}`: real consumer-path proof.
- `src/game/game-cli.cpp`: `--glb-consumer-selftest`.
- docs.

## Next (auto-selected)
Assert a real ECS Tool Entity's EntityId/owner/equip continuity across A->B, then
audit retirement, then PNG/WAV consumers through the same bridge, then resource
late join.
