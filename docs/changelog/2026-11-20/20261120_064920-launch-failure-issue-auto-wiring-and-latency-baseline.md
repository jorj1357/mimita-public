// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Launch failure issue auto-wiring and latency baseline

# Task

- Task ID: session-f6437a953165
- Session ID: session-f6437a953165
- Owner: launch-latency-investigation
- Focus: devscripts/dev_loop/launch_report.py
- Summary: Wire failed launch reports to append/create matching issues and record per-launch issue outcome; add focused checks and baseline evidence to existing launch issues.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-20T06:49:19+00:00
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
| `config/audio/music-settings.json` |  |  |  | git checkout -- config/audio/music-settings.json |
| `config/gamemodes/bloodtest.json` |  |  |  | git checkout -- config/gamemodes/bloodtest.json |
| `config/gamemodes/bombtag.json` |  |  |  | git checkout -- config/gamemodes/bombtag.json |
| `config/gamemodes/connected_world.json` |  |  |  | git checkout -- config/gamemodes/connected_world.json |
| `config/gamemodes/counterstrike.json` |  |  |  | git checkout -- config/gamemodes/counterstrike.json |
| `config/gamemodes/duel.json` |  |  |  | git checkout -- config/gamemodes/duel.json |
| `config/gamemodes/elimination.json` |  |  |  | git checkout -- config/gamemodes/elimination.json |
| `config/gamemodes/ffa.json` |  |  |  | git checkout -- config/gamemodes/ffa.json |
| `config/gamemodes/funcounterstrike.json` |  |  |  | git checkout -- config/gamemodes/funcounterstrike.json |
| `config/gamemodes/jugger_horde.json` |  |  |  | git checkout -- config/gamemodes/jugger_horde.json |
| `config/gamemodes/juggernaut_duel.json` |  |  |  | git checkout -- config/gamemodes/juggernaut_duel.json |
| `config/gamemodes/masterlist/gamemode-masterlist.json` |  |  |  | git checkout -- config/gamemodes/masterlist/gamemode-masterlist.json |
| `config/gamemodes/npc_waves.json` |  |  |  | git checkout -- config/gamemodes/npc_waves.json |
| `config/gamemodes/payload.json` |  |  |  | git checkout -- config/gamemodes/payload.json |
| `config/gamemodes/retrograd.json` |  |  |  | git checkout -- config/gamemodes/retrograd.json |
| `config/gamemodes/rocket_duel.json` |  |  |  | git checkout -- config/gamemodes/rocket_duel.json |
| `config/gamemodes/sandbox.json` |  |  |  | git checkout -- config/gamemodes/sandbox.json |
| `config/gamemodes/ssbm.json` |  |  |  | git checkout -- config/gamemodes/ssbm.json |
| `config/gamemodes/survive_disasters.json` |  |  |  | git checkout -- config/gamemodes/survive_disasters.json |
| `config/gamemodes/tdm.json` |  |  |  | git checkout -- config/gamemodes/tdm.json |
| `config/gamemodes/vehicle_arena.json` |  |  |  | git checkout -- config/gamemodes/vehicle_arena.json |
| `config/gamemodes/vs_jason.json` |  |  |  | git checkout -- config/gamemodes/vs_jason.json |
| `config/gamemodes/zombie_tower.json` |  |  |  | git checkout -- config/gamemodes/zombie_tower.json |
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `config/video-settings.json` |  |  |  | git checkout -- config/video-settings.json |
| `devscripts/agent-wake.py` |  |  |  | git checkout -- devscripts/agent-wake.py |
| `devscripts/agent_coord.py` |  |  |  | git checkout -- devscripts/agent_coord.py |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dashboard-keepalive.py` |  |  |  | git checkout -- devscripts/dashboard-keepalive.py |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/cli.py` |  |  |  | git checkout -- devscripts/dev_loop/cli.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `devscripts/quality/test_agent_coord.py` |  |  |  | git checkout -- devscripts/quality/test_agent_coord.py |
| `devscripts/quality/test_game_build_snapshot.py` |  |  |  | git checkout -- devscripts/quality/test_game_build_snapshot.py |
| `docs/ROUTER.md` |  |  |  | git checkout -- docs/ROUTER.md |
| `docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md` |  |  |  | git checkout -- docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md |
| `docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md` |  |  |  | git checkout -- docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md` |  |  |  | git checkout -- docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md |
| `docs/issues/bug/20261120-fix-launch-latest-stable-mimita-exe-live-executa-issue-bbc291c180f8.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fix-launch-latest-stable-mimita-exe-live-executa-issue-bbc291c180f8.md |
| `docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md` |  |  |  | git checkout -- docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md |
| `docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |
| `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md |
| `docs/regressions/dev-loop/README.md` |  |  |  | git checkout -- docs/regressions/dev-loop/README.md |
| `docs/specs/dev-loop/agent-coordination.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-coordination.md |
| `docs/specs/dev-loop/agent-self-improvement.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-self-improvement.md |
| `docs/specs/dev-loop/agent-wake.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-wake.md |
| `docs/specs/dev-loop/change-to-evidence-latency.md` |  |  |  | git checkout -- docs/specs/dev-loop/change-to-evidence-latency.md |
| `docs/specs/dev-loop/dashboard.md` |  |  |  | git checkout -- docs/specs/dev-loop/dashboard.md |
| `docs/specs/dev-loop/scenario-input-recording.md` |  |  |  | git checkout -- docs/specs/dev-loop/scenario-input-recording.md |
| `docs/specs/networking/contact-authority.md` |  |  |  | git checkout -- docs/specs/networking/contact-authority.md |
| `docs/specs/performance/performance.md` |  |  |  | git checkout -- docs/specs/performance/performance.md |
| `src/actor/player-ai.cpp` |  |  |  | git checkout -- src/actor/player-ai.cpp |
| `src/config/collision-config.h` |  |  |  | git checkout -- src/config/collision-config.h |
| `src/engine/engine-tick-render.cpp` |  |  |  | git checkout -- src/engine/engine-tick-render.cpp |
| `src/gamemode/gamemode.h` |  |  |  | git checkout -- src/gamemode/gamemode.h |
| `devscripts/dev_loop/launch_report.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/dev_loop/launch_report.py' |
| `devscripts/quality/test_launch_report.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_launch_report.py' |
| `devscripts/tech-debt-audit.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/tech-debt-audit.py' |
| `docs/changelog/2026-11-20/20261120_010448-build-wait-and-agent-time-rules.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_010448-build-wait-and-agent-time-rules.md' |
| `docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md' |
| `docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md' |
| `docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md' |
| `docs/changelog/2026-11-20/20261120_045549-atomic-patch-and-issue-loop-audit.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_045549-atomic-patch-and-issue-loop-audit.md' |
| `docs/changelog/2026-11-20/20261120_050000-culling-specs-ai-controller-issues.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_050000-culling-specs-ai-controller-issues.md' |
| `docs/changelog/2026-11-20/20261120_050111-user-context-notification-issue.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_050111-user-context-notification-issue.md' |
| `docs/changelog/2026-11-20/20261120_051943-prove-dashboard-monitor-position-survives-restart.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_051943-prove-dashboard-monitor-position-survives-restart.md' |
| `docs/changelog/2026-11-20/20261120_055045-agent-brain-check-issue-suggestions.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_055045-agent-brain-check-issue-suggestions.md' |
| `docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md' |
| `docs/changelog/2026-11-20/20261120_064450-arcade-collision-default-all-modes.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_064450-arcade-collision-default-all-modes.md' |
| `docs/changelog/2026-11-20/20261120_064821-launch-issue-autowire-and-latency-baseline.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_064821-launch-issue-autowire-and-latency-baseline.md' |
| `docs/gold/2026-11-20-juggernaut-60hz-server-and-client-culling.md` |  |  |  | Remove-Item -LiteralPath 'docs/gold/2026-11-20-juggernaut-60hz-server-and-client-culling.md' |
| `docs/issues/architecture/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/architecture/' |
| `docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md' |
| `docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md' |
| `docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md' |
| `docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md' |
| `docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md' |
| `docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md' |
| `docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md' |
| `docs/issues/build-friction/20261120-game-dll-build-blocked-by-low-disk-space-issue-916633ab3313.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-game-dll-build-blocked-by-low-disk-space-issue-916633ab3313.md' |
| `docs/issues/feature/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/feature/' |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md' |
| `docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md' |
| `docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md' |
| `docs/issues/improvement/20261120-culling-lod-and-work-avoidance-backlog-issue-5f49047dae49.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-culling-lod-and-work-avoidance-backlog-issue-5f49047dae49.md' |
| `docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md' |
| `docs/issues/improvement/20261120-make-arcade-collision-the-default-for-all-gamemo-issue-56f7a692b646.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-make-arcade-collision-the-default-for-all-gamemo-issue-56f7a692b646.md' |
| `docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md' |
| `docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md' |
| `docs/issues/improvement/20261120-track-agent-work-time-and-phase-latency-issue-89c11130189a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261120-track-agent-work-time-and-phase-latency-issue-89c11130189a.md' |
| `docs/issues/tech-debt/` |  |  |  | Remove-Item -LiteralPath 'docs/issues/tech-debt/' |
| `docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md` |  |  |  | Remove-Item -LiteralPath 'docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md' |
| `docs/skills/performance-culling-v1.md` |  |  |  | Remove-Item -LiteralPath 'docs/skills/performance-culling-v1.md' |
| `docs/specs/architecture/` |  |  |  | Remove-Item -LiteralPath 'docs/specs/architecture/' |
| `docs/specs/dev-loop/agent-context-contract.md` |  |  |  | Remove-Item -LiteralPath 'docs/specs/dev-loop/agent-context-contract.md' |
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

- Owner: launch-latency-investigation
- Focus: devscripts/dev_loop/launch_report.py
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / ce0009d5b2983f462d1d17dd640c4e66769c2a23 / ce0009d5b2983f462d1d17dd640c4e66769c2a23
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-20T05:38:11.772319Z", "latest_successful_build_id": "2026-11-20T05:38:11.772319Z:06e0eafc8e0291960e9d19162dd5e6355d49d88d702b6febf7a6e6890d5a545b", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "2e5e23ed5aa36185c79cfe940bd110209d357e161fce9d6b2201d3ffe6e8ff2b", "dll": "build\\game\\generation-397\\game-g397.dll", "generation": 397, "revision": "143c336f3258ee55a2e6a3035fef954d2a818830d3d4e46d289b0a1049e107cb", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-397\\game-g397.dll", "generation": 397, "published_utc": "2026-11-20T06:47:53.563Z", "revision": "143c336f3258ee55a2e6a3035fef954d2a818830d3d4e46d289b0a1049e107cb", "status": "ready"}, "progress": {"compiled": 3, "elapsed_s": 821.572, "phase": "build_complete", "result": "success", "skipped": 84, "updated_utc": "2026-11-20T05:38:00.656Z"}}
- Generation: 397
- Artifact: build\game\generation-397\game-g397.dll
- Journal path: C:\mimita-v9\logs\2026-11-20\20261120_063800_01\dev-loop\events.jsonl
- Session ID: session-f6437a953165

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
 M config/audio/music-settings.json
 M config/gamemodes/bloodtest.json
 M config/gamemodes/bombtag.json
 M config/gamemodes/connected_world.json
 M config/gamemodes/counterstrike.json
 M config/gamemodes/duel.json
 M config/gamemodes/elimination.json
 M config/gamemodes/ffa.json
 M config/gamemodes/funcounterstrike.json
 M config/gamemodes/jugger_horde.json
 M config/gamemodes/juggernaut_duel.json
 M config/gamemodes/masterlist/gamemode-masterlist.json
 M config/gamemodes/npc_waves.json
 M config/gamemodes/payload.json
 M config/gamemodes/retrograd.json
 M config/gamemodes/rocket_duel.json
 M config/gamemodes/sandbox.json
 M config/gamemodes/ssbm.json
 M config/gamemodes/survive_disasters.json
 M config/gamemodes/tdm.json
 M config/gamemodes/vehicle_arena.json
 M config/gamemodes/vs_jason.json
 M config/gamemodes/zombie_tower.json
 M config/hot-reload/packet-ledger.json
 M config/video-settings.json
 M devscripts/agent-wake.py
 M devscripts/agent_coord.py
 m devscripts/blender-ai
 M devscripts/dashboard-keepalive.py
 M devscripts/dev-loop.py
 M devscripts/dev_loop/cli.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/quality/test_agent_coord.py
 M devscripts/quality/test_game_build_snapshot.py
 M docs/ROUTER.md
 M docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
 M docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
 M docs/issues/bug/20261120-fix-launch-latest-stable-mimita-exe-live-executa-issue-bbc291c180f8.md
 M docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md
 M docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/regressions/dev-loop/README.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/agent-wake.md
 M docs/specs/dev-loop/change-to-evidence-latency.md
 M docs/specs/dev-loop/dashboard.md
 M docs/specs/dev-loop/scenario-input-recording.md
 M docs/specs/networking/contact-authority.md
 M docs/specs/performance/performance.md
 M src/actor/player-ai.cpp
 M src/config/collision-config.h
 M src/engine/engine-tick-render.cpp
 M src/gamemode/gamemode.h
?? devscripts/dev_loop/launch_report.py
?? devscripts/quality/test_launch_report.py
?? devscripts/tech-debt-audit.py
?? docs/changelog/2026-11-20/20261120_010448-build-wait-and-agent-time-rules.md
?? docs/changelog/2026-11-20/20261120_041150-safe-same-file-patch-handoff.md
?? docs/changelog/2026-11-20/20261120_042600-contact-purpose-graph-ontology-tech-debt-audit.md
?? docs/changelog/2026-11-20/20261120_043533-keep-dashboard-maximize-on-its-monitor.md
?? docs/changelog/2026-11-20/20261120_045549-atomic-patch-and-issue-loop-audit.md
?? docs/changelog/2026-11-20/20261120_050000-culling-specs-ai-controller-issues.md
?? docs/changelog/2026-11-20/20261120_050111-user-context-notification-issue.md
?? docs/changelog/2026-11-20/20261120_051943-prove-dashboard-monitor-position-survives-restart.md
?? docs/changelog/2026-11-20/20261120_055045-agent-brain-check-issue-suggestions.md
?? docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
?? docs/changelog/2026-11-20/20261120_064450-arcade-collision-default-all-modes.md
?? docs/changelog/2026-11-20/20261120_064821-launch-issue-autowire-and-latency-baseline.md
?? docs/gold/2026-11-20-juggernaut-60hz-server-and-client-culling.md
?? docs/issues/architecture/
?? docs/issues/bug/20261120-audit-npc-target-visibility-and-behavior-behind--issue-82cdc1277704.md
?? docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md
?? docs/issues/bug/20261120-dashboard-snaps-to-another-monitor-issue-6e59229d7386.md
?? docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
?? docs/issues/bug/20261120-juggernaut-mode-npcs-do-not-fire-at-opposing-act-issue-1b2463da5666.md
?? docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
?? docs/issues/bug/20261120-upgrade-the-local-ai-player-controller-issue-f8873319f12b.md
?? docs/issues/build-friction/20261120-game-dll-build-blocked-by-low-disk-space-issue-916633ab3313.md
?? docs/issues/feature/
?? docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
?? docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
?? docs/issues/improvement/20261120-capture-a-screenshot-at-a-specific-tick-for-proo-issue-89efbbc86535.md
?? docs/issues/improvement/20261120-culling-lod-and-work-avoidance-backlog-issue-5f49047dae49.md
?? docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
?? docs/issues/improvement/20261120-make-arcade-collision-the-default-for-all-gamemo-issue-56f7a692b646.md
?? docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
?? docs/issues/improvement/20261120-notify-agents-when-the-user-adds-new-context-issue-a271b0296faa.md
?? docs/issues/improvement/20261120-track-agent-work-time-and-phase-latency-issue-89c11130189a.md
?? docs/issues/tech-debt/
?? docs/regressions/dev-loop/dashboard-monitor-snap-REG-2026-11-20.md
?? docs/skills/performance-culling-v1.md
?? docs/specs/architecture/
?? docs/specs/dev-loop/agent-context-contract.md
?? docs/specs/dev-loop/launch-feedback.md
?? docs/specs/dev-loop/visual-proof-screenshots.md
?? docs/specs/performance/culling-and-work-avoidance.md
```

## Captured diff stat

```text
AGENTS.md                                          |  19 +
 config/accounts/default.json                       |   8 +-
 config/analytics.json                              |   4 +-
 config/audio/music-settings.json                   |   6 +-
 config/gamemodes/bloodtest.json                    |   1 +
 config/gamemodes/bombtag.json                      |   1 +
 config/gamemodes/connected_world.json              |   1 +
 config/gamemodes/counterstrike.json                |   1 +
 config/gamemodes/duel.json                         |   1 +
 config/gamemodes/elimination.json                  |   1 +
 config/gamemodes/ffa.json                          |   1 +
 config/gamemodes/funcounterstrike.json             |   1 +
 config/gamemodes/jugger_horde.json                 |   1 +
 config/gamemodes/juggernaut_duel.json              |   1 +
 .../gamemodes/masterlist/gamemode-masterlist.json  |   4 +-
 config/gamemodes/npc_waves.json                    |   1 +
 config/gamemodes/payload.json                      |   1 +
 config/gamemodes/retrograd.json                    |   1 +
 config/gamemodes/rocket_duel.json                  |   1 +
 config/gamemodes/sandbox.json                      |   1 +
 config/gamemodes/ssbm.json                         |   1 +
 config/gamemodes/survive_disasters.json            |   1 +
 config/gamemodes/tdm.json                          |   1 +
 config/gamemodes/vehicle_arena.json                |   1 +
 config/gamemodes/vs_jason.json                     |   1 +
 config/gamemodes/zombie_tower.json                 |   1 +
 config/hot-reload/packet-ledger.json               |  12 +-
 config/video-settings.json                         |   2 +-
 devscripts/agent-wake.py                           |  16 +-
 devscripts/agent_coord.py                          | 589 ++++++++++++++++++++-
 devscripts/blender-ai                              |   0
 devscripts/dashboard-keepalive.py                  |   6 +-
 devscripts/dev-loop.py                             | 117 +++-
 devscripts/dev_loop/cli.py                         |   6 +
 devscripts/dev_loop/game_build.py                  |  89 +++-
 devscripts/dev_loop/gui.py                         | 141 ++++-
 devscripts/quality/test_agent_coord.py             |  76 +++
 devscripts/quality/test_game_build_snapshot.py     |  17 +
 docs/ROUTER.md                                     |   7 +-
 ...dination-brain-and-build-fan-out-attribution.md |  17 +-
 .../20261120_040000-live-dashboard-build-health.md |  19 +-
 docs/issues/README.md                              |  31 +-
 ...tency-limbs-only-capsule--issue-da337d1fe2f3.md |  84 +++
 ...e-mimita-exe-live-executa-issue-bbc291c180f8.md |  28 +
 ...asses-through-and-is-not--issue-04079f42c6f9.md |   8 +
 ...wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |  28 +
 ...move-avoidable-build-and--issue-e044a931c54b.md |  14 +
 docs/regressions/dev-loop/README.md                |   9 +-
 docs/specs/dev-loop/agent-coordination.md          |  74 +++
 docs/specs/dev-loop/agent-self-improvement.md      |  68 +++
 docs/specs/dev-loop/agent-wake.md                  |  24 +
 docs/specs/dev-loop/change-to-evidence-latency.md  |  19 +
 docs/specs/dev-loop/dashboard.md                   |  25 +-
 docs/specs/dev-loop/scenario-input-recording.md    |  12 +
 docs/specs/networking/contact-authority.md         |  86 +++
 docs/specs/performance/performance.md              |  39 +-
 src/actor/player-ai.cpp                            |  62 ++-
 src/config/collision-config.h                      |   6 +-
 src/engine/engine-tick-render.cpp                  |  18 +
 src/gamemode/gamemode.h                            |   4 +-
 60 files changed, 1688 insertions(+), 127 deletions(-)
```

## Captured changed paths

```text
M	AGENTS.md
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/gamemodes/bloodtest.json
M	config/gamemodes/bombtag.json
M	config/gamemodes/connected_world.json
M	config/gamemodes/counterstrike.json
M	config/gamemodes/duel.json
M	config/gamemodes/elimination.json
M	config/gamemodes/ffa.json
M	config/gamemodes/funcounterstrike.json
M	config/gamemodes/jugger_horde.json
M	config/gamemodes/juggernaut_duel.json
M	config/gamemodes/masterlist/gamemode-masterlist.json
M	config/gamemodes/npc_waves.json
M	config/gamemodes/payload.json
M	config/gamemodes/retrograd.json
M	config/gamemodes/rocket_duel.json
M	config/gamemodes/sandbox.json
M	config/gamemodes/ssbm.json
M	config/gamemodes/survive_disasters.json
M	config/gamemodes/tdm.json
M	config/gamemodes/vehicle_arena.json
M	config/gamemodes/vs_jason.json
M	config/gamemodes/zombie_tower.json
M	config/hot-reload/packet-ledger.json
M	config/video-settings.json
M	devscripts/agent-wake.py
M	devscripts/agent_coord.py
M	devscripts/blender-ai
M	devscripts/dashboard-keepalive.py
M	devscripts/dev-loop.py
M	devscripts/dev_loop/cli.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/quality/test_agent_coord.py
M	devscripts/quality/test_game_build_snapshot.py
M	docs/ROUTER.md
M	docs/changelog/2026-11-19/20261119_231035-shared-agent-coordination-brain-and-build-fan-out-attribution.md
M	docs/changelog/2026-11-20/20261120_040000-live-dashboard-build-health.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-arcade-collision-consistency-limbs-only-capsule--issue-da337d1fe2f3.md
M	docs/issues/bug/20261120-fix-launch-latest-stable-mimita-exe-live-executa-issue-bbc291c180f8.md
M	docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md
M	docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/regressions/dev-loop/README.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/agent-wake.md
M	docs/specs/dev-loop/change-to-evidence-latency.md
M	docs/specs/dev-loop/dashboard.md
M	docs/specs/dev-loop/scenario-input-recording.md
M	docs/specs/networking/contact-authority.md
M	docs/specs/performance/performance.md
M	src/actor/player-ai.cpp
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
    "code_hash": "2e5e23ed5aa36185c79cfe940bd110209d357e161fce9d6b2201d3ffe6e8ff2b",
    "dll": "build\\game\\generation-397\\game-g397.dll",
    "generation": 397,
    "revision": "143c336f3258ee55a2e6a3035fef954d2a818830d3d4e46d289b0a1049e107cb",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-397\\game-g397.dll",
    "generation": 397,
    "published_utc": "2026-11-20T06:47:53.563Z",
    "revision": "143c336f3258ee55a2e6a3035fef954d2a818830d3d4e46d289b0a1049e107cb",
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
