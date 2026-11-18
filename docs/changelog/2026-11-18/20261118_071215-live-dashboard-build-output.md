# Live dashboard build output

# Task

- Task ID: session-20261118-071210
- Session ID: session-20261118-071210
- Owner: Codex
- Focus: Dev-loop dashboard UI
- Summary: Replaced the fixed ten-line dashboard activity display with a live-following pane that retains up to 300 distinct status messages observed across refreshes.
- Status: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-11-18T07:12:14Z (2026-11-18 02:12:14 EST)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base/final commit: 43b89938b8aa29e3bffab982a544972f8dad7968 (uncommitted edits)

# Pre-existing changes

The checkout was already dirty when this work began, including `devscripts/dev-loop.py`, `docs/specs/dev-loop/dev-loop.md`, the game sources, and multiple configuration files. Those unrelated edits were preserved. The dev-loop spec already contained a separate user note requesting the older rainbow terminal UI; this session changed only the activity-pane requirements in that spec.

# Requested behavior

The user wanted the Python dashboard's build pane to show as much recent output as fits in its window, follow live progress, then close and reopen the GUI to confirm the change. The user also asked whether they could continue using the PC while Computer Use ran. I explained that simultaneous use is possible but mouse/keyboard actions can conflict; I limited interaction to launching and observing the dashboard.

# Specification alignment

- Specification: `docs/specs/dev-loop/dev-loop.md`, “Native dashboard and contributor activity feed — desired”.
- Requirement: show newest build or loop activity lines that fit in the pane, retain bounded scrollback, and follow the newest output during live refresh.
- Finding: the GUI previously sliced the display to ten rows even though its text widget already had a scrollbar and refreshed every second. The updated GUI follows the pane's full available model output and has a 300-message local history. It does not claim to capture each raw daemon line between refreshes.
- Related existing preference: the spec's “Human UI and build-status preference (2026-11-18)” records the request to restore the rainbow terminal UI as the primary interface; that separate redesign was not part of this change.

# Exact implementation changes

## `devscripts/dev_loop/gui.py`

- `run()` state, around line 192: added a `deque(maxlen=300)` and last-message marker.
- Activity pane title, line 549: changed “LATEST BUILD AND LOOP ACTIVITY · LAST 10 LINES” to “LIVE BUILD AND LOOP OUTPUT · FOLLOWING LATEST”.
- `refresh()`, lines 1314–1325: removed fixed ten-line/7+3 slicing; insert all available build and recent activity rows, retain distinct status messages observed on refresh, and keep the existing automatic scroll to the newest line.
- Existing one-second refresh remains unchanged.

## `docs/specs/dev-loop/dev-loop.md`

- Updated the compact-pane requirement to show all newest lines that fit, keep bounded scrollback, and follow the latest output.
- Updated the activity-feed requirement to retain distinct status messages observed across refreshes.

# Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `devscripts/dev_loop/gui.py` | Remove ten-row output cap; retain up to 300 observed status messages; label live-following pane. | Revert only this session's GUI diff; preserve any later edits. |
| `docs/specs/dev-loop/dev-loop.md` | Clarify desired visible feed and bounded scrollback behavior. | Revert only this session's activity-pane wording; preserve the pre-existing terminal-UI preference and other edits. |
| `docs/changelog/2026-11-18/20261118_071215-live-dashboard-build-output.md` | This session record. | Remove this file. |

# Focused review

- `docs/skills/spec-behavior-review-v1.md`: PASS. The owner is `devscripts/dev_loop/gui.py`; implementation now matches the revised pane requirement. The skill's acceptance distinction remains: visible dashboard evidence does not prove game behavior.
- `docs/skills/documentation-checker-v1.md`: PASS for the edited section. The existing spec has a purpose and DOES NOT block; the edits clarify the desired pane behavior without changing the separate rainbow-terminal preference.
- Computer Use skill: completed a read-only visual check after launch; dashboard window showed the new title and a filled output pane.

# Validation and evidence

- `git diff --check -- devscripts/dev_loop/gui.py docs/specs/dev-loop/dev-loop.md`: passed; Git emitted only its line-ending normalization warning for the pre-existing spec file.
- Launched the dashboard through `devscripts/open-dev-loop-dashboard.py`; when that did not surface a window in the initial inventory, used the permitted `python devscripts/dev-loop.py --ensure-running` path. The existing coordinator window appeared and was inspected.
- Visual observation: the dashboard showed “LIVE BUILD AND LOOP OUTPUT · FOLLOWING LATEST”, more than ten numbered output rows, build messages, and loop journal events. A later screenshot showed newer build generation 332 output than the first screenshot, demonstrating live dashboard refresh while the user could continue using the PC.
- `python devscripts/dev-loop.py --status` at 2026-11-18T07:13Z reported `lifecycle=game_building`, `build=published_current`, and an in-progress game build detail for generation 332 (`compiled=11`, `skipped=631`, `compile=75.1s`, `link=112.5s`, `total=187.6s`). The service status separately reported `game-build generation=329`; the detail string identified generation 332. This is observed dev-loop build evidence, not a build triggered by this session.
- No build request, compiler, or test suite was run. The game run was reported `NOT RUNNING`; no gameplay or NPC weapon behavior was verified. The dashboard itself loaded the edited Python GUI and visibly rendered the new activity pane.
- Human review: the window is left open for the user. The user can keep using the PC; avoid simultaneous mouse/keyboard actions on the dashboard while interacting with it.

# Regression review

No new gameplay regression was investigated or recorded. This was a dashboard presentation fix.

# Remaining review

The live dashboard display is verified. The user should confirm that the amount of scrollback and auto-follow behavior feels right on their display. The separate rainbow terminal UI preference remains documented for a future interface task.
# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-18T05:35:43.892563Z", "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "194b4e2fd24fb3152ebbf0e2875779f0335454230f015c09e3e471f6fd15f2af", "dll": "build\\game\\generation-331\\game-g331.dll", "generation": 331, "revision": "fc0cec6af883d6d2bcee83a8f82d3aaad7a1f1ecefdd1d0aa70a89d0104ea444", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-331\\game-g331.dll", "generation": 331, "published_utc": "2026-11-18T07:07:35.939Z", "revision": "fc0cec6af883d6d2bcee83a8f82d3aaad7a1f1ecefdd1d0aa70a89d0104ea444", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 54.844, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-18T05:35:41.192Z"}}
- Generation: 331
- Artifact: build\game\generation-331\game-g331.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_062057\server-01\events.jsonl
- Session ID: session-20261118-071210

# How to reverse

- Uncommitted source file: `git checkout -- <file>`
- Deleted file: `git restore --source=<base> -- <file>`
- Committed change: `git revert <commit>`
- Generated file: rerun its generator
- ABI/state/schema change: restore the old generation and state backup; do not
  assume a migration reverse is automatic.
- Reverse only files touched by this session. Preserve pre-existing changes.

# Related feature record

- Feature path:

## Captured git state

```text
M config/accounts/default.json
 M config/analytics.json
 M config/audio/music-settings.json
 M config/camconfig.json
 M config/gamemodes/funcounterstrike.json
 M config/healthbar.json
 M config/movement.json
 M config/movement/movement-source.json
 M config/video-settings.json
 M config/weaponcollisions.json
 M config/weapons/rocket_launcher/config.json
 m devscripts/blender-ai
 M devscripts/dev-launch-modes.json
 M devscripts/dev-loop.py
 M devscripts/dev-profiles/npc-navigation.json
 M devscripts/dev_loop/gui.py
 M docs/regressions/dev-loop/README.md
 M docs/regressions/gameplay/README.md
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
 M docs/specs/dev-loop/dev-loop.md
 M docs/specs/gamemodes/funcounterstrike.md
 M docs/specs/open-gameplay-issues.md
 M docs/specs/performance/performance.md
 M src/combat/explosion-fx.cpp
 M src/combat/explosion-fx.h
 M src/combat/weapon-rocket-launcher.cpp
 M src/effects/effect-part.cpp
 M src/effects/effect-part.h
 M src/gui/hud/player-nameplates.cpp
 M src/gui/hud/player-nameplates.h
 M src/network/multiplayer-context.h
 M src/network/multiplayer-projectiles.cpp
 M src/npc/npc.cpp
 M src/physics/movement/physics-collision-body.cpp
?? config/movement/movement-deliberate.json
?? docs/changelog/2026-11-17/20261117_211612-payload-startup-abi-request-version-fix.md
?? docs/changelog/2026-11-17/20261117_212624-deliberate-movement-metric-comments.md
?? docs/changelog/2026-11-17/20261117_213105-gui-server-join-startup-investigation.md
?? docs/changelog/2026-11-17/20261117_215248-juggernaut-gui-weapon-set.md
?? docs/changelog/2026-11-17/20261117_221804-juggernaut-gui-weapon-override.md
?? docs/changelog/2026-11-18/
?? docs/handoffs/collision-melee-fcs-trace-handoff.md
?? docs/regressions/dev-loop/juggernaut-gui-weapon-set-REG-2026-11-17.md
?? docs/regressions/dev-loop/unowned-process-reaper-REG-2026-11-17.md
?? docs/regressions/gameplay/duel-npc-weapon-cycle-REG-2026-11-18.md
```

## Captured diff stat

```text
config/accounts/default.json                       |  2 +-
 config/analytics.json                              |  4 +-
 config/audio/music-settings.json                   |  2 +-
 config/camconfig.json                              |  4 +-
 config/gamemodes/funcounterstrike.json             |  6 +-
 config/healthbar.json                              |  2 -
 config/movement.json                               |  1 +
 config/movement/movement-source.json               | 10 ++-
 config/video-settings.json                         |  2 +-
 config/weaponcollisions.json                       | 29 +++++----
 config/weapons/rocket_launcher/config.json         |  2 +-
 devscripts/blender-ai                              |  0
 devscripts/dev-launch-modes.json                   |  1 +
 devscripts/dev-loop.py                             | 22 +++++++
 devscripts/dev-profiles/npc-navigation.json        |  2 +-
 devscripts/dev_loop/gui.py                         | 17 ++++--
 docs/regressions/dev-loop/README.md                | 16 ++++-
 docs/regressions/gameplay/README.md                |  9 ++-
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 +++++----
 docs/specs/dev-loop/dev-loop.md                    | 40 ++++++++----
 docs/specs/gamemodes/funcounterstrike.md           |  3 +
 docs/specs/open-gameplay-issues.md                 | 14 ++++-
 docs/specs/performance/performance.md              | 33 ++++++++++
 src/combat/explosion-fx.cpp                        | 21 ++++---
 src/combat/explosion-fx.h                          |  6 +-
 src/combat/weapon-rocket-launcher.cpp              | 71 ++++++++++++++++++++--
 src/effects/effect-part.cpp                        |  4 +-
 src/effects/effect-part.h                          |  2 +-
 src/gui/hud/player-nameplates.cpp                  |  8 +++
 src/gui/hud/player-nameplates.h                    |  3 +-
 src/network/multiplayer-context.h                  |  4 ++
 src/network/multiplayer-projectiles.cpp            | 19 ++++++
 src/npc/npc.cpp                                    |  8 ++-
 src/physics/movement/physics-collision-body.cpp    |  9 ++-
 35 files changed, 327 insertions(+), 81 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/camconfig.json
M	config/gamemodes/funcounterstrike.json
M	config/healthbar.json
M	config/movement.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	config/weaponcollisions.json
M	config/weapons/rocket_launcher/config.json
M	devscripts/blender-ai
M	devscripts/dev-launch-modes.json
M	devscripts/dev-loop.py
M	devscripts/dev-profiles/npc-navigation.json
M	devscripts/dev_loop/gui.py
M	docs/regressions/dev-loop/README.md
M	docs/regressions/gameplay/README.md
M	docs/regressions/networking/README.md
M	docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
M	docs/specs/dev-loop/dev-loop.md
M	docs/specs/gamemodes/funcounterstrike.md
M	docs/specs/open-gameplay-issues.md
M	docs/specs/performance/performance.md
M	src/combat/explosion-fx.cpp
M	src/combat/explosion-fx.h
M	src/combat/weapon-rocket-launcher.cpp
M	src/effects/effect-part.cpp
M	src/effects/effect-part.h
M	src/gui/hud/player-nameplates.cpp
M	src/gui/hud/player-nameplates.h
M	src/network/multiplayer-context.h
M	src/network/multiplayer-projectiles.cpp
M	src/npc/npc.cpp
M	src/physics/movement/physics-collision-body.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-18T05:35:43.892563Z",
    "latest_successful_build_id": "2026-11-18T05:35:43.892563Z:4f59f6293ea73a9bfbb2eee800c348980030d495c99d6df656ed002a28922012",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "194b4e2fd24fb3152ebbf0e2875779f0335454230f015c09e3e471f6fd15f2af",
    "dll": "build\\game\\generation-331\\game-g331.dll",
    "generation": 331,
    "revision": "fc0cec6af883d6d2bcee83a8f82d3aaad7a1f1ecefdd1d0aa70a89d0104ea444",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-331\\game-g331.dll",
    "generation": 331,
    "published_utc": "2026-11-18T07:07:35.939Z",
    "revision": "fc0cec6af883d6d2bcee83a8f82d3aaad7a1f1ecefdd1d0aa70a89d0104ea444",
    "status": "ready"
  },
  "progress": {
    "compiled": 4,
    "elapsed_s": 54.844,
    "phase": "build_complete",
    "result": "success",
    "skipped": 83,
    "updated_utc": "2026-11-18T05:35:41.192Z"
  }
}
```
