# ICE transport reconnect rebuild

## Task

- Task ID: 2026-11-08-ice-reconnect-rebuild
- Summary: Rebuild a lost ICE transport for an established session and resume the
  same authoritative player slot with the retained reconnect token.
- Status: Implemented, compiled, not runtime-verified.
- Date, time, timezone: 2026-11-08T17:24:00Z (2026-11-08 12:24 EST)
- Branch: checkpoint/pre-live-kernel-20261108
- Base commit: 760f2b82
- Final commit: not committed

## Pre-existing changes

- The working tree already contained concurrent ownership/module work from
  another session: module ownership manifest, network-policy consolidation,
  NPC policy split, dev-loop/build changes, and their changelog.
- Files created or modified by this session:
  - `src/network/multiplayer-context.h`
  - `src/network/multiplayer-packets.cpp`
  - `src/network/multiplayer-tick.cpp`
  - this changelog
- All other modified/untracked paths in `git status` are pre-existing concurrent
  work and are not claimed by this session.

## Requested behavior

The client must not keep retrying a dead ICE transport. When an established
session loses connectivity, the client must rebuild ICE, then restore the same
player slot with the valid reconnect token instead of starting an unrelated
fresh join.

## Specification alignment

- `docs/specs/networking/networking.md` section 60 requires reconnect to retain
  temporary session state, attempt the connection again, let the server reserve
  the player slot temporarily, restore it with a valid reconnect token, and
  resume authoritative state.
- Section 61 requires bounded reconnect attempts with backoff.
- The loading/reconnect presentation contract in the same specification reserves
  Reconnecting for an established session that the connection-health owner has
  judged lost; this change preserves that state while the replacement transport
  is negotiated.

## Exact implementation changes

### `src/network/multiplayer-context.h`

- Added `reconnectTransportPending`, `reconnectTransportPlayerId`,
  `reconnectTransportToken`, and `reconnectTransportRoomCode` so the old player
  identity survives the transport teardown.
- Declared `mpIceReconnectStart(MultiplayerContext&, playerName)`.

### `src/network/multiplayer-packets.cpp`

- Split the existing ICE start body into `startIceConnectInternal`.
- `mpIceConnectStart` keeps its previous fresh-join behavior.
- `mpIceReconnectStart` finds the current room, then tears down only the old
  transport/session while preserving reconnect token, player ID, grace deadline,
  disconnect time, and backoff.
- `mpInstallIceConnectSuccess` detects a pending reconnect, restores the player
  ID/token, clears the fresh join token, stays in Reconnecting, and immediately
  sends ReconnectRequest over the new ICE transport.
- `mpTickReconnect` does not send through a torn-down transport while the ICE
  replacement job is active.
- The connection-health transition to Reconnecting now chooses full ICE rebuild
  when the session used ICE; direct UDP keeps the existing reconnect path.
- ICE reconnect failure clears pending identity fields and reports
  ReconnectFailed instead of pretending a fresh connection failed.

### `src/network/multiplayer-tick.cpp`

- The async ICE failure handler distinguishes a reconnect failure from an
  initial join failure, keeps the honest ReconnectFailed state, and clears the
  preserved transport-reconnect identity.

## Diagnostics

- New owner events:
  - `network.ice-reconnect.start`
  - `network.ice-reconnect.transport-installed`
- Existing owner events now carry `reconnect=true` for failed ICE replacement.
- Events are emitted once per attempt/install, not per frame.

## Validation

- Targeted C++ compile:
  - `src/network/multiplayer-packets.cpp`: PASS
  - `src/network/multiplayer-tick.cpp`: PASS
- `git diff --check`: PASS
- Build artifact inspected for new strings:
  - `network.ice-reconnect.start`: present
  - `network.ice-reconnect.transport-installed`: present
  - `Reconnecting via ICE`: present
- Runtime attempt:
  - Manual headless server and direct client were started to exercise the real
    executable.
  - The ICE room-code client probe could not find the manually created room
    (`network.ice-room-lookup exists=false`), so the replacement-transport path
    was not reached.
  - No runtime acceptance is claimed.

## Measured evidence

- Prior failing run `20261108_155202` stopped receiving snapshots around
  `snapshot_tick=14045` while the server continued to `42840`; reconnect then
  failed after the 60 s grace.
- No after-fix runtime measurement exists yet.

## Regression review

- Regression entry appended: no.
- This is a fix for an observed failure, not a newly confirmed regression of
  previously working behavior.
- Related failure evidence remains in the run journals named above.

## Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: not performed.
- Still unverified: real ICE transport loss followed by successful replacement
  transport install and ReconnectAccept, including gameplay snapshot recovery.
