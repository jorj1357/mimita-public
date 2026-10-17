# Priority 1 map disagreement / Priority 2 weapon identity session

Time: 2026-10-17T00:32:02Z

## Scope

Worked the 2026-10-16 open gameplay issues in gate order: prove agent command
adapters (Gate A), complete the one-command scenario runner (Gate B), fix the
Priority 1 map/spawn disagreement (Gate C), and fix the Priority 2
equip/attack disagreement across lives (Gate D).

## Root cause found (Priority 1)

The server advertises its authoritative map id over `WelcomePacket`,
`JoinAcceptPacket`, `MapChangePacket`, `DuelStatePacket`, and is answered with
`ClientMapReadyPacket`. All five stored it in `char mapId[MAX_NAME_BYTES]` where
`MAX_NAME_BYTES = 32`. The reported map id
`mimita-aabb-only-interior-small-v4` is 34 characters, so `strncpy(dst.mapId,
gServerMapId.c_str(), sizeof(dst.mapId) - 1)` truncated it to
`mimita-aabb-only-interior-small`. The client then tried to load a nonexistent
base map, stayed on its previous world, and still sent `CLIENT_MAP_READY` (its
gate only required `worldLoaded`). The server's truncated advertised id matched
its own map id, so it admitted and spawned the client into a different
collision world — the reported below-world spawn.

## Before / after

`src/network/packets.h`:

```cpp
// before
constexpr int MAX_NAME_BYTES = 32;
char mapId[MAX_NAME_BYTES];
// after
constexpr int MAX_MAP_ID_BYTES = 64;
char mapId[MAX_MAP_ID_BYTES];   // all five map-id fields
```

`src/engine/engine-tick-net.cpp`:

```cpp
// before
const bool mapReady = !mpContext.requiredMapId.empty() && worldLoaded;
// after
const bool mapReady = !mpContext.requiredMapId.empty() && worldLoaded &&
    mapIdsReferToSameMap(ACTIVE_MAP_PATH, mpContext.requiredMapId);
```

## Changes

- `src/network/packets.h`: added `MAX_MAP_ID_BYTES = 64`; widened all five
  `mapId` fields; updated the `WelcomePacket` (352), `JoinAcceptPacket` (352),
  and `ClientMapReadyPacket` (88) wire-size `static_assert`s.
- `src/network/client.cpp`: added a `copyMapId` helper for the widened field.
- `src/engine/engine-tick-net.cpp`: client readiness now requires the loaded
  world to match the required map; bounded `network.client-map-not-ready`
  diagnostic; visible `connectionStatus` and stop-retrying after repeated
  `network.client-map-load-failed`.
- `src/network/multiplayer-tick.cpp`: bounded `network.map-required` diagnostic
  at the welcome, join-accept, and map-change writers.
- `src/network/server-players.cpp` / `src/network/server.h`:
  `serverPickSafeSpawn` now returns `bool` and is the single authoritative spawn
  owner: authored points are ground-snapped and validated; maps with no authored
  points derive a validated standing position by probing collision geometry;
  a genuinely unsafe world refuses visibly (`spawn.server-no-safe-spawn`)
  instead of placing the actor at an unvalidated coordinate. Emits
  `spawn.server-selected`. All three callers handle refusal.
- `src/network/server-attack.cpp`: `handleAttackRequest` authorizes on the
  stable `equippedWeaponId` (native slot only as a fallback when the id is
  empty); the rejection event now reports the authoritative id and generation.
- `src/network/server-projectiles.cpp`: the legacy projectile path keeps
  `equippedWeaponId` consistent with the reconciled slot.
- `src/input/input-poll.cpp`: fixed the `consumeScheduledTerminalMove` caller to
  pass the added `justEnded` out-parameter.

## Priority 2 gate (already partly staged by a prior session)

- `ServerPlayer::equippedWeaponId` is preserved across
  `resetPlayerForSpawn` when still owned/allowed; `completeAuthoritativeSpawn`
  publishes it and the client re-applies it. `weapon.attack.result` is emitted
  for every authoritative attack.

## Scenario runner (Gate B)

`devscripts/dev-loop.py --scenario-file <path>` now ensures a live run (reusing
a matching run or launching with an optional per-scenario `map` override),
queues command steps through the daemon, waits for the game's execution-complete
record, evaluates assertions from the live journal, appends
`scenario.started` / `scenario.step` / `scenario.assert` / `scenario.finished`,
and returns nonzero unless passed. New assertion types include `event_present`,
`event_absent`, `event_field_equals`, `spawn_in_bounds`, and `map_ids_agree`.

## Dev-loop documentation

`docs/specs/dev-loop/dev-loop.md` §19.4a: AI is always allowed to restart the
dev-loop daemon/game whenever it wants or needs to, provided it restarts it and
keeps exactly one healthy daemon alive; includes a watcher-script TODO so the AI
does not have to remember.

## Runtime evidence

- Build: `python build.py build-only` = BUILD SUCCESS (forced recompile of all
  touched translation units). Published builds 2447–2449.
- Priority 1 scenario `devscripts/scenarios/priority1_map_spawn_v1.json`, run
  `logs/2026-10-17/20261017_002823/events.jsonl`: `passed=True
  execution_complete=True assertions=10`. Server loaded
  `assets/maps/mimita-aabb-only-interior-small-v4.glb` (`spawn_points=0`), client
  `network.map-required required_map_id=mimita-aabb-only-interior-small-v4`,
  client loaded the same file, `spawn.server-selected
  source=derived-probe validated=true position=[262.875,32.016,120.770]`
  (in bounds `[-20,-51,-5.13]..[564,110,130]`), no `client-map-not-ready`, no
  `map.load-failed`, no `spawn.server-no-safe-spawn`.
- Priority 2 scenario `devscripts/scenarios/priority2_equip_respawn_v1.json`,
  same run journal: `passed=True assertions=4`. Rocket Launcher logical slot 3
  / native slot 7 attack accepted before (`spawn_generation=18`) and after
  (`spawn_generation=19`) `respawn_all` with no manual re-equip;
  `weapon.attack.weapon_not_equipped` count 0.
- Command smoke `devscripts/scenarios/command_proof_smoke_v1.json`:
  `passed=True execution_complete=True assertions=2`.

## Evidence boundaries

- Build success proves compilation only; the runtime claims above come from the
  named live journals.
- Human visual review of the in-bounds spawn is still outstanding; the Priority
  1 regression record is `ATTEMPTED FIX (1)`, not `SOLUTION`.

## Regression record

`docs/regressions/2026-10-17/map-id-truncation-stale-ready-REG.md`.

## Follow-up (same session, 2026-10-17T01:10:00Z)

- **Always-on dev-loop watcher implemented:** `devscripts/dev-loop-watch.py`
  (per `dev-loop.md` §19.4a). Detects a dead checkout-owned daemon and restarts
  it from the current `dev-loop.py`; detects a changed daemon script hash and
  performs a controlled replacement (deferring while a live run is present
  unless `--force-source-change`); keeps exactly one daemon per checkout; records
  `dev-loop.watcher.started`/`restarted`/`source-change-deferred` and a visible
  error if it cannot keep a daemon alive. The daemon now records
  `daemon_pid`/`daemon_script_hash`/`daemon_started_utc` in `.dev/current-run.json`
  so a stale daemon is detectable. Verified live: the watcher auto-restarted the
  daemon after a stop (`restarts=3`) and kept the loop alive.
- **Scenario runner extended** (`devscripts/dev-loop.py`): per-step execution
  with `wait_for_event` (bounded, field-conditioned), fresh-journal capture,
  the daemon's per-entry `:0` request-id suffix handled, changed-run detection
  after a launch, and per-scenario `map` + `weapon_set` launch overrides. New
  assertion types `event_count_at_least`/`event_count_at_most`/
  `event_field_count_at_least`, plus `map_ids_agree` and `spawn_in_bounds`.
- **Terminal equip argument bug fixed:** `equip`, `equipslot`, and `equipweapon`
  indexed `args[1]`/required size 2, but the dispatcher passes only the arguments
  (the command name is stripped; `npc_spawn` and `agent.*` use `args[0]`). They
  now use `args[0]`, so the commands work through the agent queue.
- **Priority 2 full multi-life / NPC-kill / two-set scenarios added and passed:**
  - `devscripts/scenarios/priority2_multilife_npc_kill_v1.json` (set 5, hotbar
    slot 3): run `20261017_010820`, 27 NPC deaths, 5/5 attacks accepted at
    `client_slot=3`, 0 `weapon_not_equipped`.
  - `devscripts/scenarios/priority2_multilife_npc_kill_set3_v1.json` (set 3,
    hotbar slot 1): run `20261017_010704`, 25 NPC deaths, 6/6 attacks accepted
    at `client_slot=1`, 0 `weapon_not_equipped`.
  Both use `healthme 1` (self-only, persists through respawn) and `npc_spawn` on
  the known-good map `dust2cyberiav4`. The reported map
  `mimita-aabb-only-interior-small-v4` was unsuitable for the NPC-kill case
  because it has zero spawn points, so NPCs spawn away from the player.

## Follow-up 2: dev-loop helper console hygiene (Option B)

Problem: starting/restarting the daemon opened a burst of visible PowerShell
windows. Cause (`devscripts/dev-loop.py`): `windows_process_identity()` spawned
`powershell -NoProfile -Command Get-CimInstance ...` with no `creationflags`,
and `state()` called `server_health()` up to three times **per status render**
(the main loop renders ~5x/s), so while the server was adopted
(`server_process is None`, the daemon-restart-with-persistent-server state) the
loop spawned PowerShell ~15-30 times/second.

Fix (Option B):
- New `run_hidden()`/`_hidden_startupinfo()` create helpers with
  `CREATE_NO_WINDOW` + `STARTF_USESHOWWINDOW`/`SW_HIDE` + `close_fds`.
- Routed `existing_dev_loop_pids`, `windows_process_identity`, `taskkill`,
  `--versioninfo`, and the incremental-build `Popen` through hidden creation.
- Short-TTL identity cache for `windows_process_identity`
  (`_PROCESS_IDENTITY_TTL_SECONDS = 1.5`).
- `state()` now queries `server_health()` once and reuses it for
  `server_alive`/`run_alive`.
- Documented the PowerShell-free steady-state target (ctypes Win32 APIs) so the
  Option-C migration is specified: `dev-loop.md` §10.1, §5.1 clause, §19.4
  cross-reference.

Runtime evidence: in the adopted-server state a 6 s sampler counted 3 distinct
`powershell.exe` PIDs (including the sampler) versus the prior per-render flood;
the daemon still adopted the server and rendered status.

Records: `docs/regressions/2026-10-17/dev-loop-helper-powershell-window-flood-REG.md`.

## Follow-ups

- Human visual confirmation of the Priority 1 spawn on the reported map.
- Human visual confirmation that the daemon restart no longer flashes windows.
- Option C: replace the remaining PowerShell identity helper with ctypes Win32
  APIs (specified in `dev-loop.md` §10.1).
- Consider a startup hook that launches the watcher automatically.
