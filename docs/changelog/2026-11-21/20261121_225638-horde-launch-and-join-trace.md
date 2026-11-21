// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Horde launch and join trace

# Task

- Task ID: session-34361b7ba372
- Session ID: session-34361b7ba372
- Owner: reconnect-horde-consistency
- Focus: dev-loop GUI launch mode, server/client reuse, Juggernaut Horde roster and NPC targeting
- Summary: Recorded the new Horde targeting report, corrected misattributed runtime evidence, and traced GUI launch-mode/server reuse branches. No source, build, or runtime changes; pinned reproduction is handed to the active end-to-end owner.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-21T22:56:37+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d89884f64374810efa624eac8ee6cc5af7e19dae
- Final commit: d89884f64374810efa624eac8ee6cc5af7e19dae

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
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` |  |  |  | git checkout -- docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-2235461bb31b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-2235461bb31b.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c3ecfab72cba.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c3ecfab72cba.md |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c632de44f8d6.md` |  |  |  | git checkout -- docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c632de44f8d6.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/risk/20261120-risk-premature-hot-layer-migration-without-live--issue-23148d00bf5b.md` |  |  |  | git checkout -- docs/issues/risk/20261120-risk-premature-hot-layer-migration-without-live--issue-23148d00bf5b.md |
| `docs/specs/procedural-infinite-world/procedural-infinite-world.md` |  |  |  | git checkout -- docs/specs/procedural-infinite-world/procedural-infinite-world.md |
| `src/effects/zombie-tower-effects.cpp` |  |  |  | git checkout -- src/effects/zombie-tower-effects.cpp |
| `src/gamemode/map-config.cpp` |  |  |  | git checkout -- src/gamemode/map-config.cpp |
| `src/mapeditor/map-editor-commands.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor-commands.cpp |
| `src/mapeditor/map-editor.cpp` |  |  |  | git checkout -- src/mapeditor/map-editor.cpp |
| `src/mapeditor/map-editor.h` |  |  |  | git checkout -- src/mapeditor/map-editor.h |
| `docs/changelog/2026-11-21/20261121_215833-infinite-xxity-scale-feature-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_215833-infinite-xxity-scale-feature-intake.md' |
| `docs/changelog/2026-11-21/20261121_220424-issue-136d4553ff92-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_220424-issue-136d4553ff92-independent-review.md' |
| `docs/changelog/2026-11-21/20261121_220437-one-source-compile-link-trial.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_220437-one-source-compile-link-trial.md' |
| `docs/changelog/2026-11-21/20261121_222546-infinite-xxity-generation-zone-map-editor-controls.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_222546-infinite-xxity-generation-zone-map-editor-controls.md' |
| `docs/changelog/2026-11-21/20261121_222900-live-build-timer-and-resource-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_222900-live-build-timer-and-resource-audit.md' |
| `docs/changelog/2026-11-21/20261121_223000-issue-6907072bb155-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_223000-issue-6907072bb155-independent-review.md' |
| `docs/changelog/2026-11-21/20261121_225553-issue-23148d00bf5b-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_225553-issue-23148d00bf5b-independent-review.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-1b08300935ae.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-1b08300935ae.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-44d82d8626ad.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-44d82d8626ad.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-585ca55b5bb8.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-585ca55b5bb8.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-a1eebc340521.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-a1eebc340521.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-cc0a03f471ad.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-cc0a03f471ad.md' |
| `docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-4651ec01f8a5.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-4651ec01f8a5.md' |
| `docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-d69d22e34259.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-d69d22e34259.md' |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md' |
| `docs/reports/20261121_220437-one-source-compile-link-trial.md` |  |  |  | Remove-Item -LiteralPath 'docs/reports/20261121_220437-one-source-compile-link-trial.md' |
| `docs/reports/20261121_222900-live-build-timer-and-resource-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/reports/20261121_222900-live-build-timer-and-resource-audit.md' |

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

- Owner: reconnect-horde-consistency
- Focus: dev-loop GUI launch mode, server/client reuse, Juggernaut Horde roster and NPC targeting
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d89884f64374810efa624eac8ee6cc5af7e19dae / d89884f64374810efa624eac8ee6cc5af7e19dae
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-21T02:26:50.306117Z", "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "63386cc96d870e80700eb8a6e2e125ccdca3df12290f40cc0f17ff4646e3f293", "dll": "build\\game\\generation-436\\game-g436.dll", "generation": 436, "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-436\\game-g436.dll", "generation": 436, "published_utc": "2026-11-21T22:36:56.026Z", "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45", "status": "ready"}, "progress": {"compiled": 17, "elapsed_s": 2727.517, "phase": "build_complete", "result": "success", "skipped": 70, "updated_utc": "2026-11-21T02:26:10.895Z"}}
- Generation: 436
- Artifact: build\game\generation-436\game-g436.dll
- Journal path: C:\mimita-v9\logs\2026-11-21\20261121_224058\client-01\events.jsonl
- Session ID: session-34361b7ba372

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
 M config/video-settings.json
 m devscripts/blender-ai
 M devscripts/dev_loop/gui.py
 M docs/issues/README.md
 M docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-2235461bb31b.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c3ecfab72cba.md
 M docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c632de44f8d6.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/risk/20261120-risk-premature-hot-layer-migration-without-live--issue-23148d00bf5b.md
 M docs/specs/procedural-infinite-world/procedural-infinite-world.md
 M src/effects/zombie-tower-effects.cpp
 M src/gamemode/map-config.cpp
 M src/mapeditor/map-editor-commands.cpp
 M src/mapeditor/map-editor.cpp
 M src/mapeditor/map-editor.h
?? docs/changelog/2026-11-21/20261121_215833-infinite-xxity-scale-feature-intake.md
?? docs/changelog/2026-11-21/20261121_220424-issue-136d4553ff92-independent-review.md
?? docs/changelog/2026-11-21/20261121_220437-one-source-compile-link-trial.md
?? docs/changelog/2026-11-21/20261121_222546-infinite-xxity-generation-zone-map-editor-controls.md
?? docs/changelog/2026-11-21/20261121_222900-live-build-timer-and-resource-audit.md
?? docs/changelog/2026-11-21/20261121_223000-issue-6907072bb155-independent-review.md
?? docs/changelog/2026-11-21/20261121_225553-issue-23148d00bf5b-independent-review.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-1b08300935ae.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-44d82d8626ad.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-585ca55b5bb8.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-a1eebc340521.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-cc0a03f471ad.md
?? docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-4651ec01f8a5.md
?? docs/issues/capability/20261121-make-infinite-xxity-a-larger-fogged-meter-scaled-issue-d69d22e34259.md
?? docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
?? docs/reports/20261121_220437-one-source-compile-link-trial.md
?? docs/reports/20261121_222900-live-build-timer-and-resource-audit.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev_loop/gui.py                         |  84 ++++++-
 docs/issues/README.md                              |  27 +-
 ...rs-fail-to-consistently-e-issue-b95574b63f29.md |  14 ++
 ...e-dll-build-at-game-build-issue-2235461bb31b.md |  31 +++
 ...e-dll-build-at-game-build-issue-c3ecfab72cba.md |  30 +++
 ...e-dll-build-at-game-build-issue-c632de44f8d6.md |  36 +++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  12 +
 ...nd-observe-context-bundle-issue-6907072bb155.md |  53 ++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  46 ++++
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 122 +++++++++
 ...r-migration-without-live--issue-23148d00bf5b.md |  41 +++
 .../procedural-infinite-world.md                   |   9 +
 src/effects/zombie-tower-effects.cpp               |   1 +
 src/gamemode/map-config.cpp                        |   7 +-
 src/mapeditor/map-editor-commands.cpp              |  59 +++--
 src/mapeditor/map-editor.cpp                       | 280 ++++++++++++++++++++-
 src/mapeditor/map-editor.h                         |   8 +
 22 files changed, 820 insertions(+), 52 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/video-settings.json
M	devscripts/blender-ai
M	devscripts/dev_loop/gui.py
M	docs/issues/README.md
M	docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-2235461bb31b.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c3ecfab72cba.md
M	docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-c632de44f8d6.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/risk/20261120-risk-premature-hot-layer-migration-without-live--issue-23148d00bf5b.md
M	docs/specs/procedural-infinite-world/procedural-infinite-world.md
M	src/effects/zombie-tower-effects.cpp
M	src/gamemode/map-config.cpp
M	src/mapeditor/map-editor-commands.cpp
M	src/mapeditor/map-editor.cpp
M	src/mapeditor/map-editor.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-21T02:26:50.306117Z",
    "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "63386cc96d870e80700eb8a6e2e125ccdca3df12290f40cc0f17ff4646e3f293",
    "dll": "build\\game\\generation-436\\game-g436.dll",
    "generation": 436,
    "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-436\\game-g436.dll",
    "generation": 436,
    "published_utc": "2026-11-21T22:36:56.026Z",
    "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45",
    "status": "ready"
  },
  "progress": {
    "compiled": 17,
    "elapsed_s": 2727.517,
    "phase": "build_complete",
    "result": "success",
    "skipped": 70,
    "updated_utc": "2026-11-21T02:26:10.895Z"
  }
}
```

## Session findings and completion state

### Scope and findings

- The user’s full report was preserved verbatim as context `ctx-7b69ff7b24ef` on `issue-cf9cd77eb056` (the existing end-to-end networking/reliability issue). The NPC-targeting clause was appended to existing high-priority `issue-b95574b63f29`; this avoids duplicating already tracked issues.
- **Observed evidence correction:** `logs/2026-11-20/20261120_232200/run.json` identifies the run as `fun counter strike` on `funworld3`, build `2026-11-20T20:20:51.788061Z:64cd930...`. It is not a Horde run, despite the Horde targeting issue’s previous evidence summary. The correction is appended to issue `issue-b95574b63f29`.
- **Observed source path, not yet matched to the user’s click sequence:** `devscripts/dev_loop/gui.py` mode selection sends `set-launch-mode` (around line 2200). `devscripts/dev-loop.py` updates the selected daemon config (around line 3060); `start-server` rejects an already-running server (around line 3097); `start_client_instance` attaches to the existing server room/port without comparing the new selected mode (around line 1610). `launch_latest` checks the existing server’s launch configuration (around line 4088 and line 4316). This makes “switch mode, then Start client” a plausible old-server reuse path. It remains a hypothesis until a correlated GUI run confirms it.
- Launch mode `44` is configured as `juggernaut horde`, with `--mode jugger_horde --gamemode jugger_horde` and map `dust2cyberiav4`.
- **Observed source/config, not runtime proof:** `config/gamemodes/jugger_horde.json` declares `team_roster`, 64 Fighter slots, and 4 Juggernaut slots. `src/network/server-gamemode.cpp::reconcileTeamRosterNpc` fills declared capacities and emits `gamemode.team-roster-npc.created` and `gamemode.team-roster.resolved`; this reconciliation occurs after a player becomes active. Therefore, one NPC and generic team colors would be consistent with a wrong or not-yet-active Horde server, but the user’s actual run remains unknown.
- The live shared runtime at inspection was Payload run `20261121_224058`, server PID 27816, client PID 24192 marked stale; both are preserved. It provides no Horde evidence.
- A second active agent was already assigned the same end-to-end investigation. Findings and the proposed mode-switch/reuse check were sent to that agent; this session made no source/config changes and did not start a build or runtime.

### Specification and review

Read and applied: `docs/ROUTER.md`; `docs/specs/dev-loop/agent-lanes-build-authority.md`; `docs/specs/dev-loop/change-to-evidence-latency.md`; `docs/specs/dev-loop/agent-coordination.md`; `docs/specs/dev-loop/agent-proof-and-verification.md`; `docs/specs/networking/networking.md`; `docs/specs/gamemodes/gamemodes.md`; `docs/specs/gamemodes/juggernaut-horde.md`; `docs/specs/npc-behavior/npc-behavior.md`; `docs/specs/dev-loop/dashboard.md`; `docs/specs/dev-loop/dev-loop.md`; `docs/workflows/runtime-scenario-validation.md`; `docs/workflows/issue-intake-and-triage.md`; `docs/skills/spec-behavior-review-v1.md`; `docs/regressions/README.md`; and `docs/operations/task-completion/task-completion.md`.

The Horde specification requires the human to be a Fighter, opposite-team NPC targeting, and same-run server/client evidence from assignment through target, fire, damage, and receipt. It also requires stage-by-stage join/reconnect evidence. No live acceptance was claimed.

### Changed records and prior content

- `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md`: appended the user’s targeting clause and the manifest-based correction above. Existing issue text and claims were retained; earlier evidence was not rewritten.
- `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md`: appended the exact full user report, reproduction/acceptance context, the mode-switch/server-reuse source finding, and a handoff message. Prior content was retained.

### Validation and remaining work

- Coordination: registered, synchronized, recorded user context, appended findings to both existing issues, and sent the active end-to-end owner the mode-switch/server-reuse hypothesis and correct Horde launch mode.
- Source: read-only inspection of the GUI control path, server launch argument builder, Horde configuration, roster reconciliation, and event owners. No implementation change was made.
- Build: none requested. The EXE build recorded by the dev loop is stale for the current source; the currently ready game DLL is generation 436. Neither proves current EXE behavior.
- Runtime: no Horde run was started. The shared Payload session was preserved. HUD/team labels, roster count, reconnect transitions, targeting, and damage remain unverified in a current pinned Horde run.
- Next action: in one pinned run through the GUI, select mode 44, capture the accepted mode-selection request, launch or add a client according to the actual user sequence, and correlate effective server mode, `gamemode.match.configured`, roster counts/actor roles, join-ready/reconnect transitions, target-sense, attack gate, shots, authoritative damage, and client receipt. If Start client joins a prior-mode server, add a clear mode mismatch guard or replace path at the demonstrated owner, then repeat the same scenario.
- Human visual acceptance is still required for displayed team names and NPC behavior. Issue `issue-b95574b63f29` remains open and requires valid Horde runtime proof.
