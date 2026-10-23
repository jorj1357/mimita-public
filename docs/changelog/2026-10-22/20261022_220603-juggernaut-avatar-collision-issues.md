# Juggernaut avatar, collision reload, and death-time performance issues

- Task: Add three newly reported issues to the open gameplay issue inbox and index.
- Result: PASS_WITH_FOLLOW_UP
- Time: 2026-10-22T22:06:03Z (America/New_York display: 2026-10-22 18:06:03 EDT)
- Branch: `20261011zt`
- HEAD: `cbfa4b0df4e56b6f217c45dbe986b49df599b50a` (no commit created)

## Exact changes

### `docs/specs/open-gameplay-issues.md`

- Line 121 adds the avatar-texture regression report. Before, the intake only mentioned chopped textures in the earlier Dust2CyberiaV4 report; it did not record the user's report that avatar stretching returned after a previous fix. The addition says: `an older avatar-texture stretching issue, which had been fixed before, has returned.` The cause and details of the earlier fix remain unknown; the general visual spec allows some intentional world-texture distortion, so the issue is explicitly avatar-specific.
- Line 122 adds the live actor-collision JSON report. Before, there was no entry for capsule-size JSON edits failing to affect actors live. The addition records the requested behavior: capsule sizes for all actors should be editable and applied live during play. It leaves config ownership, reload, existing actor application, and safe update details to trace.
- Line 123 adds the Juggernaut death-time performance report with the supplied timestamp `2026-10-22 18:04–18:05`, the reported transition after two or three NPC deaths, FPS/stutter, and NPC movement appearing around 6 Hz versus 60 Hz. Corpse involvement is expressly a hypothesis.
- Lines 141-143 add the matching index rows.

## Review and validation

- Read `docs/ROUTER.md`, avatar and visuals specs, collision and actor-preset ownership including actor-preset hot-reload guidance, JSON configuration architecture, performance, Juggernaut, NPC behavior, and ragdoll specs.
- The avatar appearance entry remains a player-reported regression; no prior avatar-specific fix was conclusively identified in this intake. The visual spec's intentional texture distortion refers to world art, so the issue wording keeps avatar textures distinct.
- `git diff --check -- docs/specs/open-gameplay-issues.md` passed (Git reports the configured LF-to-CRLF conversion notice). Confirmed all linked owner documents exist.
- Documentation-only issue intake; no code, build, runtime scenario, or tests were changed/run. The timing and symptom details are the player's report, not independent measurements.

## Pre-existing work preserved

At the start of this turn, modified files included `config/analytics.json`, `devscripts/dev-loop.py`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/open-gameplay-issues.md`, `src/engine/engine-tick-render.cpp`, `src/engine/engine-tick.cpp`, `src/network/multiplayer-context.h`, `src/network/multiplayer-packets.cpp`, `src/network/multiplayer-tick.cpp`, and `src/perf/perf-frame.cpp`; `devscripts/perf-watch.py` and `docs/regressions/2026-10-22/` were untracked. Existing untracked changelogs covered gameplay intake, Juggernaut profiling/build/networking, and resource diagnostics (`20261022_195219` through `20261022_220447`). These were preserved.
