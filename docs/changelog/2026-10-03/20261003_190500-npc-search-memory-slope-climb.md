# NPC search behavior: climb walkable slopes, remember where it was, stop wall-ramming

Date: 2026-10-03
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, and automated (real `NpcSystem`) evidence are proven. No live
sandbox/multiplayer visual acceptance was performed; the human checklist below
remains required.

## Scope

Follow-up to the actor-preset NPC movement policy (commit `5f59a4d2`). Human
playtest pass 7 reported: NPCs re-test the same wall, turn into an open
direction then turn back into the wall, get stuck at the bottom of slopes, and
pacing one spot instead of exploring. This change makes the generic search
behavior start from the ground up:

1. Walkable slopes are no longer mis-detected as walls (climb them).
2. Search remembers visited and blocked world points and never doubles back.
3. A search heading is committed for much longer.
4. A no-progress watchdog breaks slope-bottom/corner lock-ups.

All tuning lives in `config/npc-difficulty.json`, so it applies to sandbox and
legacy NPCs (no actor preset required) and hot-reloads live. No mode-specific
branch was added.

## Root causes

- `NpcNavigation::obstacleInDirection` cast a horizontal ray at `pos.z + 0.5`
  and treated *any* hit as a wall. On a rising ramp that ray hits the ramp, so
  every walkable slope read as a wall and the NPC refused to step onto it.
  `wallAvoidDirection` used the same test.
- The navigator's local grid accepted `normal.z >= 0.7` while physics only
  treats `normal.z >= 0.80` (`MAX_WALKABLE_SLOPE_DOT`) as walkable — a mismatch
  that could plan onto ground the body cannot hold.
- `updatePatrolHeading` re-sampled every tick while blocked (the commit timer
  only applied when *not* blocked), with a weak projection-only recency score,
  causing left/right/back oscillation and re-testing the same wall.
- The recency ring was only 8 points at 1 Hz and had no memory of blocked spots,
  and nothing forced a new heading when the actor made no progress.

## Files changed

### Terrain-aware sensing — `src/npc/npc-navigation.{h,cpp}`
- Added `NpcNavigation::kWalkableSlopeDot = 0.80f` (matches physics).
- `obstacleInDirection` returns true only for a non-walkable (steep/vertical)
  hit; a walkable ramp is terrain, not a wall.
- `wallAvoidDirection` forward and alternative-direction checks use the same
  walkable filter (a ramp no longer triggers avoidance; ledges still do).

### Navigator alignment — `src/npc/npc-navigator.cpp`
- `kWalkableNormalZ` now uses `NpcNavigation::kWalkableSlopeDot` (0.80) so the
  local A* only plans onto ground the body can actually walk.

### Search memory + commitment — `src/npc/npc-state-machine.h`, `npc.cpp`, `npc-states.cpp`
- `NpcStateMachine` now keeps `patrolVisited[]` and `patrolBlocked[]` timestamped
  rings (32 each), a `patrolSnapshotTimer`, and a no-progress watchdog
  (`patrolNoProgressTimer`, `patrolLastProgressPos`, `patrolForcedDetour*`).
- `updatePatrolHeading` rewritten: commits a heading until it is blocked or
  `searchHeadingCommitSeconds` elapses; candidate scoring uses a lookahead
  sample with a heavy penalty near visited/blocked points, a strong continuity
  bonus, and an openness bonus (prefer directions with a longer line of sight).
  Blocked spots are recorded; the planner no longer re-samples every tick.
- A generic no-progress watchdog runs in every state: in Patrol it forces a
  fresh heading; while chasing it blacklists the spot, requests a nav repath,
  and applies a short lateral detour so the actor physically leaves the spot.

### Tuning — `src/npc/npc-difficulty-config.{h,cpp}`, `config/npc-difficulty.json`
New hot-reloadable fields with `comment_*` docs:
`searchHeadingCommitSeconds` 6.0, `searchMemorySeconds` 12.0,
`searchMemoryPoints` 24, `searchSnapshotSeconds` 0.4, `searchLookahead` 4.0,
`searchAvoidRadius` 3.0, `searchNoProgressSeconds` 1.5.

### Test — `src/game/game-cli.cpp`
New `--npc-search-behavior-selftest` builds a real `World` + `NpcSystem`.

## Evidence

### Source / config
- Files above; `config/npc-difficulty.json` parses (comments stripped) and
  exposes the seven `search*` fields.

### Build
- `python build.py build-only` (after removing changed objects): `BUILD
  SUCCESS` (exit 0), `[LINK] mimita.exe`. Note: a background dev-loop is also
  rebuilding objects during the session; all objects newer than the changed
  headers were verified current at link time.

### Automated (`mimita.exe --...`)
- `--npc-search-behavior-selftest`: PASS — "NPC advances onto the ramp", "NPC
  climbs the walkable slope (z rises)", "blocked search never uses
  random/circle/strafe/zigzag", "NPC turns and explores sideways along the
  wall", "NPC keeps covering ground instead of pacing one spot", "NPC never
  stalls in one spot longer than ~6s".
- `--npc-movement-policy-selftest`: PASS (`lowHealthRetreats=257/300 ...`).
- `build/npc-movement-policy-test.exe`: PASS (72 checks).

### Runtime / human
Not performed. Live sandbox play on `dust2cyberiav4` (per pass 6: NPC behavior
only, in sandbox) is required.

## Human acceptance checklist

1. NPCs walk up walkable ramps instead of turning away at the slope bottom.
2. An NPC hitting a wall turns once and keeps going; it does not turn back into
   the same wall.
3. A blocked NPC keeps covering new ground and does not pace one spot.
4. No NPC stays stuck in one place for more than a couple of seconds.
5. Edit a `search*` value in `config/npc-difficulty.json`, save, and confirm
   the behavior changes live without a restart.

## Notes / limitations

- Steeper-than-walkable slopes (`normal.z < 0.80`) are routed around, not
  climbed, per the agreed slope policy. Climbing those would need a later
  jump-climb path.
