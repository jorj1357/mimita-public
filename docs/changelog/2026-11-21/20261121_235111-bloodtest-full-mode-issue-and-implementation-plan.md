// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Bloodtest full-mode issue and implementation plan

# Task

- Task ID: session-20261121-235108
- Session ID: session-20261121-235108
- Owner: bloodtest-mode-expansion-plan
- Focus: Full Bloodtest sandbox: projectiles, separate actor/part health, gore, deliberate movement, traversal, weapons, grenades and future VR readiness
- Summary: Linked the expanded user direction to the existing Bloodtest issue, recorded a staged full-mode plan, and added nine dependent planned tasks.
- Status: PLANNED
- Date, time, timezone: 2026-11-21T23:51:10+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 935c0e7ed4a702055b553aea19ea6c51b6c31a94
- Final commit: 935c0e7ed4a702055b553aea19ea6c51b6c31a94

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
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md` |  |  |  | git checkout -- docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md |

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

- Owner: bloodtest-mode-expansion-plan
- Focus: Full Bloodtest sandbox: projectiles, separate actor/part health, gore, deliberate movement, traversal, weapons, grenades and future VR readiness
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 935c0e7ed4a702055b553aea19ea6c51b6c31a94 / 935c0e7ed4a702055b553aea19ea6c51b6c31a94
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-21T02:26:50.306117Z", "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "63386cc96d870e80700eb8a6e2e125ccdca3df12290f40cc0f17ff4646e3f293", "dll": "build\\game\\generation-436\\game-g436.dll", "generation": 436, "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-436\\game-g436.dll", "generation": 436, "published_utc": "2026-11-21T22:36:56.026Z", "revision": "631d0fcb48088898ead2da234cc6bb0820a654ab216d486622be1fd71f17ed45", "status": "ready"}, "progress": {"compiled": 17, "elapsed_s": 2727.517, "phase": "build_complete", "result": "success", "skipped": 70, "updated_utc": "2026-11-21T02:26:10.895Z"}}
- Generation: 436
- Artifact: build\game\generation-436\game-g436.dll
- Journal path: C:\mimita-v9\logs\2026-11-21\20261121_224058\server-01\events.jsonl
- Session ID: session-20261121-235108

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
m devscripts/blender-ai
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
```

## Captured diff stat

```text
devscripts/blender-ai                              |  0
 devscripts/dev_loop/game_build.py                  | 90 +++++++++++++++++++++-
 devscripts/dev_loop/gui.py                         | 32 ++++++++
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md | 45 +++++++++++
 4 files changed, 164 insertions(+), 3 deletions(-)
```

## Captured changed paths

```text
M	devscripts/blender-ai
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
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


# Session findings

- Exact user report was appended as a multiline quote under issue-2e7b0cd6f868; the new user context ctx-010793c65100 is linked as the primary context. The previous floor/gore/movement report remains unchanged.
- The issue now covers the intended Bloodtest sandbox: compact purposeful maps, existing MiMITA character/rig, nearby NPC spawn, physical projectile firearms only, physical gun/muzzle aiming, deterministic force/body-region damage, separate actor HP and limb health, body layers and gore, slow walk with sprint/stamina, forced first-person view, ragdoll/death fade with immediate respawn, climbing, expanded firearm/melee/grenade experiments, and future VR readiness with Retrograd credit.
- The implementation plan is appended to the issue in nine ordered phases: contract/baseline; actor-life and floor reproduction; compact playable slice; projectile aiming/combat; gore/ragdoll/fade; traversal/collision profiling; weapon/melee/grenade expansion; VR readiness; full live acceptance.
- Nine planned and unassigned tasks were added in the same dependency order: bloodtest-contract -> bloodtest-body-life -> bloodtest-vertical-slice -> bloodtest-projectile-combat -> bloodtest-gore-ragdoll-fade -> bloodtest-traversal-collision -> bloodtest-weapon-expansion -> bloodtest-vr-readiness -> bloodtest-live-acceptance. Issue readiness reports 9 tasks and no missing fields.
- Observed related regression: docs/regressions/gameplay/healthall-body-part-lethal-REG-2026-11-07.md documents a confirmed global-health/body-part-health mismatch that can kill actors at nonzero displayed health. It is a useful separate reproduction target, not proof that it is the same as the newly reported disappearing-limb case.
- Open decisions are retained in the issue plan: numeric movement/stamina and damage thresholds, shotgun lethality distance, climbing/step limits, fade duration, map/layout, organ/gore assets, performance budget, and when detailed limb/weapon collision is necessary. The plan starts with arcade collision and measured performance.
- VR is recorded as future readiness and a later hardware/runtime-dependent phase. This work does not claim current VR support.
- No source code or spec was changed; no build, run, gameplay, visual, or human acceptance was performed. The user asked for issue expansion and planning. Runtime reproduction and implementation remain open.
- Focused documents: docs/ROUTER.md; docs/specs/gamemodes/bloodtest.md; docs/specs/movement/movement.md; docs/specs/dev-loop/agent-coordination.md; docs/workflows/issue-intake-and-triage.md; docs/skills/spec-behavior-review-v1.md; docs/regressions/gameplay/healthall-body-part-lethal-REG-2026-11-07.md; docs/operations/task-completion/task-completion.md.
- Pre-existing working-tree changes were preserved. Issue records were appended using the coordination issue writer; no unrelated files were touched manually.

