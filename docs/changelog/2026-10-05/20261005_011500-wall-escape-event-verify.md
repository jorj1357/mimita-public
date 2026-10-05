# Counter-Strike wall-escape: fix stale test fixture + verify JSONL event

Date (UTC): 2026-10-05
EST timestamp: 2026-10-04 21:15:00 EDT
Branch: `afad20a-rebuild`
Commit at handoff: `a1bcea51` (working tree dirty with unrelated concurrent work)

## Result

`PASS_WITH_HUMAN_REVIEW`

Source, build, pure-test, and in-binary runtime evidence are proven. No live
Counter-Strike match was played (the human asked for build + tests, not a live
visual check). Human acceptance of the CT spawn escape remains required.

## Scope

Finish the deferred items from
`docs/specs/20261005-counterstrike-wall-escape-handoff.md`: resolve the one
failing automated check (`--npc-movement-policy-selftest`), prove the
`npc.wall-escape` diagnostic actually reaches `events.jsonl`, and stop it from
flooding. Option B (authoring real bomb sites) was explicitly NOT done.

## Files changed (this session only)

### `src/game/game-cli.cpp`

1. Added includes at the top:
   - line ~50 `#include <fstream>` (read the JSONL back)
   - line ~52 `#include "debug/structured-log.h"`
   - line ~69 `#include "npc/npc-difficulty-config.h"`

2. `--npc-movement-policy-selftest`, Phase 2 (line ~408): replaced the
   hard-coded wall X with one relative to the actor.

   Old:

   ```cpp
   // Phase 2: wall ahead. ...
   addTri(world, {4,-3,0}, {4,3,0}, {4,3,4});
   addTri(world, {4,-3,0}, {4,3,4}, {4,-3,4});
   ```

   New:

   ```cpp
   // Phase 2: wall ahead. Place it relative to the NPC's current position
   // so it is always directly in front regardless of how far Phase 1
   // advanced; an absolute X becomes stale as movement tuning changes and
   // would leave the wall behind the actor. ...
   const float wallX = npc->body.pos.x + 3.0f;
   addTri(world, {wallX,-3,0}, {wallX,3,0}, {wallX,3,4});
   addTri(world, {wallX,-3,0}, {wallX,3,4}, {wallX,-3,4});
   ```

3. Added `--npc-wall-escape-event-selftest` (line ~893). It initializes the
   real `StructuredLogger`, builds a closed wall pocket, drives a real
   `counter_strike`-preset NPC through `NpcSystem::updateOneNpc` for 180 ticks,
   shuts the logger down, then reads the canonical events file back and asserts:
   - the events path is available;
   - `npc_movement` allows `Important` (the level gate);
   - at least one `"event":"npc.wall-escape"` record exists;
   - at least one has `"preset":"counter_strike"`;
   - the count is `< 180` (rate-limited, not once per tick).

### `src/npc/npc.cpp`

Rate-limited the backtrack diagnostic (line ~1574-1598). The `open_turn_repath`
event was already once-per-episode; the backtrack event was not, and fired on
every tick an actor was pinned. Added an episode guard before emitting:

```cpp
const bool newBacktrackEpisode = !npc.navigator.backtrackActive;
npc.navigator.startBacktrack(...);
...
if (newBacktrackEpisode)
{
    StructuredLogger::instance().writeEvent(
        StructuredCategory::NpcMovement, StructuredLevel::Important,
        "npc.wall-escape", ..., "backtrack", ...);
}
```

## Reasoning

### Why the selftest failed

The handoff suspected Option A (un-gating the escape) had broken
`--npc-movement-policy-selftest` (`lateral=0.49`). Instrumenting the test showed
the actual cause: after Phase 1 the actor was already at **x=15.17**, so the
Phase 2 wall at absolute `x=4` sat *behind* it. The actor never met a wall,
walked off the floor edge, and `maxLateral` (~0.49) was just drift. The
assertion ("NPC made a local lateral correction at the wall") was never
obsolete; the fixture was stale. Placing the wall at `pos.x + 3` makes it
genuinely ahead, and the actor now corrects laterally (`lateral=0.64`, PASS)
without random wandering. This is the smallest correct fix and does not loosen
the assertion.

### Why the diagnostic needed a guard

With the pocket selftest, the unguarded backtrack event wrote **90 records in 90
ticks** (one per tick), which violates `AGENTS.md` ("keep repeated diagnostics
categorized, useful, and rate-limited") and the handoff's own no-flood intent.
The episode guard reduced it to **1 record in 180 ticks** (the actor cannot make
progress, so the same backtrack stays active).

## Documents and skills used

- `AGENTS.md`, `docs/ROUTER.md`
- `docs/specs/20261005-counterstrike-wall-escape-handoff.md`
- `docs/specs/debug-logging/debug-logging.md`, `docs/specs/debug-logging/canonical-jsonl.md`
- `docs/skills/logging-checker-v1.md` — result: PASS. The event has an owner
  (`npc.cpp`), category (`npc_movement`), level (`Important`), reason
  (`backtrack` / `open_turn_repath`), actor/correlation id, and is now
  rate-limited. No per-frame spam remains.
- `docs/skills/spec-behavior-review-v1.md` — result: PASS. The failing test was
  a code/fixture disagreement, not a spec disagreement; the spec
  (`docs/specs/20261002plan.md`: "path recovery around walls", "no repeated
  wall-running or stuck loops") still requires the lateral correction, which the
  fixed fixture proves.
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/operations/task-completion/task-completion.md`

## Validation

### Build

```text
python build_agent.py            -> BUILD SUCCESS, mimita.exe relinked
(MIMITA_FORCE_LINK=1 used to reconcile the background dev-loop's
 compile-without-relink; see environment note)
```

### Pure tests

```text
build/npc-movement-executor-test.exe  PASS (15 checks)
build/npc-navigation-test.exe         PASS (34 checks)
build/npc-movement-policy-test.exe    PASS (72 checks)
```

### In-binary selftests

```text
--npc-movement-policy-selftest        PASS
  lateral=0.64  (was 0.49 FAIL)
--npc-wall-escape-event-selftest      PASS
  ok  canonical events path is available
  ok  npc_movement category allows Important
  ok  counter_strike test actor spawned
  info npc.wall-escape records=1 counter_strike=1
  ok  at least one npc.wall-escape record landed in events.jsonl
  ok  the recorded escape came from the counter_strike actor
  ok  backtrack diagnostic is rate-limited (not once per tick)
--npc-navigation-selftest             PASS
--npc-movement-executor-selftest      PASS
--counterstrike-acceptance-selftest   PASS
```

Exact verified record (from
`logs/10-04-2026/20261004_211409/events.jsonl`):

```json
{"category":"NPC_MOVEMENT","correlation_id":"9501","event":"npc.wall-escape","fields":{"actor":9501,"blocked_dir":[0.9999716,0.0075323],"policy":"turn_then_repath","pos":[0.0,0.0,2.0],"preset":"counter_strike","team":0},"func":"updateOneNpc","level":"IMPORTANT","line":1596,"process":"client","reason":"backtrack","tick":0,"wall_time":"2026-10-05T01:14:09.187Z"}
```

`team:0` is the Counter-Terrorist side; `preset:"counter_strike"` and
`policy:"turn_then_repath"` confirm the actor is a CS policy actor at spawn.

## Environment note

The background dev-loop watcher compiles changed objects but does not always
relink, and `build_agent.py` only links when it compiles something in that run
(`needs_link = compiled_count > 0`). `MIMITA_FORCE_LINK=1 python build_agent.py`
forces the relink. This is an environment artifact, not a code defect.

## Pre-existing / unrelated changes

The working tree still contains unrelated concurrent edits (Option A itself,
the unified-executor work, bomb-site placeholders, new `src/gamemode/*` and
`src/npc/npc-movement-context.*`, config edits). They were not reverted and are
not claimed by this session.

## Human review still needed

1. Start `counterstrike` on `dust2cyberiav4`, pick CT, and confirm CT NPCs leave
   the spawn wall instead of holding it (the original report).
2. Confirm `npc.wall-escape` records appear for CT actors during a real round
   (read `logs/<date>/<run>/events.jsonl`; no visual eyeballing required).
3. `npc_movement` must stay at least `important` in `config/debuglogger.json`
   or the events are dropped by the level gate.
