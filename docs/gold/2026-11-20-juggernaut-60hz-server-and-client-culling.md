// 2026-11-20T04:55:00Z
/* purpose
* preserve the measured path that took Juggernaut from ~11 Hz to ~60 Hz server
* explain the client culling improvement and its limits
* record the repeatable diagnosis and performance method
* this file DOES NOT claim 240 FPS, 999 FPS, or human acceptance
* this file DOES NOT replace the specifications or the runtime journals
*/

# Gold reference: Juggernaut 60 Hz server and client culling

- Timestamp: `2026-11-20T04:55:00Z`
- Audience: MiMITA developers and AI agents working on performance, NPCs, rendering, or networking
- Related specs: [performance](../specs/performance/performance.md), [culling and work avoidance](../specs/performance/culling-and-work-avoidance.md), [collision](../architecture/collision/collision.md), [agent coordination](../specs/dev-loop/agent-coordination.md)
- Related builds: generation 390 (baseline), generation 394 (current)
- Related runs: `20261120_032618`, `20261120_040807`

## What the user saw

Before the fix, Juggernaut had 67 NPCs but the server ran around 11–13 Hz with
100–573 ms fixed-tick stalls. The client looked worse than the tick rate alone
would suggest because it was also rendering all remote actors, including those
outside the camera.

The user's goal is not “a good average.” It is a consistent 240 FPS visible
competitive mode, with 999 FPS as an aspirational engineering target. A high
average with 400–2,300 ms spikes is a failure.

## The server 60 Hz cause

The dominant fix is in commit `d4898e0b`. The important changes were:

1. **Per-tick navigation graph budget**
   - `NpcNavGraph::findRoutes` now has an 8 ms shared budget across all actors
     for one simulation tick.
   - An actor that misses the budget keeps its previous route or falls back to
     the local planner instead of consuming the whole tick.
   - This prevents several NPCs replanning on the same tick from each spending
     the entire 16.67 ms budget.

2. **Stuck-recovery cooldown**
   - The expensive deep inside-solid check was repeating every tick for an actor
     already flagged stuck even when its root was clear.
   - It now rechecks at 30-tick intervals.
   - This removed a dominant repeated fixed-tick cost.

3. **Cached and bounded NPC spawn resolution**
   - NPC spawn safety now uses a bounded candidate search.
   - The result is cached by NPC, raw spawn, and geometry revision.
   - A bad spawn can no longer stall a fixed tick for seconds.

4. **NPC collision solid bounds**
   - The NPC world rebuilds solid bounds for its collision mesh.

5. **Server world-phase instrumentation**
   - `server.world-phase-window` reports infinite collision, NPC simulation,
     position history, recorder, projectiles, hazards, melee, physical contact,
     and impacts separately.
   - This made it possible to stop guessing which phase was responsible.

The baseline journal showed NPC AI around 818–888 ms per second and stalls up
to 573 ms. Generation 394 shows NPC AI around 65–97 ms/s, loop average around
8–14 ms, and server windows around 59.3–60.3 Hz with 67 NPCs.

The fix was not “reduce the NPC count” and not “hide the work.” It bounded
repeated work, cached a bounded result, and gave expensive actors a fallback.

## The client culling cause

Commit `ce0009d5` adds conservative frustum culling for remote players/NPCs
before `renderNetworkPlayer` and remote weapon rendering.

Baseline gen 390:

- client 8–15 FPS;
- `Rendering::Actors::NetworkEntities` up to ~922 ms;
- only 6–7 of 67 remote NPCs in view;
- `Ragdoll::WorldCollision` up to ~810 ms.

After culling:

- `NetworkEntities` commonly ~0.5–2.5 ms;
- client roughly 60–120 FPS depending on scene/effects;
- the culling did not fix ragdoll, snapshot, UI, or animation spikes.

Culling is a work-avoidance change, not a gameplay change. The server remains
authoritative and every replicated actor still exists; only the presentation of
actors outside the camera is skipped.

## What is still wrong

Generation 394 is not 240 FPS consistent. Observed evidence includes:

- user screenshot: `FPS 179 avg 138`, `FRAME 5.37 ms`, `MAX 2368.59 ms`;
- UI `0.21 ms` in that sample, but other samples show `HUD::Game` ~11.5 ms and
  `UI` ~21.6 ms;
- `PlayerAnimation` up to ~88 ms in a spike;
- `Network::SnapshotApply::InterpolationInsert`, `Ragdoll::WorldCollision`,
  `Rendering::Effects::EffectParts`, and `Swap` all appear in spike scopes;
- one-second snapshots/diagnostics can hide the frame distribution.

So the next work is not more average tuning. It is:

- trace the 2368 ms max frame and every >100 ms spike;
- add UI subphase scopes and remove UI spikes;
- cull/LOD effects, ragdolls, remote actors, shadows, and offscreen UI;
- reduce network snapshot application and per-frame polling;
- keep the server at 60 Hz while doing it;
- define 240 FPS as a 100%-of-frames controlled-window gate on a capable
  high-refresh display;
- report 999 FPS separately as aspirational, not as the current gate.

## Repeatable method

```text
visible problem
-> exact run/build/generation/journal
-> per-frame distribution, not average FPS
-> ranked scope and spike call tree
-> identify repeated/unnecessary work
-> bound, cache, cull, or LOD the work
-> root dev-loop build
-> pinned executable + game generation run
-> same scenario + journal
-> compare frame distribution and correctness
```

This is the method to reuse. It works for server NPCs, client rendering, UI,
network, and collision because it starts with measured ownership and ends with
real journal evidence.

## Remaining acceptance

- 240 FPS every accepted frame on a capable visible display.
- 999 FPS aspirational measurement, not required now.
- No unintended reconnect/connection loss.
- Arcade capsule, seams, slopes, and no-penetration collision.
- AI player controller and NPC target-visibility behavior.
- Screenshot-at-tick visual proof.
- Human visual and feel acceptance.
