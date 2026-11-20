// 2026-11-20T05:00:00Z
/* purpose
* record the culling/performance specs, visual proof screenshot spec, and issue intake
* separate documentation/specification work from runtime implementation
* this file DOES NOT claim the AI controller, collision, or screenshot code is implemented
* this file DOES NOT claim 240 FPS or human acceptance
*/

# Culling specs, AI controller issues, and visual proof intake

## Task

- Task ID: session-20261120-culling-specs-ai-controller-issues
- Owner: runtime-60hz
- Focus: permanent culling/work-avoidance backlog, 240 FPS consistency, visual
  screenshot proof, AI controller issues, collision issue tasks
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Status: specifications and issue records complete; code implementation pending

## What changed

### Performance specifications

- Added `docs/specs/performance/culling-and-work-avoidance.md` with a living
  backlog for frustum/occlusion culling, distance/screen-size LOD, offscreen
  animation/ragdoll/effect suppression, relevance filtering, batching, UI
  dirty regions, event-driven polling, prewarming, and frame consistency.
- Updated `docs/specs/performance/performance.md`:
  - 240 FPS is a per-frame gate: every frame in the controlled acceptance
    window must be at or below 4.167 ms;
  - a 239 FPS frame is a failure;
  - report p50/p95/p99/p99.99/max and spike/jitter history;
  - 999 FPS remains aspirational and is not required to be measured;
  - visible 240 requires a display capable of 240 Hz;
  - UI work must add subphase scopes for layout, text measure, font render,
    draw, panels, HUD, overlays, and culled/visible widget counts.
- Added `docs/skills/performance-culling-v1.md`.

### Visual proof at a tick

- Added `docs/specs/dev-loop/visual-proof-screenshots.md`.
- The command surface is `screenshot_at_tick <tick> [name]`; it captures after
  the UI pass and writes an identity-matched PNG plus JSON sidecar.
- The sidecar records build/generation/run/tick/camera/actor/UI identity and
  the journal; a missed tick is reported as `missed`, never relabelled exact.
- A screenshot is visual evidence only and must be paired with `events.jsonl`.
- Linked the workflow from `scenario-input-recording.md` and `ROUTER.md`.

### Agent coordination

- Updated `agent-coordination.md` with review-while-waiting, issue context
  packets, path ownership, and awareness/`needs_info` gaps.
- Updated `agent-self-improvement.md` with automatic efficiency/performance
  proposals from measured spikes and a continuous issue-progress loop.
- Updated `dashboard.md`: `required=true` means one dashboard exists and is
  reachable, not foreground; if it is already open, minimized, behind another
  window, or on another monitor, leave it alone.
- Added new culling/screenshot routes to `ROUTER.md`.

### Gold reference

- Added `docs/gold/2026-11-20-juggernaut-60hz-server-and-client-culling.md`.
- It records the server 60 Hz cause from `d4898e0b`:
  - 8 ms per-tick NPC nav-graph budget;
  - 30-tick stuck-recovery cooldown;
  - bounded/cached NPC spawn resolution;
  - NPC solid bounds;
  - server world-phase instrumentation.
- It separates the client frustum-culling improvement from the server result
  and records the remaining 2368 ms spike / 240 FPS consistency problem.

## New issues and tasks

- `issue-f8873319f12b`: Upgrade the local AI player controller (main fix).
- `issue-82cdc1277704`: Audit NPC target visibility and behavior behind walls
  (separate audit for other agents).
- `issue-89efbbc86535`: Capture a screenshot at a specific tick for proof.
- `issue-549680e53b26`: Frame consistency: no frame below 240 FPS.
- `issue-5f49047dae49`: Culling, LOD, and work-avoidance backlog.
- `issue-91e3384b219b`: Agent awareness gaps and continuous issue progress.
- `issue-da337d1fe2f3`: Updated with the exact ramp position
  `(-685.651, -22.853, 2351.122)` and tasks for capsule JSON, no-penetration
  slide, seams/slopes, and dust2 ramp reproduction.
- `arcade-collision-default` is active on the collision default work; this
  session did not edit the collision source/config.

## Evidence

- Documents and issue records were written and linked.
- The current pinned run `20261120_040807` is observed at roughly 59.3–60.3 Hz
  server with 67 NPCs; client samples range roughly 90–120 FPS, but spike
  samples show 2368 ms max, 155 ms frames, and UI/HUD/network/ragdoll scopes.
- No new game code was built by this session.
- No screenshot-at-tick implementation, AI controller upgrade, collision change,
  or 240 FPS acceptance is claimed.
- Existing focused tests were not rerun because this session changed
  documentation and issue records only.

## Remaining

- Implement screenshot-at-tick and link it to proof.
- Upgrade the local AI controller and audit NPC target visibility separately.
- Implement collision default/capsule/seam/slope fixes with the active
  collision agent.
- Trace UI and the 2368 ms spike; implement culling/LOD work in the new spec.
- Prove 240 FPS per-frame on capable visible hardware; keep 999 aspirational.
- Review-while-waiting and awareness queues need brain/CLI implementation.
