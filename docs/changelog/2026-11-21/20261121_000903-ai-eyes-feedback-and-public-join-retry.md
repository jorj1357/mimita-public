// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# AI eyes feedback and public join retry

# Task

- Task ID: session-f75b3e33e623
- Session ID: session-f75b3e33e623
- Owner: root
- Focus: AI awareness and public server join evidence
- Summary: Added user's exact desktop inspection timeout and awareness request to issue-91e3384b219b, related it to public join issue issue-cf9cd77eb056, and sent the AI-awareness owner a concrete inspection fallback proposal. Retried desktop inspection; CUA timed out at service_info again, so no public JOIN click or public connection events were obtained. Existing visible dev-loop run 20261120_232200 is a separate sandbox run and was left untouched. No source code changed.
- Status: Issue updated; public retry blocked by repeated desktop inspection timeout
- Date, time, timezone: 2026-11-21T00:09:03+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

# Pre-existing changes

- Exact status output:
- Files not created or modified by this session:

# Requested behavior

# Specification alignment

- Current specification paths:
- Exact requirements:
- Why the change follows the specification:
- Conflicts or decisions:

# Exact implementation changes

## File: `path/to/file`

- Lines/functions/headings:
- Old content or behavior:
- New content or behavior:
- Reason:
- Why unrelated behavior is preserved:

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/behavior-profiles.json` |  |  |  | git checkout -- config/behavior-profiles.json |
| `config/movement/movement-heavy.json` |  |  |  | git checkout -- config/movement/movement-heavy.json |
| `config/movement/movement-source.json` |  |  |  | git checkout -- config/movement/movement-source.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `devscripts/quality/test_game_build_snapshot.py` |  |  |  | git checkout -- devscripts/quality/test_game_build_snapshot.py |
| `docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md` |  |  |  | git checkout -- docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md |
| `docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md` |  |  |  | git checkout -- docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md` |  |  |  | git checkout -- docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md |
| `docs/specs/dev-loop/agent-build-queue.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-build-queue.md |
| `docs/specs/dev-loop/build-time-observability.md` |  |  |  | git checkout -- docs/specs/dev-loop/build-time-observability.md |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `devscripts/quality/test_build_coordinator.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_build_coordinator.py' |
| `docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md' |
| `docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md' |
| `docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md' |
| `docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md' |
| `docs/changelog/2026-11-21/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/' |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md' |
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md' |
| `docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md' |
| `docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md' |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md' |

# Contract / ABI changes

- Contract:
- Version bump:
- Compatibility:
- Reverse:

# State / schema changes

- Domain:
- Old schema:
- New schema:
- Migration:
- Reverse:

# Deleted paths

- Path:
- Before:
- Why:
- Reverse:

# Diagnostics

- Owner/category:
- Input:
- Decision:
- Output:
- Failure or rejection reason:
- Rate limiting:

# Validation

- Focused skill paths and results:
- Tests and exact commands:
- Build status:
- Runtime or hot-reload evidence:
- Output files:

# Measured evidence

- Before values:
- After values:
- Timestamps:
- Tick/frame/network measurements:

# Regression review

- Regression entry appended: yes/no
- Why this is or is not a confirmed regression:
- Related regression paths:

# Human acceptance

- Visual review:
- Gameplay review:
- Multiplayer review:
- Still unverified:

# Agent handoff context

- Owner: root
- Focus: AI awareness and public server join evidence
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d651e1332fccff70bc5cc4601dc24d55784820b5 / d651e1332fccff70bc5cc4601dc24d55784820b5
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_current", "latest_build_stale": false, "latest_successful_build": "2026-11-20T20:20:51.788061Z", "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55", "latest_successful_source_generation": 38}, "game_current": {"build_mode": "release", "code_hash": "8b470e4249101ba96d6bc32a4a3b1133e09366bc6c476b4bf9d504f95394a17d", "dll": "build\\game\\generation-416\\game-g416.dll", "generation": 416, "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-416\\game-g416.dll", "generation": 416, "published_utc": "2026-11-20T21:27:56.236Z", "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 31.195, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-20T21:40:39.596Z"}}
- Generation: 416
- Artifact: build\game\generation-416\game-g416.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_232200\server-01\events.jsonl
- Session ID: session-f75b3e33e623

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
 M config/behavior-profiles.json
 M config/movement/movement-heavy.json
 M config/movement/movement-source.json
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/game_build_worker.py
 M devscripts/quality/test_game_build_snapshot.py
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
 M docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
 M docs/specs/dev-loop/agent-build-queue.md
 M docs/specs/dev-loop/build-time-observability.md
 M src/npc/npc.cpp
?? devscripts/quality/test_build_coordinator.py
?? docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md
?? docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md
?? docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md
?? docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md
?? docs/changelog/2026-11-21/
?? docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
?? docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
?? docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
?? docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md
?? docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/behavior-profiles.json                      |  23 ++---
 config/movement/movement-heavy.json                |   3 +-
 config/movement/movement-source.json               |   3 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             | 110 +++++++++++++++++++--
 devscripts/dev_loop/game_build.py                  |  21 ++--
 devscripts/game_build_worker.py                    |  93 ++++++++++++++++-
 devscripts/quality/test_game_build_snapshot.py     |  30 +++++-
 ...e-issues-preview-overlay-first-frame-context.md |  30 +++++-
 docs/issues/README.md                              |  19 ++--
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |  35 +++++++
 ...e-dll-build-at-game-build-issue-0322fa45b0e8.md |  18 ++++
 ...ative-editing-and-credite-issue-421c5be1669f.md |   6 ++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  10 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  12 +++
 ...ner-preview-overlays-and--issue-06c8cd76dc9d.md |  26 +++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  40 ++++++++
 .../counterstrike-npc-targeting-REG-2026-10-04.md  |  27 +++++
 docs/specs/dev-loop/agent-build-queue.md           |  11 +++
 docs/specs/dev-loop/build-time-observability.md    |  22 +++++
 src/npc/npc.cpp                                    |  11 ++-
 25 files changed, 503 insertions(+), 55 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/behavior-profiles.json
M	config/movement/movement-heavy.json
M	config/movement/movement-source.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/game_build_worker.py
M	devscripts/quality/test_game_build_snapshot.py
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
M	docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
M	docs/specs/dev-loop/agent-build-queue.md
M	docs/specs/dev-loop/build-time-observability.md
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_current",
    "latest_build_stale": false,
    "latest_successful_build": "2026-11-20T20:20:51.788061Z",
    "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55",
    "latest_successful_source_generation": 38
  },
  "game_current": {
    "build_mode": "release",
    "code_hash": "8b470e4249101ba96d6bc32a4a3b1133e09366bc6c476b4bf9d504f95394a17d",
    "dll": "build\\game\\generation-416\\game-g416.dll",
    "generation": 416,
    "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-416\\game-g416.dll",
    "generation": 416,
    "published_utc": "2026-11-20T21:27:56.236Z",
    "revision": "4d4c42863bdf0f0388a13bf2fa92944af3bee127d115e1ca043355125223b0fc",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 31.195,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-20T21:40:39.596Z"
  }
}
```
