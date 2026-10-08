# NPC client-side movement audio

time_utc=2026-10-08T01:54:50Z
display_timezone=America/New_York
display_time=2026-10-07 21:54:50 EDT
branch=working tree
commit=not committed

## Scope

Moved NPC movement sound presentation to the client. The dedicated server now
replicates one-shot movement event counters, and the client reuses the existing
3D `playWorldSound()` path at the NPC's replicated world position.

## Pre-existing edits

The working tree contained unrelated edits before this change. They were
preserved. The earlier death-audio path fix and its changelog were also
preserved.

## Source change

- `src/network/server.h`: added NPC dash, ground-jump, air-jump, down-dash, and
  freeze presentation serials.
- `src/network/server-npcs.cpp`: increments those serials from the simulated
  NPC body and copies them into the existing snapshot fields. No packet schema
  expansion was needed.
- `src/network/multiplayer-interpolation.cpp`: NPCs now play one of the
  existing `entity/player/walk1` through `walk4` sounds through the same
  directional world-audio path used by gun sounds. Existing remote dash/jump/
  freeze handlers can now also receive NPC serials.
- `src/npc/npc.cpp`: removed the server-side NPC dash sound call.

## Validation

- `git diff --check`: no whitespace errors; only existing CRLF conversion
  warnings were reported.
- Forced recompilation of the touched source files, then
  `python build_agent.py`: `BUILD SUCCESS`, compiled 42 translation units,
  linked `mimita.exe`, return code 0.
- Copied the newly linked executable to a uniquely named runtime-check copy
  and ran `--versioninfo`. It reported:
  `EVENTS_JSONL_PATH=logs/10-07-2026/20261007_215444/events.jsonl`.
- Live multiplayer/NPC audio acceptance was not performed in this session, so
  audible playback remains to be confirmed in the running game.

## Regression status

No new regression record was created.
