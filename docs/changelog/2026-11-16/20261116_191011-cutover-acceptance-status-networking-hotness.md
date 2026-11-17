# Cutover acceptance, truthful status, networking FAQ, and hotness matrix

## Task

- Task ID: 20261116-cutover-acceptance-status-networking-hotness
- Session ID: 20261116-cutover-acceptance-status-networking-hotness
- Owner: opencode
- Focus: Cutover-1 acceptance and hotness boundaries
- Status: partial; documentation and owner proof complete, live-edit proof pending
- Date, time, timezone: 2026-11-16T19:10:11Z
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Final commit: uncommitted

## Requested behavior

Remove audit/planning claims from authoritative status, expand Cutover-1
acceptance around real-EXE `events.jsonl` evidence, add networking local/public
guidance and visible local-only state, and define/verify the representative
hotness matrix for melee, FPS, room joining, and reconnect.

## Exact implementation changes

### `devscripts/hot_reload_status.py`

- Removed reads and output for:
  - `audit_clean`,
  - `audit_rows`,
  - `audit_high_risk`,
  - `audit_counts`,
  - `unresolved_game_blockers`.
- `status.json` now reports only build/run/packet/state facts and authoritative
  gates.
- Optional audit/census reports remain separate report-only files.

### `docs/specs/hotreload/hotreload.md`

- Added the Cutover-1 runtime acceptance contract:
  - real executable, same PID, exact journal, iteration identity;
  - requirement-to-event matrix;
  - hotness proof procedure;
  - melee, FPS, room, reconnect, NPC/collision, map editor, state, and rollback
    event contracts;
  - randomized stress as a later, deterministic-seed extension;
  - explicit rule that self-tests are not acceptance.
- Linked `hotness-matrix.md`.
- Updated the generated status description to exclude audit/census from
  authoritative status.

### `docs/specs/hotreload/hotness-matrix.md`

- Added owner/cold-exception matrix for melee, FPS, room creation/join,
  reconnect, NPC containment, surface snags, map editor gizmos, outlines,
  input, state migration, server policy, and safe unload.
- Added the representative live-edit proof format.
- Recorded that changing raw `send`/`recv`/socket/ICE primitives is cold, while
  game-side connection/room/session policy is live.
- Recorded current evidence caveats: room registered/Connected and melee FX/FPS
  events exist, but controlled live edits and reconnect acceptance are pending.

### `docs/specs/networking/networking.md`

- Added a connectivity FAQ:
  - `127.0.0.1` is same-machine only;
  - other players can join only when coordinator registration and a public room
    code exist;
  - public server/preset launch examples;
  - loopback/dev-loop launch examples;
  - room-code join example;
  - coordinator-unreachable behavior: local play stays allowed, but the room
    must visibly become LOCAL ONLY/OFFLINE and must not imply public joinability;
  - what room/status information must be shown.

### `src/engine/engine-tick-ui-overlays.cpp`

- Room HUD now displays local room codes as:
  `Room: LOCAL-XXXX LOCAL ONLY - others cannot join`.
- Local-only text uses a high-visibility orange and a smaller scale to fit the
  warning.
- Public room codes keep the existing display.

## Validation

- `python -m py_compile devscripts/hot_reload_status.py`: passed.
- `python devscripts/hot_reload_status.py --output build/hot-reload/status.json`:
  passed; audit fields removed.
- Game DLL build after the HUD change: generation `283` published.
- Owner proof:
  `python devscripts/cold_change_guard.py src/combat/melee-client.cpp src/perf/perf-frame.cpp src/network/multiplayer-packets.cpp src/network/connection-health.cpp`
  → all four `HOT`, exit 0;
  report `build/hot-reload/hotness-owner-proof.json`.

## Runtime evidence currently available

- Run `20261117_000451`:
  - `network.room.registered`: room `YDZ4UYY`, public coordinator registration.
  - `network.connection-state.changed`: `Connecting -> Connected` on
    `127.0.0.1`.
  - `performance.fps`: about 60 FPS in that window, 100% of frames over the
    4.16 ms budget.
  - melee contact FX events exist in the client journal.
  - reconnect path reached `ReconnectFailed` after about 60 s.
- Run `20261116_203949_01`:
  - `performance.fps` around 69–86 FPS in sampled windows, all over budget.

No controlled live-edit proof for melee/FPS/room/reconnect was completed because
the currently available client journal is frozen/offline. The matrix records the
exact owner proof and the remaining live-edit evidence requirement.

## Important limitations

- The public/local-only HUD currently depends on the room code being `LOCAL-`.
  Dedicated-server coordinator registration failure still needs a game-visible
  local-only status event and banner if no local code is published.
- The room HUD change is compiled but not visually accepted by a human.
- The networking FAQ documents the agreed behavior; it is not runtime proof.
- `ReconnectFailed` is observed, not fixed.

## Next tasks

1. Add a game-visible local-only/offline status transition for dedicated-server
   coordinator registration failure.
2. Run controlled live edits for melee, FPS, room joining, and reconnect using
   the matrix proof format.
3. Fix reconnect root cause and re-run the same-PID session scenario.
4. Fix/measure FPS against the 4.16 ms budget.
5. Add real-EXE room/HUD visual acceptance and human multiplayer acceptance.
