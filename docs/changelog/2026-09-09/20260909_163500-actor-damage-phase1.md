# Actor damage migration phase 1

Date: 2026-09-09

## Changes

- Added shared server actor references for player and NPC actors.
- Added a neutral damage request carrying actor, weapon, source, event,
  correlation, map epoch, and server tick context.
- Added typed actor lookup so callers do not assume every actor ID is a player.
- Extended damage results with rejection and correlation fields for the next
  migration phase.

## Validation

- `python build_agent.py` completed with `Status: SUCCESS`.
- Existing standalone tests were inventoried; the canonical build does not
  automatically compile or run the test programs.
- No live multiplayer acceptance was performed.
