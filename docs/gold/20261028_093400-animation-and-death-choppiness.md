# Gold note: tracing and reducing animation and death choppiness

- Human update: 2026-10-28 09:34 EDT (`2026-10-28T13:34:00Z`)
- Follow-up human confirmation: 2026-10-28 09:45 EDT
  (`2026-10-28T13:45:00Z`) — “its smooth i ahvent had issues choppiness etc yet”
- Evidence window: runtime work completed through 2026-10-28 09:27 UTC
- Audience: gameplay, animation, audio, ragdoll, and performance contributors
- Purpose: preserve the measured causes, fixes, improvement, and reusable performance lesson.
- This note does not claim the overall 240-FPS target or every performance issue is solved.

## What made it choppy

The October 28 performance investigation isolated several costs that stacked up
during actor animation and deaths:

- `interpolateAnimClip` allocated an `unordered_map` and `vector<string>` for
  each actor on every tick. The selector also built a `nlohmann::json`
  candidate array every tick even when priority debugging was disabled.
- Animation hot reload checked file timestamps on the gameplay thread: avatar
  animation dependencies every 100 ms and more than 100 weapon/global JSON
  files every 250 ms. `WeaponData` and `WeaponConfig` duplicated part of that
  polling.
- Walk/run selection repeatedly crossed the 12 m/s threshold and restarted
  clips. One Juggernaut run logged 75,689 `anim.state.changed` events in about
  two minutes.
- Six corpses used the 24-iteration alive-ragdoll solver, self-collision, and
  two world-depenetration passes per tick. Frequent corpse heartbeats, pose
  traces, and blood added more work.
- Changing to an Opus music track could launch FFmpeg synchronously on the
  gameplay thread; one `Audio` scope reached 701.9 ms.

These are measured/source-traced causes from the run recorded in the related
changelog, rather than assumptions that animation by itself was expensive.

## What changed and what improved

The implementation moved JSON watching to a background worker, reused fixed
storage during animation sampling, skipped candidate JSON construction unless
debugging requested it, rate-limited state-change diagnostics, added walk/run
hysteresis, reduced corpse-only solver and trace work, and moved Opus decoding
off the gameplay thread.

In build 3633's 32-avatar Juggernaut run, `anim.state.changed` fell from 75,689
to 1,244. Steady 30–45 second windows averaged 5.5–8.8 ms, with p95 values of
8–16 ms per 60-frame window. The 701.9 ms music spike was gone. The player
reported that the previously very bad choppiness seemed much better as of
09:34 EDT. That is a strong human improvement signal and agrees with the
measured reductions.

This was a substantial improvement, not proof that all frame pacing is fixed:
the 240-FPS-for-60-seconds target was not achieved. Large snapshot/NPC reset
bursts, first-time avatar/animation binding, rendering at 32 actors, and mass
death sound startup remained measured costs in that investigation.

## Reusable lesson

Always count the work at its real repetition rate. A small allocation,
container build, JSON operation, diagnostic event, or file check may look
cheap once, but when it runs for every actor on every 60 Hz tick, the cost
scales with both actor count and tick rate. Prefer reusable bounded storage,
event-driven/background file watching, state-transition hysteresis, and
rate-limited diagnostics. Profile and compare equivalent live runs before
claiming a change solved the frame-time problem; keep unrelated remaining
spikes visible rather than hiding them in an average.

## Related records

- [Detailed evidence, code, numbers, and method](20261028_094500-animation-death-choppiness-evidence.md)
- [Performance regression record](../regressions/2026-10-28/animation-death-frame-choppiness-REG.md)
- [Investigation, implementation, and runtime evidence](../changelog/2026-10-28/20261028_041903-plan-a-gameplay-diagnostics.md)
- [Performance specification](../specs/performance/performance.md)
- [Open gameplay issues](../specs/open-gameplay-issues.md)
