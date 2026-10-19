# Melee NPC Death Server Crash From Stale Dependency

Time created: 2026-10-19T21:24:32Z (2026-10-19 17:24:32 EDT, America/New_York)
Time last updated: 2026-10-19T21:24:32Z

Status: FIXED (source/build-owner fix; runtime recheck pending)

Related specifications:

- `docs/specs/gamemodes/supersmashbrosmelee.md`, sections 24, 31, 32, 60

Related changelog: added by the same session under `docs/changelog/2026-10-19/`.

## Regression occurrence 1

### Expected behavior

When a Melee NPC loses its stock by falling into the existing world void, the
server records the stock loss, respawns or eliminates the fighter, and keeps
running the authoritative loop.

### Actual behavior

The server exited with `INT_DIVIDE_BY_ZERO (0xc0000094)`.

Live evidence:

- Journal:
  `logs/2026-10-19/20261019_205156/events.jsonl`
- Crash report:
  `%LOCALAPPDATA%\MiMITA\crashes\crash-2026-10-19_16-52-41-15192.txt`
- Process: server PID 15192, crash at `mimita.exe+0x7ae85a`
- Last server activity: NPC movement at large negative Z near tick 2475, then
  the process ended without `npc.death`, `melee.stock_lost`, or `npc.respawn`.

### Confirmed cause

The crash stack maps to:

`runServer` -> `simulateSharedNpcs` -> `serverProceduralWorldOwnsNpc` ->
`ProceduralWorldState::encounterNpcIds.find`.

`build/obj-debug/procedural_procedural-world.o` was compiled on 2026-10-16.
`src/network/server-gamemode.h` changed on 2026-10-17, adding fields before
`ProceduralWorldState procedural`. The matching
`build/obj-debug/procedural_procedural-world.d` dependency file was zero bytes,
so `build.py` read no header dependencies and considered the object current.
The stale object read the wrong offset as `encounterNpcIds`; the unordered-set
bucket count at that offset was zero, so `find` divided by zero.

### Fix

`build.py` now treats a zero-byte `.d` dependency file as changed and removes it
before rebuilding, exactly as it already handled zero-byte `.o` files. The four
existing zero-byte dependency files were deleted so the next dev-loop build
rebuilds their translation units.

### Evidence and limits

The crash report and journal prove the failing process and first failing owner.
The source fix is confirmed but the rebuilt executable still must reproduce the
same NPC void death in a fresh dev-loop run and keep the server alive. A
successful build alone does not prove the crash is fixed.
