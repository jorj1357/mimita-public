# Whole-game DLL load/initialize evidence (2026-11-13)

**Status:** the generation-75 whole-game candidate was loaded and initialized by
the rebuilt host EXE. This is load/initialize evidence, not full-runtime proof.

## Build identity

- Host artifact: `.dev/builds/2026-11-13T02-44-36.913656Z/mimita.exe`
- Build ID: `2026-11-13T14-07-20.022935Z`
- Candidate: `build/game/generation-75/game-g68.dll`
- Candidate export: `GetMimitaGameModuleV1`
- Candidate ABI: 1

## Runtime observation

- Run: `20261113_090828`
- Process: dedicated server PID `13028`
- Journal: `logs/11-12-2026/20261113_002726/events.jsonl`
- Environment: `MIMITA_GAME_PROBE=1`,
  `MIMITA_GAME_DLL=C:\mimita-v9\build\game\generation-75\game-g75.dll`
- Event:
  `game.host.probe.started` with `abi=1`, `generation=1`, and the candidate DLL
  path.
- The server stayed alive after initialization; it was stopped by the operator
  after the observation window.
- The `executable` log category was temporarily raised to `important` so the
  probe event was written; `config/debuglogger.json` was restored afterward.

`HostLoader::loadCandidate` calls `initialize` before emitting
`game.host.probe.started`; a failed initialize unloads the candidate. The event
therefore proves the whole-game `initialize` returned `Ok`.

## Not proved

- Full `gameTick`/render loop. The current whole-game `gameTick` is a no-op.
- Client, multiplayer, listen-server, ICE, or authored-rebuild execution.
- Same-PID generation swap, rollback, or state migration after load.
- Source-ownership cutover.
- Any gameplay or user-visible behavior.

## Next runtime gate

Drive a real fixed tick and render frame through the whole-game module, then
exercise a source edit/activation with the same process. That requires deciding
how the host engine loop hands tick/render ownership to the game generation; it
is not implied by link 0 or successful initialization.
