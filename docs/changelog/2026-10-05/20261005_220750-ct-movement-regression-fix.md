# CT NPC movement regression fix + live-tunable movement knobs

Date (UTC): 2026-10-05
EST timestamp: 2026-10-05 18:07:50 EDT
Branch: `afad20a-rebuild` (working tree was clean at start of this session)

## Result

`PASS_WITH_HUMAN_REVIEW`

Human report: "after these changes CT NPCs keep running into the wall, jump a
lot, don't pursue one direction, net movement very small (≈20 m over 90 s), and
it got shorter after your edits." Root-caused and fixed; live acceptance still
required.

## Why it got worse (root causes of the regression)

1. **Waypoint arrival was impossible, so the actor orbited the first waypoint.**
   Navigation waypoints are floor surfaces (feet height, z≈0) while
   `body.pos` is the capsule center ≈1.8 m above the floor. The reach test
   compared the waypoint z to the body z with a ±1.3 m tolerance, so *no* walk
   waypoint was ever "reached"; `pathIndex` stayed at 0 and the actor circled
   the first node. The new long travel goals made this far worse than the old
   12 m body-relative waypoint, which changed every tick.
2. **Recovery jumps fired while following a route.** Jumping made the actor
   airborne, and its air control is too weak to turn, so it orbited the
   waypoint in the air (61 % of sampled time airborne; ~97 k jumps, 99.99 %
   `obstacle`).
3. **The navigator's "arrive and stop" was not applied.** `moveDir` kept the
   tactical/committed direction unless a route was active, so the actor ran
   past its objective and off the map instead of holding.
4. **The persistent travel target flipped on heading reversal**, turning the
   60 m goal 180° whenever the committed direction changed.
5. **Frequent replans** on the instantaneous "wall within probe" flag
   (`commitmentBlocked`) churned the route; commitment directions were replaced
   the instant a wall was sensed.

## Fixes

- `kReachXZ` 1.0→1.5 and arrival now ignores waypoint height below the actor
  (`kReachAbove`); only a waypoint meaningfully ABOVE the actor needs climbing.
- Recovery hops (obstacle, wall-climb, stuck) are skipped while a route is being
  followed to a non-visible goal (`navHasPathThisTick`); legal navigation jumps
  (traversal) and combat jumps still apply.
- The navigator now owns non-combat steering whenever it has a valid target,
  including a zero direction (arrive → stop).
- `resolveExploreTarget` holds the target until reached or genuinely stalled
  (`travelTargetHoldSeconds` / `travelTargetMinProgressMeters`); a heading
  reversal no longer retargets.
- `updateCommitment` keeps the committed direction while progress continues and
  the hold has not expired, even if a wall is sensed ahead; it replaces only on
  progress failure/timeout. `npc.movement-commitment-blocked` is emitted only on
  the replacement edge. The navigator replans on real `isStuck`, not on the
  instantaneous blocked flag.
- `makeNavGoal` adds a 4 m hold radius for objective goals so the actor stops at
  the objective.
- Nav-graph routes are simplified (collinear merge) and decimated at 5 m.

## Live-tunable movement knobs (`config/npc-difficulty.json`, hot-reloaded)

Documented and wired; edit while the game runs (client + server both poll the
file):

| Key | Default | Meaning |
|---|---|---|
| `wallAvoidMinProbe` | 3.0 | floor for the local wall-avoid probe (m) |
| `jumpCooldownSeconds` | 0.45 | min seconds between recovery hops |
| `exploreDistanceMeters` | 60.0 | persistent exploration target distance |
| `exploreHoldSeconds` | 12.0 | hold before retargeting |
| `exploreMinProgressMeters` | 6.0 | required progress in that window |
| `useNavGraph` | false | lazy global walkable graph on/off |
| `navGraphChunkSize` | 32.0 | graph chunk edge (m) |
| `navGraphCellSize` | 2.0 | graph node spacing (m) |
| `navGraphMaxRoutes` | 3 | distinct equal-cost routes |
| `navGraphMaxDropHeight` | 14.0 | max drop the graph will link |

`useNavGraph` defaults **off** because the graph currently produces longer,
more wandering routes than the local planner in testing; it is fully wired and
can be enabled live. Changing `navGraphChunkSize`/`CellSize` invalidates the
cached graph automatically (difficulty revision change).

Per-profile pursuit/commitment knobs remain in `config/behavior-profiles.json`
(also hot-reloaded); the difficulty file overrides the exploration ones so the
whole squad can be retuned from one place.

## Validation

### Build
`python build_agent.py` / `MIMITA_FORCE_LINK=1` -> `BUILD SUCCESS`, relinked.

### New regression guard
`--npc-travel-progress-selftest`: a CT-preset/rage2 NPC with a far objective
behind a wall, 60 s. Before the fixes: net 24–91 m, net/path 2–7 %, ran off the
map. After: **net ≈115 m, ends ≈3.5 m from the objective, stops (move 0)**.

### In-binary selftests (all PASS)
```text
--npc-travel-progress-selftest    PASS
--npc-search-behavior-selftest    PASS (ramp climb + wall explore)
--npc-navigation-selftest         PASS
--npc-movement-policy-selftest    PASS
--npc-movement-commitment-selftest PASS
--npc-movement-decision-selftest  PASS
--npc-nav-graph-selftest          PASS (3 distinct routes around a wall)
--npc-utility / --team-brain / --gamemode / --cs-round / --targeting / --perception PASS
--counterstrike-acceptance-selftest PASS
```

### Runtime (headless server, `dust2cyberiav4`, 6 NPCs, ~8 s)
```text
exited 0; per-NPC net 27–54 m, net/path ratio 0.69–0.89 (was ~0.01–0.02)
events: npc.jump 18, npc.stuck 816, npc.nav-plan-created 6, nav-replan 24
(previous live session: ~97k jumps, 99.99% obstacle, net/path ~1–2%)
```

## Human review still needed

Live Dust2 round: confirm CT and T leave spawn, travel without circling, do not
run into walls, jump only occasionally, stop at objectives, and (optionally)
toggle `useNavGraph` live to compare. Bomb-site anchors/hot-reload/visible were
unchanged this session (see the layered-navigation changelog).

## Files changed

`config/behavior-profiles.json`, `config/gamemodes/counterstrike.json`,
`config/maps/dust2cyberiav4.json`, `config/npc-difficulty.json`,
`src/game/game-cli.cpp`, `src/gamemode/gamemode.{h,cpp}`,
`src/network/server-gamemode.cpp`, `src/network/server-npcs.cpp`,
`src/npc/npc.{h,cpp}`, `src/npc/npc-behavior.{h,cpp}`,
`src/npc/npc-difficulty-config.{h,cpp}`, `src/npc/npc-goal.h`,
`src/npc/npc-navigation.cpp`, `src/npc/npc-navigator.{h,cpp}`,
`src/npc/npc-utility.{h,cpp}`, `src/npc/team-brain.{h,cpp}`,
`src/npc/npc-nav-graph.{h,cpp}` (new).
