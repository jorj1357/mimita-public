// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Agent brain check issue suggestions

# Task

- Task ID: session-8a8cba897285
- Session ID: session-8a8cba897285
- Owner: brain-check-hook
- Focus: agent coordination brain checks, issue-linked suggestions, and user context issue
- Summary: Added automatic brain-check context to agent sync and outbound coordination messages, issue-linked discussion appending, and an issue suggestion command. Updated agent guidance and recorded evidence on issue-a271b0296faa.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-20T05:50:45+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: ce0009d5b2983f462d1d17dd640c4e66769c2a23
- Final commit: ce0009d5b2983f462d1d17dd640c4e66769c2a23

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
| `AGENTS.md` |  |  |  | git checkout -- AGENTS.md |
| `config/accounts/default.json` |  |  |  | git checkout -- config/accounts/default.json |
| `config/analytics.json` |  |  |  | git checkout -- config/analytics.json |
| `config/gamemodes/masterlist/gamemode-masterlist.json` |  |  |  | git checkout -- config/gamemodes/masterlist/gamemode-masterlist.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `devscripts/agent-wake.py` |  |  |  | git checkout -- devscripts/agent-wake.py |
| `devscripts/agent_coord.py` |  |  |  | git checkout -- devscripts/agent_coord.py |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dashboard-keepalive.py` |  |  |  | git checkout -- devscripts/dashboard-keepalive.py |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/cli.py` |  |  |  | git checkout -- devscripts/dev_loop/cli.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md` |  |  |  | git checkout -- docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md |
| `docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md` |  |  |  | git checkout -- docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md` |  |  |  | git checkout -- docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md |
| `docs/regressions/dev-loop/README.md` |  |  |  | git checkout -- docs/regressions/dev-loop/README.md |
| `docs/specs/dev-loop/agent-coordination.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-coordination.md |
| `docs/specs/dev-loop/agent-self-improvement.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-self-improvement.md |
| `docs/specs/dev-loop/agent-wake.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-wake.md |
| `docs/specs/dev-loop/dashboard.md` |  |  |  | git checkout -- docs/specs/dev-loop/dashboard.md |
| `docs/specs/dev-loop/scenario-input-recording.md` |  |  |  | git checkout -- docs/specs/dev-loop/scenario-input-recording.md |
| `docs/specs/networking/contact-authority.md` |  |  |  | git checkout -- docs/specs/networking/contact-authority.md |
| `docs/specs/performance/performance.md` |  |  |  | git checkout -- docs/specs/performance/performance.md |
| `src/config/collision-config.h` |  |  |  | git checkout -- src/config/collision-config.h |
| `src/engine/engine-tick-render.cpp` |  |  |  | git checkout -- src/engine/engine-tick-render.cpp |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `devscripts/dev_loop/launch_report.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/dev_loop/launch_report.py' |
| `devscripts/quality/test_launch_report.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_launch_report.py' |
| `devscripts/tech-debt-audit.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/tech-debt-audit.py' |
| `docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md' |
| `docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md' |
| `docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md' |
| `docs/changelog/2026-11-20/20261120_045549-atomic-patch-and-issue-loop-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_045549-atomic-patch-and-issue-loop-audit.md' |
| `docs/changelog/2026-11-20/20261120_050000-culling-specs-ai-controller-issues.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_050000-culling-specs-ai-controller-issues.md' |
| `docs/changelog/2026-11-20/20261120_050111-user-context-notification-issue.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_050111-user-context-notification-issue.md' |
| `docs/changelog/2026-11-20/20261120_051943-prove-dashboard-monitor-position-survives-restart.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_051943-prove-dashboard-monitor-position-survives-restart.md' |
| `docs/gold/2026-11-20-juggernaut-60hz-server-and-client-culling.md` |  |  |  | Remove-Item -LiteralPath 'docs/gold/2026-11-20-juggernaut-60hz-server-and-client-culling.md' |
| `docs/issues/architecture/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/architecture/' |
| `docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md' |
| `docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md' |
| `docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md' |
| `docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md' |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md' |
| `docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md' |
| `docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md' |
| `docs/issues/improvement/20261120-culling-lod-and-work-avoidance-backlog-issue-5f49047dae49.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-culling-lod-and-work-avoidance-backlog-issue-5f49047dae49.md' |
| `docs/issues/improvement/20261120-make-arcade-collision-the-default-for-all-gamemo-issue-56f7a692b646.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-make-arcade-collision-the-default-for-all-gamemo-issue-56f7a692b646.md' |
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md' |
| `docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md' |
| `docs/issues/tech-debt/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/tech-debt/' |
| `docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md' |
| `docs/skills/performance-culling-v1.md` |  |  |  | Remove-Item -LiteralPath 'docs/skills/performance-culling-v1.md' |
| `docs/specs/architecture/` |  |  |  | Remove-Item -LiteralPath 'docs/specs/architecture/' |
| `docs/specs/dev-loop/launch-feedback.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/launch-feedback.md' |
| `docs/specs/dev-loop/visual-proof-screenshots.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/visual-proof-screenshots.md' |
| `docs/specs/performance/culling-and-work-avoidance.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/performance/culling-and-work-avoidance.md' |

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

- Owner: brain-check-hook
- Focus: agent coordination brain checks, issue-linked suggestions, and user context issue
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / ce0009d5b2983f462d1d17dd640c4e66769c2a23 / ce0009d5b2983f462d1d17dd640c4e66769c2a23
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-20T05:38:11.772319Z", "latest_successful_build_id": "2026-11-20T05:38:11.772319Z:06e0eafc8e0291960e9d19162dd5e6355d49d88d702b6febf7a6e6890d5a545b", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "b1e8824cc320422d440880ae80f1e1bf7c91b76b8cf20983ddd3a7f7126062be", "dll": "build\\game\\generation-394\\game-g394.dll", "generation": 394, "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-394\\game-g394.dll", "generation": 394, "published_utc": "2026-11-20T04:05:35.363Z", "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0", "status": "ready"}, "progress": {"compiled": 3, "elapsed_s": 821.572, "phase": "build_complete", "result": "success", "skipped": 84, "updated_utc": "2026-11-20T05:38:00.656Z"}}
- Generation: 394
- Artifact: build\game\generation-394\game-g394.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_040807\server-01\events.jsonl
- Session ID: session-8a8cba897285

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
M AGENTS.md
 M config/accounts/default.json
 M config/analytics.json
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/hot-reload/packet-ledger.json
 M devscripts/agent-wake.py
 M devscripts/agent_coord.py
 m devscripts/blender-ai
 M devscripts/dashboard-keepalive.py
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M docs/ROUTER.md
 M docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
 M docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
 M docs/regressions/dev-loop/README.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/agent-wake.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/dev-loop/scenario-input-recording.md
 M docs/specs/networking/contact-authority.md
 M docs/specs/performance/performance.md
 M src/config/collision-config.h
 M src/engine/engine-tick-render.cpp
 M src/gamemode/gamemode.h
?? devscripts/dev_loop/launch_report.py
?? devscripts/quality/test_launch_report.py
?? devscripts/tech-debt-audit.py
?? docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md
?? docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md
?? docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md
?? docs/changelog/2026-11-20/20261120_045549-atomic-patch-and-issue-loop-audit.md
?? docs/changelog/2026-11-20/20261120_050000-culling-specs-ai-controller-issues.md
?? docs/changelog/2026-11-20/20261120_050111-user-context-notification-issue.md
?? docs/changelog/2026-11-20/20261120_051943-prove-dashboard-monitor-position-survives-restart.md
?? docs/gold/2026-11-20-juggernaut-60hz-server-and-client-culling.md
?? docs/issues/architecture/
?? docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md
?? docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md
?? docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
?? docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
?? docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
?? docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
?? docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
?? docs/issues/improvement/20261120-culling-lod-and-work-avoidance-backlog-issue-5f49047dae49.md
?? docs/issues/improvement/20261120-make-arcade-collision-the-default-for-all-gamemo-issue-56f7a692b646.md
?? docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
?? docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md
?? docs/issues/tech-debt/
?? docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md
?? docs/skills/performance-culling-v1.md
?? docs/specs/architecture/
?? docs/specs/dev-loop/launch-feedback.md
?? docs/specs/dev-loop/visual-proof-screenshots.md
?? docs/specs/performance/culling-and-work-avoidance.md
```

## Captured diff stat

```text
AGENTS.md                                          |  19 +++
 config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 .../gamemodes/masterlist/gamemode-masterlist.json  |   4 +-
 config/hot-reload/packet-ledger.json               |  12 +-
 devscripts/agent-wake.py                           |  16 ++-
 devscripts/agent_coord.py                          | 123 +++++++++++++++++--
 devscripts/blender-ai                              |   0
 devscripts/dashboard-keepalive.py                  |   6 +-
 devscripts/dev-loop.py                             |  98 +++++++++++----
 devscripts/dev_loop/cli.py                         |   6 +
 devscripts/dev_loop/game_build.py                  |  89 +++++++++++++-
 devscripts/dev_loop/gui.py                         | 134 +++++++++++++++++----
 docs/ROUTER.md                                     |   6 +-
 ...dination-brain-and-build-fan-out-attribution.md |  17 ++-
 .../20261120_040000-live-dashboard-build-health.md |  19 ++-
 docs/issues/README.md                              |  14 +++
 ...tency-limbs-only-capsule--issue-da337d1fe2f3.md |  42 +++++++
 docs/regressions/dev-loop/README.md                |   9 +-
 docs/specs/dev-loop/agent-coordination.md          |  33 +++++
 docs/specs/dev-loop/agent-self-improvement.md      |  65 ++++++++++
 docs/specs/dev-loop/agent-wake.md                  |  24 ++++
 docs/specs/dev-loop/dashboard.md                   |  23 +++-
 docs/specs/dev-loop/scenario-input-recording.md    |  12 ++
 docs/specs/networking/contact-authority.md         |  86 +++++++++++++
 docs/specs/performance/performance.md              |  39 ++++--
 src/config/collision-config.h                      |   6 +-
 src/engine/engine-tick-render.cpp                  |  18 +++
 src/gamemode/gamemode.h                            |   4 +-
 29 files changed, 834 insertions(+), 94 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/hot-reload/packet-ledger.json
M	devscripts/agent-wake.py
M	devscripts/agent_coord.py
M	devscripts/blender-ai
M	devscripts/dashboard-keepalive.py
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	docs/ROUTER.md
M	docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
M	docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
M	docs/regressions/dev-loop/README.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/agent-wake.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/dev-loop/scenario-input-recording.md
M	docs/specs/networking/contact-authority.md
M	docs/specs/performance/performance.md
M	src/config/collision-config.h
M	src/engine/engine-tick-render.cpp
M	src/gamemode/gamemode.h
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-20T05:38:11.772319Z",
    "latest_successful_build_id": "2026-11-20T05:38:11.772319Z:06e0eafc8e0291960e9d19162dd5e6355d49d88d702b6febf7a6e6890d5a545b",
    "latest_successful_source_generation": 0
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "b1e8824cc320422d440880ae80f1e1bf7c91b76b8cf20983ddd3a7f7126062be",
    "dll": "build\\game\\generation-394\\game-g394.dll",
    "generation": 394,
    "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-394\\game-g394.dll",
    "generation": 394,
    "published_utc": "2026-11-20T04:05:35.363Z",
    "revision": "81d6c51b237637e34c11ca2e57d74e223c93d2ebe98a4583193278619ac0c0c0",
    "status": "ready"
  },
  "progress": {
    "compiled": 3,
    "elapsed_s": 821.572,
    "phase": "build_complete",
    "result": "success",
    "skipped": 84,
    "updated_utc": "2026-11-20T05:38:00.656Z"
  }
}
```

## Session findings and exact changes

- User request: have agents inspect the shared brain when they message, append useful suggestions to issues, and claim ownership when choosing work.
- Updated `devscripts/agent_coord.py`: `CoordStore.sync()` now includes `brain_check`; every `send()` also returns a fresh brain check with the sender's current issue, recommended next work, and up to five claimable issues. `ask()` and `reply()` use `send()`, so they receive the same check. An issue-linked message (explicit `--issue <id>` or exactly one existing issue ID mentioned in its subject/body) is appended to that issue's durable discussion. Added `issue suggest <id> <body>` for standalone proposals on an existing issue. Added no schema or second owner for messages or issues.
- Ownership behavior: viewing or discussing an issue does not assign it. An agent chooses work with `next --claim`, which uses the existing assignment path.
- Updated `devscripts/agent-wake.py` to tell woken agents to review the brain check, link relevant messages to issues, append suggestions, and claim only chosen work. Updated `AGENTS.md` and `docs/specs/dev-loop/agent-wake.md` with the same workflow and acceptance rules.
- Tracker: appended the user's latest wording and an implementation status/scope note to `issue-a271b0296faa`; assigned it to `brain-check-hook`, created task `message-brain-check`, and left that task `in_progress`. The broader automatic delivery of incoming Codex user messages into the repository brain and wakes for idle agents remains unresolved.
- Pre-existing changes: preserved the existing `AGENTS.md` generic-owner section, and existing `devscripts/agent_coord.py` additions `ensure_agent` and `issue_occurrence`. Other worktree edits shown by the generated git status were not changed by this task. During review, `runtime-60hz` held active leases on `docs/specs/dev-loop/agent-coordination.md` and `agent-self-improvement.md`; neither file was edited.
- Exact before/after: before, `sync` returned issue listings and `send` returned only message/thread IDs; no message brain check or issue discussion append existed. After, `sync`/`send`/`ask`/`reply` expose bounded issue context, issue-linked messages and standalone suggestions append to issue history, and the agent prompt instructs how to use it.
- Validation: in-memory Python syntax compilation passed for `devscripts/agent_coord.py` and `devscripts/agent-wake.py`. No tests, build, or runtime activation was run. The task remains open/in progress pending behavioral validation; this does not prove external Codex user messages are automatically broadcast to idle agents.
- Focused reviews: `docs/skills/self-improvement-v1.md` and `docs/skills/issue-triage-v1.md` reviewed. Self-improvement check: preserved verbatim quote, linked evidence and metric/acceptance are in the tracker task; independent review and behavioral evidence remain. Documentation check: updated wake contract and acceptance list; router already routes wake work to this spec. Specification-writing review: stated behavior, boundaries, and acceptance; no current runtime status was added to the spec.
- Human review still needed: review whether brain-check cadence and five-issue cap are useful, then decide how incoming Codex user messages should be durably broadcast and whether/how that path may wake idle agents.
