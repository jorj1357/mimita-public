# A1: hot snapshot-chunk codec behind `net.snapshot-codecs`

Date (UTC): 2026-09-23T18:05:00Z
Status: implemented; cold build and selftests verified; hot provider resolved and exercised

## Scope

Stage A1 of the live-networking migration. Move the snapshot-chunk
quantize/encode/parse/reassemble policy out of cold code and behind the generic
`net.snapshot-codecs` capability, using the repo's established header-only
shared-implementation pattern so there is exactly one codec implementation
serving both the cold EXE fallback and the hot provider.

## Changes

1. **New `src/hot-reload/hot-snapshot-codec.h`**: the POD capability interface
   (`GameSnapshotBuildV1`, `GameSnapshotParseV1`, `GameSnapshotReassembleV1`,
   `GameSnapshotCodecV1`, `GameSnapshotCodecLookupFn`) and the ONE shared
   implementation (`MimitaNet::HotSnapshotCodecImpl`) of quantize, compact,
   decompact, build, parse, and reassemble. All capability buffers are
   caller-owned; no STL crosses the boundary. Header-only so cold and hot
   compile the same code (same pattern as `pellet-pattern.h`/`hitscan-model.h`).

2. **New hot module `src/hot-reload/modules/snapshot-codec.cpp`**: registers the
   `net.snapshot-codecs` capability provider (signature
   `sig.net.snapshot-codecs.v1`) and serves the shared implementation. It is
   picked up by the existing `modules/*.cpp` glob; no aggregator or EXE call
   site changed.

3. **`src/network/snapshot-chunks.cpp` rewritten as the cold bridge**: the
   public API is unchanged, but `buildSnapshotChunks`, `parseSnapshotChunk`, and
   `reassembleSnapshotChunks` now resolve `GAME_CAP_SNAPSHOT_CODECS` through
   `GenericRuntime::capability` and prefer the active hot provider; when no hot
   package is loaded (headless selftests, early startup) they run the shared
   implementation directly. Quantize/compact public helpers delegate to the
   shared implementation. `clearSnapshotPacket`,
   `appendSnapshotChunkToPacket`, and `runSnapshotChunkSelfTest` are unchanged.
   Also fixed a latent over-read in `parse` (the old `memcpy(&out, data, bytes)`
   could copy up to 1200 bytes into a 1128-byte struct for a malformed oversized
   datagram); the copy is now clamped to `sizeof(SnapshotChunkPacket)` and the
   existing wire-size check still rejects it.

4. **`src/live-code/live-code-selftest.cpp`**: added three checks that resolve
   the hot snapshot codec through the same generic doorway and run a real
   build/parse round trip: "hot snapshot-codecs provider resolves a live codec",
   "hot snapshot codec builds a chunk", "hot snapshot codec parses a chunk".

5. **`src/hot-reload/hot-modules.json`**: added
   `src/hot-reload/hot-snapshot-codec.h` to `headers` so editing it hot-rebuilds
   the DLL. `snapshot-chunks.cpp` stays in `cold` as a mechanism-only bridge
   (see Not done).

## Evidence

- Source changes: `src/hot-reload/hot-snapshot-codec.h` (new),
  `src/hot-reload/modules/snapshot-codec.cpp` (new),
  `src/network/snapshot-chunks.cpp`, `src/live-code/live-code-selftest.cpp`,
  `src/hot-reload/hot-modules.json`.
- Build (source/build evidence):
  - Hot DLL: `python build_game_dll.py` -> `DLL build success`
    (`build/mimita-game.dll`), package now reports `providers=11 requirements=6`.
  - Cold EXE: `python build_agent.py` -> `Status: SUCCESS`,
    `mimita-20260923T175931.exe`.
- Automated tests (test evidence), all on `mimita-20260923T175931.exe`:
  - `--snapshot-chunk-selftest` -> PASS (cold fallback path).
  - `--live-code-selftest` -> PASS, including the three new hot-codec checks;
    `[CAPABILITY_RESOLVED] provider=net.snapshot-codecs`.
  - `--transport-generation-selftest` -> PASS (uses build/parse directly).
  - `--packet-codec-selftest` -> PASS.
- Runtime / human acceptance: pending (no live game edit observed this session).

## Not done

- `src/network/snapshot-chunks.cpp` is still listed in the manifest `cold` block.
  It is now a mechanism-only bridge (dispatcher + fallback + test helpers) with
  no codec policy, so it can be reclassified/relocated later. Removing it from
  `cold` requires deleting the cold fallback, which needs the headless selftests
  to load the hot DLL first (the selftest runs before any package is active).
- A2 (movement-validation hot move) is the next stage.

## Notes / hazards

- **Shared `build/obj` collision (observed).** `build_game_dll.py` (default mode)
  and the cold `build.py` both compile into `build/obj`. Running
  `build_game_dll.py` standalone immediately before `build_agent.py` leaves
  `-DMIMITA_GAME_DLL` objects that the cold link then picks up, producing
  undefined references to hot-only symbols (e.g. `hotEmitRecipeSound`) and a
  failed link. The normal `build_agent.py` flow is safe because it links the EXE
  before it builds the DLL. Workaround used: delete the hot-compiled objects
  (`build/obj/*hot-reload*`, `*effects__effect-part*`) and rebuild. This is a
  pre-existing build-system hazard, not a regression from this change.
