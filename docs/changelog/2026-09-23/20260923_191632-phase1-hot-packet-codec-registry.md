# Phase 1: generic hot packet-codec registry

Date (UTC): 2026-09-23T19:16:32Z
Status: implemented; build and selftests verified; live wire dispatch not yet wired

## Scope

Phase 1 of the hot server/networking migration
(`docs/architecture/live-development/hot-kernel-next-steps.md`,
`docs/specs/networking/networking.md`): make a packet schema a hot source edit.
The EXE keeps sockets, framing, buffers, and retention; a hot module owns the
inner schema layout and its encode/decode/validate policy. No new
`GameplayContextV1`/`GameAPI` field was added; the doorway is the existing
generic capability registry.

## Changes

1. **Manifest classification fixed.** `src/network/packets.h` was listed both as
   a hot header and in the `cold` list. It is now cold-only (removed from
   `headers`), because legacy typed wire structs are parsed by cold dispatch
   code; changing them must remain a cold change. New schemas use the hot codec
   registry instead.

2. **Shared hot codec ABI** (`src/hot-reload/hot-packet-codec.h`, new; added to
   the manifest `headers`):
   - `PacketCodecEnvelopeV1` (80-byte, packed) — protocol family, schema id and
     version, payload size, connection id, packet sequence, ack sequence/bits,
     server/client tick, encoding generation, code hash, payload checksum;
   - `packetCodecChecksum` (one FNV-1a implementation shared by EXE and codecs);
   - `PacketCompatibilityV1` explicit reasons (UnknownSchema, VersionTooOld,
     VersionTooNew, PayloadSizeMismatch, ChecksumMismatch, Malformed, Rejected);
   - encode/decode/validate function signatures and
     `GamePacketCodecDescriptorV1` (schemaId, schemaVersion, minSupportedVersion,
     encode, decode, optional validate);
   - `GamePacketCodecLookupFn` and capability ids `net.packet-codecs` /
     `sig.net.packet-codecs.v1`.

3. **Cold dispatch** (`src/network/packet-codec-dispatch.{h,cpp}`, new): resolves
   the hot lookup provider per call (never caches across a generation swap),
   encodes `[envelope][payload]`, verifies envelope/payload size, checksum, and
   version range, and classifies every failure explicitly. It also owns bounded
   **generation retention** (`retainEncoded` / `decodeRetained` / `acknowledge`)
   so a reliable packet encoded by a generation can still be decoded after an
   activation until acknowledged.

4. **Hot registrar + module**: `HotPackageBuilder` gained
   `addPacketCodec`/`findPacketCodec` and a `PacketCodecRegistrar`.
   `src/hot-reload/modules/packet-codecs.cpp` (new, picked up by the existing
   glob) registers one real codec (`packet.hot.ping`, v1) and the
   `net.packet-codecs` capability provider, with a matching capability
   requirement so activation validates the signature.

5. **Tests**: `src/hot-reload/packet-codec-selftest.{h,cpp}` (new,
   `--packet-codec-selftest`): round trip, backward-compatible older version,
   explicit unknown/too-new/too-old/checksum/malformed/policy-rejection reasons,
   generation retention across a swap, and a schema registered after startup.
   `--live-code-selftest` gained a check that the **hot DLL's** codec resolves
   through the same generic doorway.

## Evidence

- Source changes: `hot-packet-codec.h`, `hot-package.h`, `hot-modules.json`,
  `modules/packet-codecs.cpp`, `packet-codec-dispatch.{h,cpp}`,
  `packet-codec-selftest.{h,cpp}`, `game-cli.cpp`, `live-code-selftest.cpp`.
- Build (source/build evidence):
  - Hot DLL: `python build_game_dll.py` -> `DLL build success`.
  - Cold EXE: `python build_agent.py` -> `Status: SUCCESS`
    (`mimita-20260923T151502.exe`). A `mimita.exe` (pid 5640) was running, so
    the uniquely named agent exe was used; it was not touched.
- Automated tests (test evidence):
  - `--packet-codec-selftest` -> PASS (19 checks).
  - `--live-code-selftest` -> PASS, including
    "hot packet-codecs provider resolves a live codec".
  - `--generation-bootstrap-selftest`, `--transport-generation-selftest`,
    `--capability-selftest` -> PASS (no regression).
- Runtime evidence: none. No live two-process packet exchange was run.
- Human acceptance: pending.

## Known, unrelated failures

`--hot-combat-selftest` fails three checks ("phase2 respawn after death
(lifecycle generation)", "phase2 respawn returns to idle", "per-tool phase
drives distinct arm poses"). These are in animation/respawn paths owned by
concurrent working-tree edits (`hot-combat-selftest.cpp`,
`modules/presentation/tool-visuals.cpp`, `npc/npc-avatar.cpp`,
`modules/tools/combat-policy.cpp` are all modified by another session), and the
failing set changed between runs (3 vs 5 failures). The new packet-codec module
only registers a codec and a capability and cannot affect those paths.

## Not done (next)

- The live wire doorway: a generic packet type (e.g. `PACKET_HOT_CODEC`) plus
  additive server/client dispatch branches that hand opaque bytes to
  `PacketCodecDispatch` and route decoded results to hot handlers. Until then the
  registry is proven in-process and in the loaded DLL, but ordinary packets still
  use the legacy typed dispatch.
- Codec-generation retention is not yet connected to the reliable-event queue.
- No live two-process proof that a new schema round-trips over the socket.
