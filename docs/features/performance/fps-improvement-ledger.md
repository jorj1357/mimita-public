# FPS improvement ledger

Stable record of changes that measurably improved client or server FPS/frame
time. Build success is not performance evidence. Each row links the runtime
evidence that proved the change.

Related:

- `docs/specs/performance/performance.md`
- `docs/specs/performance/perf-report-overlay.md`
- `docs/regressions/performance/README.md`

| Date | Change | Before | After | Evidence | Status |
|---|---|---|---|---|---|
| 2026-11-16 | Honest completed-frame accounting | False `UNACCOUNTED 60.91 ms` | `UNACCOUNTED 0.04 ms`; host/game buckets visible | overlay screenshot; `performance.report` | verified |
| 2026-11-17 | Weapon collider distance guard | Query box ~`115.8 x 138.1 x 451.6 m`; `2242` candidates; `solve_ms ~25.5` | Query box ~`1.2 x 1.8 x 3.7 m`; `2` candidates; `solve_ms ~0.32` | `collision.solve.summary` | verified |
| 2026-11-17 | NPC decision stagger | `updates=907/s`, `sense_runs=907`, `navigation_runs=907`, `skipped=0` | `updates=227/s`, `navigation_runs=77`, `navigation_skipped=150` | `npc.phase-window` | partial; server still below 60 Hz |
| 2026-11-17 | Performance report modes + overlay self-cost | Full report every frame | Mode 2 default, 1 Hz sample, `OVERLAY 0.05–0.12 ms` typical | `performance.report`, `Perf::Overlay` | verified for overlay cost |
| 2026-11-17 | Event-driven build readiness + keep-open publish | Live client replaced after some builds; build wait required | Build publishes without replacing a live client; completion event recorded | dev-loop journal; PID unchanged | source implemented; runtime review pending |
| 2026-11-17 | Conditional `ActorPair::FinalPose` + clean payload baseline | Client ~7–14 FPS; `ActorPair::FinalPose` ~26–51 ms | Client ~23 FPS; second final-pose sample skipped on unchanged ticks | `performance.report`, `performance.fps`, run `20261117_170234` | partial; `Simulation::FinalPose` still large |
| 2026-11-17 | Idle final-pose cadence + connection-independent client path | Prior idle client ~23 FPS after generation 318 | client-02 ~96–106 FPS idle on generation 320, p50 ~5.6–6.1 ms | `performance.fps`, `performance.report`, client-02 | partial; not connected gameplay |
| 2026-11-17 | Projectile catch-up cap + NPC 4-tick decision cadence | Unbounded projectile catch-up; NPC decisions every 3 ticks; 8 plans/s | Max 2 steps/projectile, 12 steps/tick; 4-tick decisions; 4 plans/s | source implemented; needs projectile-active clean window | needs runtime |

## Entry rules

- Add a row only when runtime evidence exists.
- Record before/after values and the exact journal/event source.
- A change that only reduces a profiler's own cost is labeled as overlay cost,
  not game FPS.
- A partial change says `partial` and names the remaining blocker.
- Link a regression detail file when the change fixes a confirmed bad behavior.
