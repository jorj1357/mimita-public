# Live generation activation (2026-11-12)

**Status:** probe-only live activation works in the same server/client processes.
No EXE restart, no cutover, no formal state-continuity proof.

## Implementation

- `HostRuntime::poll()` reads `MIMITA_GAME_READY` or `build/game/ready.json`.
- A ready file must contain `status:"ready"`, `generation`, and `dll`.
- The host throttles polling to 250 ms.
- On a new generation:
  1. `barrier_.beginQuiesce(QuiesceMode::Swap)`
  2. `barrier_.finishQuiesce(2000)`
  3. `loader_.loadCandidate(...)` while the old generation is still loaded
  4. old `quiesce(Swap)` and `shutdown()`
  5. `loader_.commit(...)`
  6. `barrier_.beginGeneration(newGeneration)`
- On quiesce failure or candidate rejection, the old generation is restarted in
  the barrier.
- The ready generation overrides the module's local `generation` field so a
  probe module with a hardcoded version does not swap repeatedly.
- `main.cpp` polls in the client loop; `server.cpp` polls in the dedicated
  server loop.

## Runtime evidence

Run: `C:\mimita-v9\logs\2026-11-12\20261112_225639`

```text
server PID 25812, client PID 5688
start: game.host.probe.started dll=generation-21/game-g21.dll
ready: build/probe/npc-target/ready.json generation=21
server: game.generation.activated generation=21 old_generation=1
client: game.generation.activated generation=21 old_generation=1
same server/client PIDs continued
```

A later `admincmds` after the swap produced:

```text
server: probe.transport.response-sent bytes=264 type=54 sent=true
client: network.server-command.result-packet transport=active
client: network.server-command.result accepted=true status=admincmds=1 (host)
```

That proves the game-owned command response still works after a live generation
swap.

## What is not proved

- Full whole-game DLL swap (the probe is 10 sources).
- Formal resident-state continuity beyond same process/connection.
- Failed-load fallback and rollback evidence.
- Compile-failure fallback evidence.
- Callback preservation when quiesce fails; the current barrier clears
  callbacks during `beginQuiesce`.
- State migration on schema change.
- Cutover or EXE-as-host-only.

## Next

1. Prove failed candidate load keeps the old generation and resumes calls.
2. Prove deliberate rollback to the previous generation.
3. Preserve/rebuild callbacks on failed quiesce.
4. Test same process with a real player position/health/entity state before and
   after swap.
5. Connect `live.py` builds to `ready.json` for the real whole-game DLL.
6. Then the single all-files cutover.
