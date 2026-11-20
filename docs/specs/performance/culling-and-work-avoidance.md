// 2026-11-20
/* purpose
* make work avoidance and visibility culling a permanent performance rule
* list the high-value efficiency backlog so agents can implement it over time
* require measurement before and after every work-avoidance change
* this file DOES NOT remove authoritative gameplay simulation or replication
* this file DOES NOT lower visual quality without a recorded decision
*/

# Culling and work avoidance

## Principle

Do not draw, animate, simulate, replicate, lay out, allocate, or poll work that
cannot affect the player in the current frame or the authoritative simulation.

Culling is allowed only for presentation and irrelevant remote detail. Server
authority, fixed-tick collision, damage, health, movement, and required
replication remain intact.

This is a living backlog. Each entry must become a normal issue/task with an
owner, expected metric gain, evidence, risk, and a pinned-run acceptance check.

## Backlog

| Candidate | Expected result | Risk | Evidence gate |
|---|---|---|---|
| Frustum-cull all remote actors, weapons, nameplates, healthbars, effects, trails, projectiles, corpses, shadows | fewer draw/update calls | edge pop-in if margin is wrong | visible + headless frame scopes |
| Occlusion cull behind walls/rooms | skip hidden work | false culling at openings | replay at known walls |
| Distance and screen-size LOD | lower far-actor cost | visible quality loss | A/B frame and visual evidence |
| Freeze/throttle far remote animation | less animation cost | animation looks stepped far away | visible check |
| Skip offscreen ragdoll/corpse simulation | remove spikes | corpse pose differs | journal + visible check |
| Cap active effects/corpses; sleep settled ones | stable frame time | shorter visual trails | measured cap evidence |
| Network relevance filtering | fewer snapshots/remote updates | missing remote state | server/client correlation |
| Batch/instance actor rendering | fewer draw calls/binds | batching complexity | GPU/CPU scope evidence |
| Cull shadow casters/lights/volumes | lower shadow cost | missing soft shadows | visual + frame evidence |
| Cache static room/world visibility | less traversal | stale cache after edits | map rebuild test |
| UI dirty-region/widget redraw | lower UI cost | stale HUD state | UI scope + visible check |
| Cache text measure/layout/font atlas | lower text cost | wrong wrapping/font | UI scope |
| Event-driven config reload | remove per-frame polling | missed config change | config apply event |
| Prewarm assets/shaders | remove first-use spikes | longer startup | first-frame timing |
| Stop settled/stationary actors/effects | lower idle cost | delayed reaction | gameplay check |
| Frame-pacing alignment | less jitter/stutter | pacing bugs | p99.99/max frame |
| Remote snapshot/update rate by distance | lower network/presentation cost | stale remote pose | journal correlation |

## Required measurement

For every change record:

- build ID, game generation, run ID, and exact journals;
- frame-time p50/p95/p99/p99.99/max and frames over budget;
- affected scope, draw calls, allocations, update counts;
- actor/effect counts and view distance;
- visible and headless results separately;
- before/after evidence with the same workload;
- whether the change is presentation-only or affects gameplay authority.

A culling change cannot be accepted from an average FPS improvement alone.

## Acceptance

- The player-facing workload remains correct and authoritative.
- No offscreen work is performed for pure presentation when it can be skipped.
- Gameplay state remains replicated and valid.
- The measured frame-time distribution improves without hidden spikes.
- Visual quality is not silently reduced; intentional LOD is documented.
- Every accepted item links to a changelog, proof record, and any related
  regression/visual review.

## Agent workflow

1. An agent or the dev-loop observes a frame/spike/perf scope.
2. Open an `efficiency`/`performance` issue with evidence.
3. Triage into a bounded task with expected metric and risk.
4. Implement the smallest visibility/work-avoidance change.
5. Build through the root dev-loop, pin the artifact, run the real scenario.
6. Record frame scopes and visual evidence; use screenshots at the exact tick
   when visual proof is required.
7. Review by another agent; human final for quality/policy changes.

Agents may propose new backlog items for any area of the repo. Missing
knowledge about game state, intent, or ownership becomes an `awareness`/
`needs_info` issue rather than an assumption.
