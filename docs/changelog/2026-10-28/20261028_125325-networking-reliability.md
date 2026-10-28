# Networking reliability: staged evidence, join/mode readiness, reconnect and relay

Time created: 2026-10-28T12:53:25Z
Time last updated: 2026-10-28T12:53:25Z

Branch: `20261011zt`
Starting commit: `ada6a0d0`
Completion commit: none (changes left uncommitted for review)

## Request

Implement Plan B networking reliability only: add a tracker entry, bounded
connection/session/mode/NPC-Waves diagnostics, a dev-loop room-code join path,
mode-start readiness gating, duplicate-join and reconnect-token fixes, one-life
NPC Waves actors, and a force-relay test switch. Do not change movement,
collision, flame, animation, or performance owners.

## Documents and skills read

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/networking/networking.md`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/specs/npc-behavior/npc-behavior.md`
- `config/gamemodes/npc_waves.json` (`docs/specs/gamemodes/npc-waves.md` is absent)
- `docs/specs/debug-logging/debug-logging.md`
- `docs/skills/logging-checker-v1.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/regressions/2026-09-17/2026-09-17-ice-host-poll-not-fetched-after-first-join.md`
- `docs/regressions/2026-09-27/dev-loop-bypassed-room-code-REG.md`
- `docs/regressions/2026-10-17/map-id-truncation-stale-ready-REG.md`
- `docs/regressions/2026-10-24/mode-one-server-port-REG.md`

## Findings

### 1. Join retries could create duplicate players

`JoinRequest` is re-sent every 500 ms while waiting for `JoinAccept`
(`src/network/multiplayer-tick.cpp:1107`). The server deduplicated only by
`reconnectToken` (`src/network/server-packets.cpp:1639`), not by the join token
that identifies the attempt. A lost accept therefore created a new player on
each retry and produced repeated join chat messages. The server now deduplicates
by `joinToken`, skips repeated coordinator validation, reuses the player, and
broadcasts the join message once.

### 2. Managed modes could start before any client was active

TDM and NPC Waves enter `INTERMISSION` at server boot
(`src/network/server-gamemode.cpp:1414`) and NPC Waves started wave 1 from that
intermission without checking for a human (`:6927`). If the client connected
after the intermission, the match could already be in COUNTDOWN/ACTIVE or the
wave could already have begun. A `WAITING`-only gate was insufficient because
the initial phase is `INTERMISSION`. The intermission now holds until at least
one required client is active, and NPC Waves explicitly waits for an active
human.

### 3. Join could stall the server for tens of seconds while picking a spawn

The unbounded safe-spawn probe scans collision geometry and calls capsule
validation for many candidates. On `npcwavesv2` it held the authoritative server
loop for about 25–35 seconds while the client retried its join. The probe now has
a 32 m local radius, a 250 ms budget, and a bounded validation deadline. If no
validated spot is found, the map author's explicit spawn is used with an
`spawn.authored-unvalidated` ERROR event instead of refusing the join and
leaving the mode waiting forever.

### 4. Reconnect token rotation outlived the client retry window

The server kept the previous reconnect token valid for 5000 ms
(`src/network/server-packets.cpp:1816`) while the client retry interval is
6000 ms (`config/networkingconfig.json:179`). A lost `ReconnectAccept` made
later retries use an expired token. The previous token is now valid for the
configured reconnect grace window, and accepted/rejected reconnect requests
emit bounded structured evidence.

### 5. NPC Waves NPCs could respawn and prevent the wave from clearing

`respawn_seconds: 0.01` in `config/gamemodes/npc_waves.json` enabled the shared
match-respawn path for NPCs (`src/network/server-npcs.cpp:1735`). Wave actors are
now one-life; dead bodies still remain in the broadcast for their death
presentation while the wave clear condition can be reached.

### 6. The dev-loop did not exercise room-code/ICE join

The default launch used `--connect 127.0.0.1:<port>` and published the room code
only after the client was already playing. New launch-mode/profile flags
`join_via_room_code`, `require_client_connection`, `require_mode_start`,
`require_npcwaves_start`, and `ice_force_relay` make the room-code path and
bounded readiness checks reproducible through the existing dev loop.

## Exact implementation changes

### `src/network/multiplayer-packets.cpp`

- Added `network.connect.stage` for room lookup, TURN credentials, candidate
  gather, coordinator begin-join, host answer, ICE connect, and join stages.
- Added `network.session.health` with packet age, stale/hard thresholds, grace
  deadline, reconnect attempts/backoff, received-after-disconnect, false-positive
  suspicion, notification title/message, and build ID.
- Preserved the existing `network.connection-state.changed` event.
- Incremented `connectionAttemptId` and tracked `joinRequestSends` for bounded
  attempt diagnostics.

### `src/network/multiplayer-tick.cpp`

- Emit join-request, join-accept, reconnect-accept, and initial-spawn stages.
- Emit session-health attempts on reconnect sends.

### `src/network/server-packets.cpp`

- Deduplicate retried joins by `joinToken` before capacity/validation.
- Skip repeated coordinator validation for an already accepted join attempt.
- Do not create or announce a second player for a retry.
- Bound VIP ticket verification to 2000 ms and emit its elapsed time.
- Keep the previous reconnect token valid for the reconnect grace window.
- Emit reconnect accepted/rejected evidence.

### `src/network/server-gamemode.cpp`

- Emit `network.mode.start` for waiting, countdown, and active stages.
- Emit `network.npcwaves.start` for begin, spawn, target-reached, cleared,
  run-over, and lifecycle-stall stages.
- Hold managed intermission while no client is active.
- Hold NPC Waves until an active human exists.
- One-life NPC Waves actors (diagnostic `npc_respawns_enabled=false`).
- Only report a wave stall for no spawn progress or a stale live roster, not for
  a normal live NPC.

### `src/network/server-players.cpp`

- Bounded the safe-spawn probe to a local 32 m area and 250 ms.
- Added a validated-fallback path using the authored spawn with an explicit
  `spawn.authored-unvalidated` event.

### `src/network/ice/*`

- Added `MIMITA_ICE_FORCE_RELAY` / `IceConfiguration::forceRelay`.
- Relay-only mode filters non-relay SDP candidates on both local and remote
  descriptions.
- Added `network.ice.path` with selected candidate types and `force_relay`.

### `src/website/api-client.*`

- Added an optional HTTP timeout to `httpRequest` and `verifyVipJoinTicket` so a
  website outage cannot block the authoritative join path for the HTTP default.

### `devscripts/dev-loop.py` and launch modes

- Added bounded room-code wait before client launch.
- Added bounded client `Connected`, mode ACTIVE, and NPC Waves begin checks.
- Added launch mode 40 (NPC Waves networking validation), 41 (TDM networking
  validation), and 42 (TDM relay-only validation).
- Added `ice_force_relay` environment propagation.
- Added `devscripts/dev-profiles/networking-validation.json` and
  `npc-waves-validation.json`.
- Added badconn preset 10 (immediate 5 s blackout) for reconnect testing.

### Documentation

- Added the networking/mode-start intake to `docs/specs/open-gameplay-issues.md`.

## Diagnostics

- Owner/category: `StructuredCategory::Network` for transport/session/stage;
  `StructuredCategory::Duel` for mode/NPC Waves lifecycle.
- Events: `network.connect.stage`, `network.session.health`,
  `network.mode.start`, `network.npcwaves.start`, `network.ice.path`,
  `spawn.authored-unvalidated`.
- Rate limiting: stage events only on transitions; ICE events at most once per
  attempt; NPC Waves stall at most once per wave; session health at most once
  per transition/attempt.
- Failure reasons are explicit fields (`error_code`, `failure_reason`,
  `waiting_for`, `result`).

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `python -c "import jsonc; ..."`: passed for `dev-launch-modes.json` and
  `config/networkingconfig.json`.
- Dev-loop published builds 3596 through 3619; the final compiled source
  includes the ICE path diagnostic in build 3617.
- `python build.py build-only` was used once to read a compiler error and then
  the dev loop rebuilt/published successfully. It was not used as runtime
  evidence.

## Runtime evidence

- TDM room-code launch, build 3596:
  `logs/2026-10-28/20261028_120409/events.jsonl` — room lookup, TURN, gather,
  begin-join, host answer, ICE connect, join request/accept, map ready, spawn,
  mode countdown/active. Dev-loop complete succeeded.
- NPC Waves room-code launch, build 3606:
  `logs/2026-10-28/20261028_123223/events.jsonl` — mode waiting for active
  human, countdown, wave 1 begin, target 1, NPC 100000 spawned, client observed
  GO/ACTIVE, dev-loop complete succeeded.
- Final TDM launch, build 3621:
  `logs/2026-10-28/20261028_125426/events.jsonl` — client Connected, mode ACTIVE,
  build identity matched, dev-loop complete succeeded.
- Final NPC Waves launch, build 3624:
  `logs/2026-10-28/20261028_125638/events.jsonl` — wave 1 begin and target
  reached, NPC `100000` spawned, mode ACTIVE, dev-loop complete succeeded.
- Repeat NPC Waves launch, build 3626:
  `logs/2026-10-28/20261028_125900/events.jsonl` — client Connected, mode ACTIVE,
  wave 1 began, dev-loop complete succeeded.
- Relay-only TDM launch, build 3617:
  `logs/2026-10-28/20261028_125100/events.jsonl` — client and server selected
  relay/relay candidates, `force_relay=true` on the server, join and mode ACTIVE
  succeeded.
- Earlier TDM and NPC Waves evidence, builds 3606/3607:
  `logs/2026-10-28/20261028_123223/events.jsonl` and
  `logs/2026-10-28/20261028_123912/events.jsonl`.
- Server-loop stall evidence before the spawn fix:
  `logs/2026-10-28/20261028_120917/events.jsonl` recorded
  `performance.server-loop-window` with `loop_max_ms=35501` and
  `spawn.authored-unvalidated` did not yet exist.

## Measured evidence

- Pre-fix join stall: about 25–35 seconds between join validation and spawn
  selection on `npcwavesv2`.
- Post-fix NPC Waves path: join accept about 0.35 s after request; mode ACTIVE
  about 3 s after countdown; wave 1 target reached on the next tick.
- Relay-only TDM: first host-answer timeout 15 s, second attempt connected;
  selected pair was relay/relay.

## Regression review

- No new append-only regression entry was created. The confirmed defects are
  recorded here and in the tracker because their prior working baseline is not
  established.

## Human acceptance

- Visual review: not performed.
- Gameplay review: not performed.
- Multiplayer review: room-code and relay-only paths were observed in journals;
  direct human multiplayer acceptance remains open.
- Still unverified: 10+ consecutive fresh TDM and NPC Waves launches, repeated
  leave/reconnect cycles, invalid/expired room, coordinator-unreachable, and
  human visual/gameplay acceptance.

## Related feature record

- None.
