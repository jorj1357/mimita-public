# NPC low-obstacle hop (legs caught, torso clears)

Date (UTC): 2026-10-05
EST timestamp: 2026-10-05 19:11:22 EDT
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Human request: NPCs should jump when something blocks their path at leg height
but their torso could clear it (a crate, a low ledge, a lip that catches the
legs), not only when already stuck.

## Change

Added a distinct, deliberate low-obstacle hop so an NPC can vault a leg-high
blocker without first having to become stuck:

- `NpcNavigation::lowObstacleAhead` (`src/npc/npc-navigation.cpp/.h`) casts two
  short forward rays:
  - a **legs** ray at `body.pos.z + lowObstacleLowOffset` (default -1.5 m,
    near the feet), and
  - a **torso** ray at `body.pos.z + lowObstacleHighOffset` (default -0.3 m).
  It returns true only when the leg ray hits a non-walkable face and the torso
  ray is clear. A tall wall blocks both rays and is correctly rejected.
- `src/npc/npc.cpp` adds a hop branch that sets `jump`/`jumpReason=Obstacle`
  when `lowObstacleAhead` is true. Unlike the recovery hop, this traversal hop
  is **not** disabled while following a route (it is a genuine traversal, not an
  arbitrary escape). It still respects the jump cooldown and the actor jump
  policy (`npcPolicyAllowsJump`).
- Live-tunable in `config/npc-difficulty.json` (hot-reloaded):
  `lowObstacleJumpEnabled` (true), `lowObstacleProbe` (1.3 m),
  `lowObstacleLowOffset` (-1.5), `lowObstacleHighOffset` (-0.3).

## Validation

### Build
`python build_agent.py` / `MIMITA_FORCE_LINK=1` -> `BUILD SUCCESS`.

### New selftest `--npc-low-obstacle-selftest` (PASS)
```text
ok  a low crate is detected as hoppable
ok  a tall wall is not treated as a hoppable low obstacle
ok  the actor crossed the low crate
ok  the actor hopped (rose) over the crate
```

### Regression battery (all PASS)
travel-progress, search-behavior (ramp + wall), navigation, movement-policy,
movement-commitment, movement-decision, nav-graph, utility, team-brain, gamemode,
cs-round, targeting, perception, nav-request, counterstrike-acceptance.

### Runtime (headless server, `dust2cyberiav4`, 6 NPCs, ~8 s)
```text
exited 0; per-NPC net/path 0.38-0.90 (directed travel)
events: npc.jump 6, npc.stuck 595, npc.wall-avoid 115, travel-goal-changed 6
```
The hop does not cause jump spam (6 jumps over 8 s for the whole squad).

## Human review still needed

Live round: confirm NPCs hop over low crates/ledges that catch their legs while
still refusing to hop tall walls, and tune the offsets/probe live if a specific
obstacle is misjudged.

## Files changed

`src/npc/npc-navigation.h`, `src/npc/npc-navigation.cpp`,
`src/npc/npc-difficulty-config.h`, `src/npc/npc-difficulty-config.cpp`,
`src/npc/npc.cpp`, `src/game/game-cli.cpp`, `config/npc-difficulty.json`.
