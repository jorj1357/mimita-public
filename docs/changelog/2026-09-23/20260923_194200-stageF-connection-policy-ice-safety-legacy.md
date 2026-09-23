# Stage F: connection policy, ICE callback safety, and legacy marking

Date (UTC): 2026-09-23T19:42:00Z
Status: implemented; cold build and selftests verified; hot provider resolved and exercised

## Scope

Stage F (orchestration/transport bridges). Move the client connection-health and
reconnect-cadence policy hot, add the generation-safe ICE callback drain, and
mark the remaining EXE-owned files LEGACY (kept in place, never deleted).

## Changes

### `net.connection-policy` (client connection health + reconnect cadence)
- New `src/hot-reload/hot-connection-health.h` (POD `GameConnectionHealthV1` +
  `GameReconnectCadenceV1` + shared implementation) and
  `src/hot-reload/modules/connection-policy.cpp`.
- `src/network/multiplayer-packets.cpp`: `mpUpdateConnectionHealth` (next-state
  decision) and `mpTickReconnect` (retry cadence) now resolve the hot policy.
  The transport, teardown, and notifications stay cold.
- The shared implementation's `ConnectionState` numeric values mirror
  `network/connection-state.h` (Connected=6, Reconnecting=7, WeakConnection=10,
  ReconnectFailed=11).

### ICE callback safety (legacy kernel contract)
- `src/network/ice/ice-agent.h/.cpp`: added `quiesceForReload()` (bumps the
  generation-safe callback token and drops queued events) and post-shutdown
  `!mAgent` guards on the state/candidate/gathering handlers. libjuice and its
  thread are untouched.
- `src/network/game-transport.h`: added the `quiesceForReload()` hook and a
  non-owning `MimitaTransport` registry (`registerTransport` /
  `unregisterTransport` / `quiesceAllTransports`).
- `src/network/game-transport.cpp` (new): the registry implementation.
- `src/network/ice-transport.h`: registers/unregisters itself and forwards
  `quiesceForReload()` to the agent.
- `src/hot-reload/hot-reload-system.cpp`: `tryActivateCandidate` calls
  `quiesceAllTransports()` at the safe-point barrier before the module swap.

### Legacy marking (no deletions)
- `src/hot-reload/hot-modules.json`: added a `legacy` array with a `why` for each
  remaining EXE-owned file (`server.cpp`, `ice-agent.cpp`, `packets.h`,
  `server-packets.cpp`, the server/client combat+sim hosts, `multiplayer-*`,
  `reliable-gameplay-events.cpp`). Nothing was deleted or moved out of `cold`.
- `hot-reload-system.h/.cpp`: loads `legacy` sources and watches them for a
  `HOT_RELOAD_LEGACY_CHANGE` relink warning, so an edit is never silently
  ignored.

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T193418.exe`.
- Automated tests (test evidence): `--live-code-selftest` -> PASS with
  `[CAPABILITY_RESOLVED] provider=net.connection-policy` and its check.
- Runtime / human acceptance: pending.

## Stage F status

Done: connection policy is hot; ICE callback safety is in place (libjuice stays
cold); remaining files are marked legacy with reasons. This completes the policy
migration for the 19-file list.
