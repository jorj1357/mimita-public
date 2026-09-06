// 09 06 2026, 12 00
/* purpose
* track reusable gameplay primitives for movement, weapons, damage, UI, and modes
* prevent one-off mode implementations from duplicating engine behavior
* connect primitive specifications to feature tests and later composed modes
* this file DOES NOT define a Counter-Strike product specification
* this file DOES NOT replace subsystem specifications
* this file DOES NOT authorize implementation without acceptance criteria
*/

# Gameplay primitives feature record

## End goal

Provide configurable primitives so new modes and weapons compose existing
movement, collision, weapon, damage, effect, UI, timer, team, and replication
behavior instead of duplicating it.

## Current status

- Status: planned

## Primitive direction

- New weapons use the shared weapon definition plus model, sound, effect, and
  damage configuration.
- New modes compose shared lifecycle, team, alive-state, timer, HUD, and network
  primitives.
- Client prediction, server authority, replay, and tests call shared gameplay
  rules rather than separate copies.

## Links

- Tests: `tests/features/gameplay-primitives/`
- Logs: `logs/features/gameplay-primitives/`
- Changelogs: add immutable session links here as work completes
