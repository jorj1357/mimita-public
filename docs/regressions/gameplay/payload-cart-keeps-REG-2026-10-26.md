# Payload cart keeps the previous map route

## Expected behavior

When Payload changes maps, the cart uses that map's authored origin, route,
dimensions, facing, and interaction box. It must not travel along a route from
the map that was active before the change.

## Actual behavior

The server reloaded the new map JSON and updated cart origin, size, facing, and
interaction box, but retained `PayloadRuntime::routePoints` from the previous
map. The cart could therefore move to unrelated locations on the new map.

## Cause

`serverPayloadTick` initialized `PayloadRuntime` only when a match started.
`serverGamemodeTick` polled map JSON after a map change, but the live runtime
copied only facing and yaw, leaving its route cached.

## Wrong and corrected code

- Wrong: map reload updated cart-facing fields while retaining the runtime
  route initialized by the previous map.
- Corrected: `ServerGamemodeState::payloadRuntimeMapId` identifies the map
  represented by the runtime. `payloadSyncCartFromMap` reinitializes the
  runtime and respawns the physical cart when the active map changes. On the
  same map, origin, size, facing, and interaction-box transforms update live;
  a size change recreates the physical body and its replicated client mirror.

## Evidence

- Before the correction, run `20261026_233539` logged
  `gamemode.match.configured` for `payload1`, then loaded `payloadsmall1` and
  logged `payload.cart-map-reloaded` while the runtime still held the prior
  route. The old cart initialized at `[3,3,2]`; the new authored map cart was
  recreated at `[10,20,10]`. Client mirror spawn events confirmed that physical
  dimensions replicated.
- Build 3391 compiled the correction successfully. Its `--versioninfo` journal
  is `logs/10-26-2026/20261026_194631/events.jsonl`.
- The live run was not restarted after build 3391 because a separate active
  game/editor session was changing `payloadsmall1.json`. Corrected route use,
  visual cleanup of the old cart, and human acceptance remain unverified.

## Files

- `src/network/server-gamemode.h`: add the runtime map identity.
- `src/network/server-gamemode.cpp`: reinitialize on map switch and synchronize
  live cart geometry and transforms from the current map configuration.
- `docs/specs/gamemodes/payload.md`: document the hot-reload boundary.
- `docs/changelog/2026-10-26/20261026_234627-payload-cart-map-route-sync.md`:
  work record.
