# Client frame budget remains unmet

Date: 2026-11-15
Time last updated: 2026-11-15T02:21:00Z
Status: OPEN — measured, not accepted
Owner: client frame simulation/render/pacing

Related:

- `docs/specs/performance/performance.md`
- `docs/specs/hotreload/hot-client-live-fix.md`

## Symptom

The client still misses the 240 FPS / approximately 4.17 ms frame target. The
problem is visible as low or unstable FPS, not as a single missing feature.

## Measured evidence

Run `20261115_005633`, steady 60-frame resource windows:

- `client-01`:
  - average: 28.0 ms, then 49.9 ms
  - p95: 81.6 ms, then 128.4 ms
  - max: 115.5 ms, then 242.3 ms
  - frames over 4 ms: 100%
- `client-02`:
  - average: about 10.0–10.4 ms
  - p95: 15.4–20.9 ms
  - max: 63.9–72.8 ms
  - frames over 4 ms: 100%

Representative spike scopes:

- `Simulation::Lifecycle`: 51–95 ms in the worst frame
- `ActorPairCollision`: 21–29 ms
- `Setup::HotReloadDLL`: 42–75 ms
- `Combat::PostWeaponContainment`: up to 22 ms
- `Rendering::Shadows`: up to 13 ms
- `Swap`: up to 30 ms
- `UI::End`: up to 13 ms

`Rendering::Actors::Cosmetics` measured about 0.1 ms CPU in the same windows.
Cosmetics are therefore not the primary measured CPU cost. A screen-covering
cosmetic could still add GPU fill cost; that needs GPU timing or an A/B test.

## Current interpretation

- The client frame budget is not met.
- The frame is not dominated by one small cosmetic draw.
- The largest recurring costs are simulation lifecycle, actor-pair collision,
  hot-reload/module polling, weapon containment, shadows, UI, and swap.
- The next useful measurement is one steady 60-second window after the hot
  client no longer stalls, including GPU timing when available.

## A/B test to run

1. Disable the Jason cosmetic / all cosmetics for one client.
2. Record a 60-frame resource window.
3. Re-enable cosmetics.
4. Record the same window.
5. Compare CPU scopes and GPU frame time.

Do not mark this fixed from a cosmetic change alone.

## Acceptance

- 60-frame windows remain under the target budget, or the remaining over-budget
  cost is explained and owned.
- The target hardware and CPU/GPU/frame-pacing limit are recorded.
- Runtime journal evidence is attached.
- Human visual/feel review confirms the result.
