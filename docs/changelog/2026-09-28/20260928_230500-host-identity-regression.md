# Host identity regression fix

- Date: 2026-09-28 (UTC)
- Scope: dev-loop server host-command authorization
- Status: code change complete; link validation passed; live multiplayer confirmation remains required

## What happened

The server decided whether a player was the host by comparing a display-name string from the join message with `--host-player`. The launcher, authentication path, and join path can provide different names or casing for the same person. The server then set `ServerPlayer::isHost` to false, so valid commands were rejected as `reason=not-host`.

## What changed

- `src/network/server-packets.cpp`: replaced `computeHostFlag(rawName, playerCount)` with `assignHostFlag(ServerPlayer&, bool)`. The first accepted player session now owns host authority by numeric player ID. A reconnecting existing player keeps its previous host flag. Both the normal Hello path and the room-code/ICE path use the same helper.
- `src/network/server.cpp` and `src/network/server.h`: added and reset `gServerHostPlayerId` for each server session. `gServerHostPlayerName` remains informational and no longer grants authority.
- `src/network/server-packet-chat.cpp`: a rejected host command now logs both the requesting `playerId` and the current `hostPlayerId`; assignment logs `[SERVER HOST OWNER]`.
- `docs/regressions/2026-09-27/dev-loop-first-client-not-host-REG.md`: recorded this as the second occurrence of the same regression, including prevention checks.

## Validation

- `python -m py_compile devscripts/dev-loop.py`: passed.
- `MIMITA_FORCE_LINK=1 python build.py build-only`: passed and linked `mimita.exe`; this run compiled 0 translation units and skipped 497, so it is link evidence, not fresh source-compilation evidence. The changed server objects were already rebuilt at 2026-09-28 18:55 UTC.
- `git diff --check`: passed; only line-ending warnings for existing Windows working-tree files were reported.
- Runtime and human acceptance are still open: start a fresh `devscripts/dev-loop.py` session, confirm the server prints `[SERVER HOST OWNER]` for the first player, and run `healthall 999`, `mapchange 1`, and `procedural_world_start infinite_dungeon_slayer`. Reconnect and repeat the checks.

## Repository safety

Pre-existing edits in `config/analytics.json`, `docs/architecture/collision/collision.md`, the earlier phase-2 changelog, `src/physics/movement/actor-triangle-solver.cpp`, and any unrelated files were preserved. This changelog is the single completion record for this repository-touching session.
