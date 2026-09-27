# Remote artifact install into the real hot loader

- EST timestamp: 2026-09-15 19:43:13 EDT (UTC 2026-09-15T23:43:13Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (`--artifact-install-selftest` PASS; full suite 40/40)

## Report
- SUBSYSTEM: distributed hot-code finalization — remote artifact install.

- REMOTE ARTIFACT INSTALL
  - cache -> loader path: verified `ArtifactCache` bytes ->
    `HotReloadSystem::installCandidateArtifact` -> immutable generation-specific
    staged DLL -> the SAME `loadCandidateFromFile` path used by a local build
    (API/ABI + self-test) -> real inactive candidate. No `RemoteHotLoader`.
  - real codeLoaded: yes — proven with the actual `build/mimita-game.dll`
    (16,845,661 bytes): it installs and `hasInstalledCandidate()` is true. A
    tampered artifact is rejected by the hash gate before any load.
  - same local/remote candidate representation: yes — the installed record is a
    `GenerationRecord` (module, api, package descriptor); the switch transaction,
    migration plan, and activation apply unchanged. A coordinated SWITCH at the
    agreed tick activates either a locally built or a remote-installed candidate.
  - client wiring: after artifact commit the client installs the bytes; late-join
    bootstrap `codeLoaded` now comes from the real install, and coordinated READY
    requires the install to have succeeded.

- FULL PRODUCTION LOOP: not executed end-to-end yet (ANNOUNCE/MANIFEST/artifact/
  install/verify/migration/READY/quorum/SWITCH are individually real and
  transport-proven; the single production-loop run remains).
- SESSION CONTINUITY (process/connection/match/EntityIds): not asserted in a live
  run.
- RAW CPP LIVE PROOF: none (no live two-process run this pass).
- MULTI-FILE LIVE PROOF: none.

- DISTRIBUTED CODE COMPLETE ENOUGH? **Not yet** — the remote install seam is
  closed (no more `codeLoaded == false` test-only gap); the full production-loop
  run and the live/two-process raw-cpp proof remain.

- DISTRIBUTED CONTENT: not started. Plan unchanged: `ContentArtifactV1
  { logicalId, kind, contentHash, byteSize, generation }` over the SAME
  ArtifactCache/Streamer/Receiver + hashing; kind-specific validation only
  (PNG -> GLB -> WAV -> shader/animation/font); logical-id mapping A->B with
  per-resource last-good; presentation-only resources publish independently,
  simulation-semantic resources coordinate a tick.

- Changeset / world-map live editing / persistence / multi-user authoring: not
  started.

- CURRENT COLD-RESTART AUDIT: no new cold boundary introduced; the remote code
  payload no longer requires a rebuild path (it installs through the loader).

- NEXT LARGEST COLD OWNER: the full production-loop F->G run + live two-process
  raw-cpp proof (verification, not new architecture), then `ContentArtifactV1`.

- LIVE-PROOF DEBT: production-loop F->G, same-session/entity assertion,
  two-process/raw-cpp, and all content-generation proofs.

## Files changed
- `src/hot-reload/hot-reload-system.{h,cpp}`: `installCandidateArtifact`, installed
  candidate state, coordinated activation of installed candidates.
- `src/network/multiplayer-tick.cpp`: install verified bytes; bootstrap/READY use
  the real install result.
- `src/hot-reload/artifact-install-selftest.{h,cpp}`: real-DLL install proof.
- `src/game/game-cli.cpp`: `--artifact-install-selftest`.
- docs.

## Next (auto-selected)
Full production-loop F->G transaction over the real server/client loops with
same-session/entity assertions and the raw-cpp/two-process proof; then begin
`ContentArtifactV1` with PNG (live generation + malformed last-good).
