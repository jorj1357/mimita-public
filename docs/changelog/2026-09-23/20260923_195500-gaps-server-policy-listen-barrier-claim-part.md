# Gaps 1-3: server startup policy, listen-thread barrier, claim-part policy

Date (UTC): 2026-09-23T19:55:00Z
Status: implemented; cold build and selftests verified; hot providers resolved and exercised

## Scope

Close the three remaining gaps from the Stage G closeout. Nothing deleted.

## Gap 1 — `server.cpp` startup policy hot (`net.server-policy`)
- New `src/hot-reload/hot-server-policy.h` (POD `GameServerModeV1` +
  `GameServerStartupNpcV1` + shared implementation) and
  `src/hot-reload/modules/server-policy.cpp`.
- `src/network/server.cpp`: the duel-vs-community mode selection (`useDuel`,
  `startMatch`) and the startup NPC count + spawn-point-vs-fallback choice are now
  hot. The launch options, world, stores, and socket loop stay cold.
- Note: the generation announce/quorum/switch block is deeply coupled to cold
  packet structs and `HotReloadSystem` handles; it remains cold mechanism. The
  policy-shaped startup decisions are now hot.

## Gap 2 — listen-thread hot-reload barrier
- `src/hot-reload/hot-reload-system.h/.cpp`: added `setExternalTickOwner`,
  `hasExternalTickOwner`, and `pollAndAdvanceFromTickOwner`. When a listen
  server's background thread owns activation, the main render thread's
  `pollAndAdvance` is a no-op; only the tick-owner call proceeds.
- `src/network/server.cpp`: `simulateOneServerTick` calls
  `pollAndAdvanceFromTickOwner` at the top of the fixed tick (the safe point, on
  the thread that runs every hot call). `startListenServer`/`stopListenServer`
  set/clear the tick owner around the background thread lifetime.

## Gap 3 — attack claim-part policy hot (`net.claim-part`)
- `src/hot-reload/hot-attack-claim.h`: added `GameClaimPartV1` + shared
  `resolvePart` (client-supplied head/leg preserved; otherwise derived from the
  matched box). Registered from `modules/attack-claim-policy.cpp`.
- `src/network/server-attack.cpp`: `claimedHitInBodyParts` now resolves the
  claimed part through the hot policy; the body-part box test stays cold geometry.

## Evidence

- Build (source/build evidence): `python build_agent.py` -> `Status: SUCCESS`,
  `mimita-20260923T195214.exe`.
- Automated tests (test evidence), all PASS: `--live-code-selftest` (with new
  `net.server-policy` checks and `[CAPABILITY_RESOLVED]` for
  `net.server-policy` and `net.claim-part`), `--packet-codec-selftest`,
  `--snapshot-chunk-selftest`, `--movement-selftest`,
  `--movement-parity-selftest`, `--transport-generation-selftest`,
  `--lagcomp-history-selftest`, `--hot-authoritative-selftest`,
  `--actor-lifecycle-selftest`, `--npc-entity-selftest`.
- Runtime smoke: `--udp-echo` binds and runs; server/listen boot is exercised by
  the existing lifecycle (full ICE boot needs a coordinator).
- Human/live acceptance: pending.

## Honest remaining

- The `server.cpp` generation announce/quorum/switch block stays cold mechanism.
- Physical-contact contact-detection geometry stays cold (only damage/knockback/
  interval/confirm policy is hot; detection is the collision primitive itself).
- Human two-client live reload proof still pending.
