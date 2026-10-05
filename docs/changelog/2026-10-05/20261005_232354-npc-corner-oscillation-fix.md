# NPC thin-wall / corner oscillation fix (per-actor direction variety)

Date (UTC): 2026-10-05
EST timestamp: 2026-10-05 19:23:54 EDT
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Human report: all 4 CT NPCs got stuck in one corner with a thin wall, switching
direction back and forth super quickly, and seemed biased to one side.

## Causes

1. **No per-actor direction variety.** Wall avoidance and the blocked-turn
   scorer always tried "left" first (a fixed orientation), so a whole squad
   picked the same side of a wall and piled into the same corner.
2. **The local correction could be re-issued every tick.** In a corner the
   wall-avoid layer recomputes an opposite escape direction each tick and called
   `startLocalCorrection` again, flipping the held direction every frame
   ("switching super quick back and forth").
3. **The backward escape lasted far too long.** `wallBacktrackDuration: 10.5`
   sent an NPC the wrong way for many seconds after one blocked frame instead of
   turning and going around.

## Fixes

- **Per-actor side preference (deterministic, by actor id):**
  - `NpcNavigation::wallAvoidDirection` and `NpcNavigator::bestTurnDirection`
    now add a small left/right bias keyed off `npc.id % 2`, so the squad splits
    across sides instead of all choosing left.
  - `NpcNavigator::chooseBestOpenDirection` uses a per-actor tiebreak rotation
    (not a rotation of the sample grid, which biased direction choice and
    degraded route following).
- **Correction hysteresis:** `startLocalCorrection` now ignores a new direction
  that is very different from a still-fresh correction (`timeRemaining >
  duration/2` and dot < 0.5), so the escape cannot flip every tick.
- **Shorter backtrack:** `config/npc-difficulty.json` `wallBacktrackDuration`
  was greatly lowered (the file is being live-edited concurrently; the parser
  clamps it to [0.25, 6] s) so a blocked frame produces a brief step back, not a
  multi-second wrong-way walk.
- Note: per-tick random rotation was deliberately NOT used (it causes jitter);
  the variation is deterministic per actor so behavior is stable but varied.

## Validation

### Build
`python build_agent.py` / `MIMITA_FORCE_LINK=1` -> `BUILD SUCCESS`.

### New selftest `--npc-corner-escape-selftest` (PASS)
4 CT-preset/rage2 actors against a thin tall wall (a dead-ahead blocker):
```text
actor=9800 net=82.9 maxAbsY=41.5 reversals=4
actor=9801 net=82.9 maxAbsY=41.5 reversals=4
actor=9802 net=82.9 maxAbsY=41.5 reversals=4
actor=9803 net=82.9 maxAbsY=41.5 reversals=4
ok  every actor leaves the wall/corner (net > 30 m)
ok  no rapid back-and-forth (direction reversals bounded)
ok  the squad splits across both sides of the wall (no shared bias)
```

### Regression battery (all PASS)
corner-escape, low-obstacle, travel-progress, search-behavior, navigation,
movement-policy, movement-commitment, movement-decision, nav-graph, utility,
team-brain, gamemode, cs-round, targeting, perception, nav-request,
behavior-profile, movement-executor, counterstrike-acceptance.

### Runtime (headless `dust2cyberiav4`, 6 NPCs, ~8 s)
```text
exited 0; per-NPC net/path 0.46-0.54 (directed travel); jumps 42, stuck 492
```

## Human review still needed

Live Dust2 round: confirm the CT squad no longer piles into one thin-wall corner
and that individual NPCs commit to one side and continue instead of flickering.

## Files changed

`src/npc/npc-navigation.cpp`, `src/npc/npc-navigator.cpp`,
`config/npc-difficulty.json`, `src/game/game-cli.cpp`.
