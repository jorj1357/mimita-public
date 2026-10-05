# Shared client/server NPC JSONL logging

Date (UTC): 2026-10-05
EST timestamp: 2026-10-05 14:26:31 EDT
Branch: `afad20a-rebuild`
Commit at handoff: `a1bcea51` (working tree dirty with unrelated concurrent work)

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, focused-test, and in-binary runtime evidence are proven. A full
live client+server Counter-Strike round was not played; human acceptance of the
shared file during a real session remains.

## Scope

Make the launcher create one `events.jsonl` session path and share it with the
client and the server through `MIMITA_EVENTS_FILE`; keep `StructuredLogger` the
only JSONL writer; convert NPC text logging to structured events; instrument the
NPC movement path with bounded change-edge events plus a one-per-second snapshot;
and make `log_open` report what it actually found.

## 1. One shared path

`src/debug/structured-log.cpp` `StructuredLogger::createLogDir`:
- When `MIMITA_EVENTS_FILE` is set, `eventsPath`, `mLogDir`, and `run_id` are all
  derived from that path's directory (no second timestamp), so the client and
  server report the same `run_id`.
- When it is unset, the process generates the run dir as before and then exports
  the absolute path with `SetEnvironmentVariableA("MIMITA_EVENTS_FILE", ...)`.
- The init log line now prints `events path ... run_id ... pid`.

`src/gui/gui-main.cpp` `launchServerProcess`: after `CreateProcessA` (which
inherits the parent environment), logs the shared events file the child will
inherit.

`devscripts/dev-loop.py`: `create_shared_events_path()` makes one run dir;
server and client Popens both receive `env={**os.environ, "MIMITA_EVENTS_FILE":
<path>}`; the path is cleared when the durable server stops.

The existing named mutex `Local\MiMITA_v9_events_jsonl_v1` is unchanged and
still wraps every append.

## 2. `StructuredLogger` only; NPC text writer removed

Converted the `npcLog(...)` text lines to `writeEvent(...)`:
- `npc.cpp` weapon switch -> `npc.weapon-switched`; reaction -> `npc.reaction`.
- `npc-combat.cpp` aim/ shot log -> `npc.shot`.
- `server-npcs.cpp` target line dropped (the `npc.target-changed` event already
  exists); mode profile -> `npc.profile-applied`; NPC-hits-player -> `npc.damage`;
  added `npc.death` (health depleted) and `npc.respawn`.
- `multiplayer-projectiles.cpp` -> `npc.damage` (`damage_confirmed`).
- `multiplayer-reconcile.cpp` -> `npc.damage` (`victim_hp_apply`).

Deleted `src/npc/npc-combat-log.h/.cpp` and the `npcLogSetProc` calls in
`server.cpp` / `main-systems.cpp`. No `NPC_log_*.txt` file is produced anymore.
`Server_log` / `LogManager` startup and error output is unchanged.

## 3. Full movement-path instrumentation

`src/npc/npc.cpp` / `npc.h` (new edge fields `lastJumpReason`,
`lastWallAvoidDir`, `wasStuck`, `movementDecisionTimer`):
- `npc.state-changed` in `logStateChange`.
- `npc.wall-avoid` when wall avoidance actually changes the requested direction
  (edge per distinct adjusted direction).
- `npc.stuck` on the `isStuck` rising edge.
- `npc.stuck-recovery` for the `open_turn_repath` stuck branch; `npc.wall-escape`
  kept for the backtrack branch (both once per episode).
- `npc.jump` on a jump-reason change while jumping.
- `npc.movement-decision` emitted **after physics** (end of `updateOneNpc`) once
  per NPC per second, with `position, state, target_id, target_visible,
  target_remembered, committed_direction, final_direction, replan_reason,
  jump_reason, distance_moved, velocity, on_ground`.

## 4. `config/debuglogger.json`

- `general`: `off` -> `important` (required so `logger.started` is not dropped by
  the level gate).
- `performance`: `important` -> `off`.
- `npc_movement` stays `important`, `npc_combat` stays `off` (switchable live).

## 5. `log_open`

`src/devtools/dev-log-commands.cpp`: prints the absolute `eventsPath`, confirms
the file exists, then reports `client`/`server` presence, `logger.started`,
record/invalid-JSON counts, NPC event counts, and category counts, and finally
selects the file in Explorer.

## Validation

### Build

```text
python build_agent.py              -> BUILD SUCCESS
MIMITA_FORCE_LINK=1 python build_agent.py -> relink verified (14 files)
```

### Pure tests

```text
build/npc-movement-policy-test.exe   PASS (72)
build/npc-navigation-test.exe        PASS (35)
build/npc-movement-executor-test.exe PASS (15)
```

### In-binary selftests (all PASS)

```text
--npc-shared-log-path-selftest        PASS  (eventsPath/run_id from MIMITA_EVENTS_FILE)
--npc-movement-decision-selftest      PASS  (3 snapshots / 180 ticks; full field set)
--npc-wall-escape-event-selftest      PASS
--npc-movement-commitment-event-selftest PASS
--npc-movement-commitment-selftest    PASS
--npc-behavior-profile-selftest       PASS
--npc-movement-policy-selftest        PASS
--npc-search-behavior-selftest        PASS
--npc-navigation-selftest             PASS
--npc-movement-executor-selftest      PASS
--counterstrike-acceptance-selftest   PASS
targeting / aim-fov / spawn-tag / cs-round / gamemode / nav-request /
perception / utility / team-brain / objective / map-config / grenade PASS
```

### Runtime logging proof

- A server run with `MIMITA_EVENTS_FILE=...\logs\shared-path-check\events.jsonl`
  wrote `logger.started` with `"process":"server"` and
  `"run_id":"shared-path-check"` to that exact path, proving the shared-path
  contract and the `general` gate. The client side writes the same fields with
  `"process":"client"` (see the movement-decision run below).
- A movement-decision run produced `logs/10-05-2026/20261005_142412/events.jsonl`
  with 29 records, **0 invalid JSON, 0 records missing any required field**
  (`process/pid/run_id/event/tick/category`), and events:
  `logger.started:1, npc.movement-decision:3, npc.stuck:9, npc.jump:9,
  npc.nav-replan:3, npc.movement-commitment-created:1, npc.goal-changed:1,
  npc.nav-plan-created:1`.

### Pre-existing, not caused by this session

- `--npc-radar-selftest` FAIL on `rage2 uses perfect radar` (config
  `information_mode: "memory"` vs test expectation) remains; unchanged at `HEAD`.
- `--actor-preset-selftest` weapon-value assertion mismatch remains.

## Human review still needed

Start a normal client session (the GUI spawns the dedicated server) and:
1. `log_open` shows one absolute path with `client: yes` and `server: yes` and
   `logger.started: yes`, plus NPC event counts.
2. The same file contains client network records and server NPC records.
3. No new `NPC_log_*.txt` file appears under `logs/<date>/`.
4. Performance records are absent.
5. A live Counter-Strike round lets `npc.movement-decision`, `npc.wall-avoid`,
   `npc.stuck`, `npc.stuck-recovery`, `npc.jump`, `npc.nav-replan`, and
   `npc.target-changed` explain why an NPC changed direction, jumped, replanned,
   was stuck, and how far it moved.

## Files changed

`src/debug/structured-log.cpp`, `src/gui/gui-main.cpp`, `devscripts/dev-loop.py`,
`src/npc/npc.cpp`, `src/npc/npc.h`, `src/npc/npc-combat.cpp`,
`src/network/server-npcs.cpp`, `src/network/multiplayer-projectiles.cpp`,
`src/network/multiplayer-reconcile.cpp`, `src/network/server.cpp`,
`src/main-systems.cpp`, `src/devtools/dev-log-commands.cpp`,
`src/game/game-cli.cpp`, `config/debuglogger.json`;
deleted `src/npc/npc-combat-log.h/.cpp`.

## Pre-existing / unrelated changes

The working tree still contains unrelated concurrent edits (mode packs, disaster
runtime, bomb-site placeholders, etc.); not touched or claimed here.
