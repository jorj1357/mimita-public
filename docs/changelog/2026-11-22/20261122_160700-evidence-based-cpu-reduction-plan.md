# Evidence-based MiMITA CPU reduction plan

## Scope

Converted the measured CPU investigation into a staged proposal and appended it to existing issue `issue-ed3ac775283d`. No code, process, build, or runtime behavior was changed.

## Priority order

1. Capture matched process CPU, actual mode, actor counts, server phases, fresh client frame slices, logging volume, and build state; hold maxFrames=999 constant for the first comparison.
2. A/B the `70f3c051` change that logs Important `npc.dash` on every dash (1,002 events/min observed) against once-per-actor-life or buffered/rate-limited diagnostics. Measure event volume, CPU, NPC AI timing, and tick health.
3. Suspend NPC/combat simulation in zero-player rooms after a grace interval while preserving the server listener and room joinability. Verify <=1% whole-machine CPU in the quiet case and resume correctly on join.
4. Expose per-server PID, actual mode, players, and live NPC count in dev-loop status so a launch count of one is not mistaken for a mode roster of 66.
5. Refresh client frame diagnostics and investigate ragdoll scopes separately; test corpse budget changes only with visual validation.
6. Compare DLL build settings (10 jobs/high priority versus 4-6 jobs/normal) with matched source/cache and with/without game load; keep the build tuning separate from runtime CPU work.

## Acceptance

Quiet empty rooms: <=1% whole-machine CPU after grace and still joinable. Active authoritative simulation retains the 60 Hz contract. Active client CPU and frame-time distributions are measured at each cap; no 1% active-play promise. Native WPR stacks require authorized profiling rights; the previous attempt was denied with `0xc5585011`.
