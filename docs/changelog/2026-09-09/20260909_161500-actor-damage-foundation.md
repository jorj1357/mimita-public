# Actor damage migration foundation

Date: 2026-09-09

## Changes

- Added shared server actor references for players and NPCs.
- Added a neutral actor damage request carrying source, weapon, event,
  correlation, map epoch, and server tick context.
- Extended damage results with rejection reason and event correlation fields.
- Added typed actor lookup that never guesses an ID is a player.

## Validation

- Canonical build started and source compilation was in progress at handoff.
- No live acceptance was performed.
