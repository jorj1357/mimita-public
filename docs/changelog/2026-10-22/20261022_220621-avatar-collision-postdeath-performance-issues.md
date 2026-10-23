# Avatar, live collision, and post-death performance issue intake

- Task: Add three player-reported issues to the open gameplay issue inbox.
- Result: PASS_WITH_FOLLOW_UP
- Time: 2026-10-22T22:06:21Z (America/New_York display: 2026-10-22 18:06:21 EDT)
- Branch: `20261011zt`
- HEAD: `cbfa4b0df4e56b6f217c45dbe986b49df599b50a` (no commit created)

## Exact changes

### `docs/specs/open-gameplay-issues.md`

- Line 121: added a report that avatar texture stretching returned after a previously reported fix. Before, the inbox only mentioned chopped avatar textures in an earlier Juggernaut/Dust2CyberiaV4 report; it did not say that an older fix was believed to have regressed. The user reports the return, while the earlier fix details and cause remain unknown. The issue distinguishes avatar defects from intentional world-texture distortion.
- Line 122: added the request for JSON-controlled actor collision capsule sizes to take effect live for all actors. Before, no issue said the player's JSON size edits did not change current collision shapes. The route asks to trace config reload, actor-profile application, and collision reads, including current actors and safe consistency.
- Line 123: added the Juggernaut performance report with the exact supplied date/time, `2026-10-22 18:04–18:05`: starts smooth; after roughly two or three NPC deaths, reported stutter/FPS loss and NPC motion appearing around 6 Hz rather than 60 Hz. Corpse involvement is marked as a hypothesis only.
- Lines 141-143 add matching index rows.

## Review and validation

- Read `docs/ROUTER.md`, avatar and visuals specifications, collision and actor-preset ownership (including hot-reload guidance), JSON configuration architecture, performance, Juggernaut, NPC behavior, and ragdoll specifications.
- Whole-docs TODO scan found relevant existing notes: `docs/specs/avatar/avatar-editor.md:5` says `also todo clean this doc!!!!!! cant have so much stuff inthis doc just spam`; suggested follow-up is to reorganize the guide around user tasks while keeping the active file/field routes easy to find. `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:17` says `todo: write currentebhavior of how it works right now`; suggested expansion is to document current corpse/ragdoll lifecycle and simulation ownership, which would help the post-death performance investigation. Neither note was edited.
- Confirmed all issue owner links exist. `git diff --check -- docs/specs/open-gameplay-issues.md` passed (Git reports the configured LF-to-CRLF conversion notice).
- Documentation-only intake; no implementation, build, runtime scenario, or tests were performed. The timestamp and performance observations are the player's report, not independently measured here.

## Pre-existing work preserved

At the start of this turn, modified files included `config/analytics.json`, `devscripts/dev-loop.py`, `docs/specs/dev-loop/dev-loop.md`, `docs/specs/open-gameplay-issues.md`, `src/engine/engine-tick-render.cpp`, `src/engine/engine-tick.cpp`, `src/network/multiplayer-context.h`, `src/network/multiplayer-packets.cpp`, `src/network/multiplayer-tick.cpp`, and `src/perf/perf-frame.cpp`; `devscripts/perf-watch.py` and `docs/regressions/2026-10-22/` were untracked. Existing untracked changelogs covered gameplay intake, Juggernaut profiling/build/networking, and resource diagnostics. These were preserved.
