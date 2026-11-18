# Dashboard refresh heartbeat

# Task

- Task ID: session-20261118-072340
- Session ID: session-20261118-072340
- Owner: Codex
- Focus: Dev-loop dashboard UI
- Summary: Display an ISO 8601 UTC timestamp and per-window GUI refresh counter, and identify the live daemon status and events.jsonl sources shown by the build/activity pane.
- Status: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-11-18T07:23:41Z (2026-11-18 02:23:41 EST)
- Branch: checkpoint/one-checkout-condensation-20261111
- Base/final commit: 43b89938b8aa29e3bffab982a544972f8dad7968 (uncommitted changes)

# Pre-existing changes

The checkout already had unrelated edits from earlier work. `devscripts/dev_loop/gui.py` and `docs/specs/dev-loop/dev-loop.md` were modified in the preceding dashboard task. The GUI live-output deque and removal of the ten-line display cap are prior work in that same file; this session only added the refresh heartbeat and source labels there. The previously edited dev-loop spec was preserved unchanged in this session.

# Requested behavior

Show a current ISO 8601 timestamp that visibly advances, a number that increments so the user can see the dashboard polling, and a clear indication of where build and loop activity data comes from.

# Specification alignment

- Specification: `docs/specs/dev-loop/dashboard.md`, “Refresh and running-process list” and “Acceptance”.
- Required behavior: one shared freshness indicator with ISO 8601 UTC last-read time and a monotonic per-window refresh count; identify the main dev-loop status response and the `events.jsonl` path.
- Source trace: `devscripts/dev-loop.py`'s `snapshot()` calls `request_daemon_status`; `gui_status()` maps `build_output_tail`, `message`, `updated_at`, and `events_path` to the GUI model; `_recent_activity(events_path)` reads loop events from that journal.

# Exact implementation changes

## `devscripts/dev_loop/gui.py`

- `run()` state, lines 205–206: added `dashboard_refresh_utc` and `dashboard_refresh_count`.
- Activity pane setup, lines 553–556: added a green shared refresh indicator and source summary.
- `refresh()`, lines 1107–1111: after each successful status-provider call, increment the per-window count and record `_utc_now_iso()`.
- `refresh()`, lines 1317–1337: display the shared freshness age, exact UTC time, and padded `GUI refresh #000000`; retain the process count separately; label build data as the main dev-loop status RPC (`build_output_tail/message`); show the current run journal path relative to the checkout and the daemon status timestamp.

## `docs/specs/dev-loop/dashboard.md`

- Refresh requirements, lines 91–101: specify the UTC ISO timestamp, monotonically increasing successful-read count, and data source labels.
- Acceptance, lines 148–152: require the timestamp and count to advance while idle.

# Changed files and reverse steps

| File | Change in this session | Reverse |
|---|---|---|
| `devscripts/dev_loop/gui.py` | Add UTC refresh timestamp/counter and label status/journal inputs. | Revert only this session's hunks; preserve the previous live-feed edits. |
| `docs/specs/dev-loop/dashboard.md` | Specify the freshness counter and source display. | Revert only this session's hunks. |
| `docs/changelog/2026-11-18/20261118_072342-dashboard-refresh-heartbeat.md` | This session record. | Remove this file. |

# Focused review

- `docs/skills/spec-behavior-review-v1.md`: PASS. The refresh display is owned by the dashboard's `run()` and `refresh()`; it represents successful GUI status reads, not build-line counts.
- `docs/skills/documentation-checker-v1.md`: PASS for the changed section. Source labels are aligned with the actual RPC model and journal reader.
- `docs/architecture/time-and-formatting/time-and-formatting.md`: PASS. Timestamp uses UTC ISO 8601 with milliseconds and `Z`.

# Validation and evidence

- `git diff --check -- devscripts/dev_loop/gui.py docs/specs/dev-loop/dashboard.md`: passed.
- Closed only the dashboard window, used the allowed `python devscripts/dev-loop.py --ensure-running` status path, and launched the dashboard with `python devscripts/open-dev-loop-dashboard.py`. The root daemon remains the build/run authority; no build request was issued.
- Visual check showed `Updated 0s ago · 2026-11-18T07:22:58.323Z · GUI refresh #000022`, then `Updated 0s ago · 2026-11-18T07:23:09.037Z · GUI refresh #000029` in a later screenshot. This confirms both timestamp and counter advance live.
- The pane displayed `Build input: main dev-loop status RPC (build_output_tail/message)` and `Loop input: logs/2026-11-18/20261118_062057/dev-loop/events.jsonl`, followed by the daemon status timestamp.
- No tests or builds were run. No gameplay behavior was evaluated.

# Human acceptance

The updated dashboard is open. The user can continue using the PC; avoid simultaneous mouse/keyboard actions on the dashboard while they interact with it.
# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 43b89938b8aa29e3bffab982a544972f8dad7968 / 43b89938b8aa29e3bffab982a544972f8dad7968
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-17T21:12:47.128057Z", "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac", "latest_successful_source_generation": 98}, "game_current": {"build_mode": "debug", "code_hash": "cb92e5d18ac890d7bced31d98643fa59be0865cf4e5c19971ad0d5bac53bbadf", "dll": "build\\game\\generation-333\\game-g333.dll", "generation": 333, "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-333\\game-g333.dll", "generation": 333, "published_utc": "2026-11-18T07:22:17.418Z", "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2", "status": "ready"}, "progress": {"compiled": 4, "elapsed_s": 54.844, "phase": "build_complete", "result": "success", "skipped": 83, "updated_utc": "2026-11-18T05:35:41.192Z"}}
- Generation: 333
- Artifact: build\game\generation-333\game-g333.dll
- Journal path: C:\mimita-v9\logs\2026-11-18\20261118_062057\dev-loop\events.jsonl
- Session ID: session-20261118-072340

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
 M docs/regressions/collisions/README.md
 M docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md
 M docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md
 M docs/regressions/dev-loop/README.md
 M docs/regressions/gameplay/README.md
 M docs/regressions/networking/README.md
 M docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
 M docs/specs/dev-loop/dashboard.md
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
 config/weaponcollisions.json                       |  2 +-
 config/weapons/rocket_launcher/config.json         |  2 +-
 devscripts/blender-ai                              |  0
 devscripts/dev-launch-modes.json                   |  1 +
 devscripts/dev-loop.py                             | 22 +++++++
 devscripts/dev-profiles/npc-navigation.json        |  2 +-
 devscripts/dev_loop/gui.py                         | 44 ++++++++++++--
 docs/regressions/collisions/README.md              |  4 +-
 .../physical-objects-collision-REG-2026-10-01.md   | 69 +++++++++++++++++++++
 .../collisions/slope-edge-snag-REG-2026-09-29.md   |  9 +++
 docs/regressions/dev-loop/README.md                | 16 ++++-
 docs/regressions/gameplay/README.md                |  9 ++-
 docs/regressions/networking/README.md              |  2 +-
 .../server-policy-startup-abi-REG-2026-11-17.md    | 30 +++++----
 docs/specs/dev-loop/dashboard.md                   | 16 +++--
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
 38 files changed, 422 insertions(+), 75 deletions(-)
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
M	docs/regressions/collisions/README.md
M	docs/regressions/collisions/physical-objects-collision-REG-2026-10-01.md
M	docs/regressions/collisions/slope-edge-snag-REG-2026-09-29.md
M	docs/regressions/dev-loop/README.md
M	docs/regressions/gameplay/README.md
M	docs/regressions/networking/README.md
M	docs/regressions/networking/server-policy-startup-abi-REG-2026-11-17.md
M	docs/specs/dev-loop/dashboard.md
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
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-17T21:12:47.128057Z",
    "latest_successful_build_id": "2026-11-17T21:12:47.128057Z:c0f4b20b6d5fb4b5c2ca2f0b296d2a7da2184296ad5fb3c7f7e13663536edfac",
    "latest_successful_source_generation": 98
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "cb92e5d18ac890d7bced31d98643fa59be0865cf4e5c19971ad0d5bac53bbadf",
    "dll": "build\\game\\generation-333\\game-g333.dll",
    "generation": 333,
    "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-333\\game-g333.dll",
    "generation": 333,
    "published_utc": "2026-11-18T07:22:17.418Z",
    "revision": "8b0d5fba1c126ddd058c902bb1d3962497062bdfc0499b77ef35d73e2957c8f2",
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
