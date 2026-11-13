# Live generation activation (2026-11-12)

**Status:** probe-only live activation, failed-candidate fallback, rollback, and
same-process state continuity are proved. No whole-game cutover.

## Implementation

- `HostRuntime::poll()` reads `MIMITA_GAME_READY` or `build/game/ready.json`.
- Ready files must contain `status:"ready"`, `generation`, and `dll`.
- Polling is throttled to 250 ms.
- On a new generation:
  1. quiesce the barrier (2 second budget);
  2. load and validate the candidate while the old generation is still loaded;
  3. quiesce and shut down the old generation;
  4. commit the candidate and start the new generation;
  5. on missing/rejected candidate, keep the old generation active.
- Failed generation/dll pairs are retried after 5 seconds instead of every
  250 ms.
- The ready manifest generation overrides the module's local `generation` field
  so probe modules with a hardcoded value do not swap repeatedly.
- `main.cpp` polls in the client loop; `server.cpp` polls in the dedicated
  server loop.

## Evidence

Run: `C:\mimita-v9\logs\2026-11-12\20261112_230534`

Same server PID `22784` and client PID `11324` throughout.

```text
before:          generation-20, player pos [117.363, 35.213, 312.492]
invalid ready:   game.generation.activate-failed generation=99 missing.dll
after-failed:    still running; admincmds accepted; pos [117.363, 35.210, 312.474]
valid ready:     game.generation.activated generation=21 old_generation=1
after-swap:      admincmds accepted; pos [117.362, 35.205, 312.475]
rollback ready:  game.generation.activated generation=20 old_generation=21
after-rollback:  admincmds accepted; pos [117.361, 35.200, 312.422]
```

Client command results after the failed candidate, after the swap, and after the
rollback:

```json
{"event":"network.server-command.result","accepted":true,
 "status":"admincmds=1 (host)"}
```

The small position changes are normal idle/physics movement; the player, server
tick, connection, and process identities continued. This is same-process state
continuity evidence, not a formal migration proof.

## What is proved

- live probe generation swap without EXE restart
- same server/client PIDs across the swap
- missing candidate keeps the old generation active
- failed compile does not publish a new current/ready generation
- rollback from generation 21 to 20
- exact `admincmds` authority works before, during, and after the swap
- player position/tick continuity across all swaps

## Failed compile fallback

A temporary source set added `build/tmp/broken-probe.cpp` with `#error`. The
worker exited 1, but `build/probe/npc-target/current.json` and `ready.json`
hashes were unchanged, the host stayed on the old generation, and a subsequent
`admincmds` still produced the client result. The temporary files were removed.

## What is not proved

- whole-game 584-source DLL swap
- schema migration on component changes
- adding/removing source files through the live coordinator
- cutover or EXE-as-host-only

## Next

1. Connect `live.py` to real whole-game builds and the production ready path.
2. Add failed **compile** fallback evidence to the coordinator-driven path; the
   direct worker test already proves a failed compile does not publish current/
   ready.
3. Add runtime failure-path evidence for callback preservation/resume; the standalone barrier test already proves `Busy -> resume -> callback`.
4. Prove add/remove/header edit through the live coordinator.
5. Then link families, static gate 0, and the single all-files cutover.
