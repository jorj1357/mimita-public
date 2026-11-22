// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# FCS NPC objective route recovery evidence

# Task

- Task ID: session-9a25bcfc1a50
- Session ID: session-9a25bcfc1a50
- Owner: Codex
- Focus: Fun Counter Strike NPC behavior
- Summary: Saved a no-visible-target navigation correction in src/npc/npc.cpp and built it in game DLL generation 439; generation 440 also includes the saved patch. A long managed gen440 run showed travel toward the site and NPC damage against a player, but repeated route failures persist and the journal has no NPC-on-NPC kill or bomb plant proof. The isolated gen439 test ended on a readiness-stage mismatch. Restarted the wedged checkout dev-loop daemon via its watchdog while preserving the shared game server; stopped the isolated runtime through the recovered manager. Task remains unproven.
- Status: in-progress
- Date, time, timezone: 2026-11-22T02:26:07+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 07ba9003a6be33556ff350eb97be042fd18a99dc
- Final commit: 07ba9003a6be33556ff350eb97be042fd18a99dc

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
| `config/movement/movement-source.json` |  |  |  | git checkout -- config/movement/movement-source.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/dev-loop.py` |  |  |  | git checkout -- devscripts/dev-loop.py |
| `devscripts/dev_loop/game_build.py` |  |  |  | git checkout -- devscripts/dev_loop/game_build.py |
| `devscripts/dev_loop/gui.py` |  |  |  | git checkout -- devscripts/dev_loop/gui.py |
| `devscripts/dev_loop/parallel_test_runs.py` |  |  |  | git checkout -- devscripts/dev_loop/parallel_test_runs.py |
| `devscripts/packet_registry_codegen.py` |  |  |  | git checkout -- devscripts/packet_registry_codegen.py |
| `devscripts/source_manifest.py` |  |  |  | git checkout -- devscripts/source_manifest.py |
| `docs/changelog/2026-11-21/20261121_234000-daemon-status-journal-cpu-reduction.md` |  |  |  | git checkout -- docs/changelog/2026-11-21/20261121_234000-daemon-status-journal-cpu-reduction.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/architecture/20261120-one-contact-path-with-purpose-and-damage-flags-s-issue-93bd29a36d24.md` |  |  |  | git checkout -- docs/issues/architecture/20261120-one-contact-path-with-purpose-and-damage-flags-s-issue-93bd29a36d24.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md` |  |  |  | git checkout -- docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md |
| `docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md |
| `docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md` |  |  |  | git checkout -- docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md |
| `docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |
| `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md |
| `docs/issues/capability/20261122-enable-bounded-authenticated-ai-website-fixes-an-issue-260667351e04.md` |  |  |  | git checkout -- docs/issues/capability/20261122-enable-bounded-authenticated-ai-website-fixes-an-issue-260667351e04.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md |
| `docs/issues/improvement/20261120-permit-safe-non-overlapping-section-leases-in-sh-issue-c55b45bd457f.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-permit-safe-non-overlapping-section-leases-in-sh-issue-c55b45bd457f.md |
| `docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/regressions/gameplay/README.md` |  |  |  | git checkout -- docs/regressions/gameplay/README.md |
| `docs/regressions/gameplay/website-auth-database-REG-2026-09-16.md` |  |  |  | git checkout -- docs/regressions/gameplay/website-auth-database-REG-2026-09-16.md |
| `src/network/reliable-gameplay-events.cpp` |  |  |  | git checkout -- src/network/reliable-gameplay-events.cpp |
| `src/network/server.cpp` |  |  |  | git checkout -- src/network/server.cpp |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `docs/changelog/2026-11-21/20261121_231800-cpu-and-build-efficiency-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_231800-cpu-and-build-efficiency-investigation.md' |
| `docs/changelog/2026-11-21/20261121_235111-bloodtest-full-mode-issue-and-implementation-plan.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/20261121_235111-bloodtest-full-mode-issue-and-implementation-plan.md' |
| `docs/changelog/2026-11-22/20261122_000927-mimita-fun-database-outage-investigation.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_000927-mimita-fun-database-outage-investigation.md' |
| `docs/changelog/2026-11-22/20261122_001700-issue-421e4a6513c2-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_001700-issue-421e4a6513c2-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_010605-mimita-fun-postgresql-recovery-and-alert-gaps.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_010605-mimita-fun-postgresql-recovery-and-alert-gaps.md' |
| `docs/changelog/2026-11-22/20261122_011200-issue-36579a10979a-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_011200-issue-36579a10979a-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_011811-fix-isolated-test-run-process-identity.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_011811-fix-isolated-test-run-process-identity.md' |
| `docs/changelog/2026-11-22/20261122_012642-issue-c55b45bd457f-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_012642-issue-c55b45bd457f-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_013800-bounded-website-agent-release-path.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_013800-bounded-website-agent-release-path.md' |
| `docs/changelog/2026-11-22/20261122_014000-cpp-latency-cache-and-snapshot-followup.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_014000-cpp-latency-cache-and-snapshot-followup.md' |
| `docs/changelog/2026-11-22/20261122_014500-ctx-4f1aa0089f64-active-gameplay-cpu-evidence.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_014500-ctx-4f1aa0089f64-active-gameplay-cpu-evidence.md' |
| `docs/changelog/2026-11-22/20261122_014700-ctx-4f1aa0089f64-routing-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_014700-ctx-4f1aa0089f64-routing-review.md' |
| `docs/changelog/2026-11-22/20261122_021111-issue-9306252422c4-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_021111-issue-9306252422c4-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_021456-generation440-preworker-timing-and-resource-pressure.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_021456-generation440-preworker-timing-and-resource-pressure.md' |
| `docs/changelog/2026-11-22/20261122_021504-issue-4891874dd02b-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_021504-issue-4891874dd02b-build-wait-review.md' |
| `docs/issues/bug/20261121-investigate-recurring-mimita-fun-login-and-datab-issue-1759b2cc6e65.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-investigate-recurring-mimita-fun-login-and-datab-issue-1759b2cc6e65.md' |
| `docs/issues/bug/20261121-investigate-recurring-mimita-fun-login-and-datab-issue-f8b4b3a4716d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-investigate-recurring-mimita-fun-login-and-datab-issue-f8b4b3a4716d.md' |
| `docs/issues/bug/20261122-fun-counter-strike-npcs-get-stuck-instead-of-rea-issue-04bce0df0d33.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261122-fun-counter-strike-npcs-get-stuck-instead-of-rea-issue-04bce0df0d33.md' |
| `docs/issues/bug/20261122-fun-counter-strike-npcs-get-stuck-instead-of-rea-issue-cb16e118f63c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261122-fun-counter-strike-npcs-get-stuck-instead-of-rea-issue-cb16e118f63c.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-421e4a6513c2.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-421e4a6513c2.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-17abc587c89a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-17abc587c89a.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-4891874dd02b.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-4891874dd02b.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-00d61cbc12a2.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-00d61cbc12a2.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-4409099b6172.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-4409099b6172.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-565c90a0d817.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-565c90a0d817.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-9306252422c4.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-9306252422c4.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d220df93f7cb.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d220df93f7cb.md' |
| `docs/issues/tech-debt/20261122-wake-envelope-messages-are-duplicated-into-unlin-issue-f643532871ac.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/tech-debt/20261122-wake-envelope-messages-are-duplicated-into-unlin-issue-f643532871ac.md' |

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

- Owner: Codex
- Focus: Fun Counter Strike NPC behavior
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 07ba9003a6be33556ff350eb97be042fd18a99dc / 07ba9003a6be33556ff350eb97be042fd18a99dc
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-21T02:26:50.306117Z", "latest_successful_build_id": "2026-11-21T02:26:50.306117Z:8b5838cbbf4c1bcda47c3f35e33583fde83bdacc89bdc52961e13980c976b871", "latest_successful_source_generation": 0}, "game_current": {"build_mode": "debug", "code_hash": "c9099e1bd97c81f640df89170b2ac41dedb091aef4ba82495173221c55a1a51c", "dll": "build\\game\\generation-440\\game-g440.dll", "generation": 440, "revision": "7e7b5f25b5b9038a9df66a27c961754310d0877221dc1d2decbf7624d4a20fbd", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-440\\game-g440.dll", "generation": 440, "published_utc": "2026-11-22T01:47:28.618Z", "revision": "7e7b5f25b5b9038a9df66a27c961754310d0877221dc1d2decbf7624d4a20fbd", "status": "ready"}, "progress": {"compiled": 12, "elapsed_s": 346.097, "phase": "build_complete", "result": "success", "skipped": 75, "updated_utc": "2026-11-22T01:53:27.861Z"}}
- Generation: 440
- Artifact: build\game\generation-440\game-g440.dll
- Journal path: C:\mimita-v9\logs\2026-11-22\20261122_021421\server-01\events.jsonl
- Session ID: session-9a25bcfc1a50

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
 M config/movement/movement-source.json
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/packet_registry_codegen.py
 M devscripts/source_manifest.py
 M docs/changelog/2026-11-21/20261121_234000-daemon-status-journal-cpu-reduction.md
 M docs/issues/README.md
 M docs/issues/architecture/20261120-one-contact-path-with-purpose-and-damage-flags-s-issue-93bd29a36d24.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md
 M docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md
 M docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
 M docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/capability/20261122-enable-bounded-authenticated-ai-website-fixes-an-issue-260667351e04.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
 M docs/issues/improvement/20261120-permit-safe-non-overlapping-section-leases-in-sh-issue-c55b45bd457f.md
 M docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/regressions/gameplay/README.md
 M docs/regressions/gameplay/website-auth-database-REG-2026-09-16.md
 M src/network/reliable-gameplay-events.cpp
 M src/network/server.cpp
 M src/npc/npc.cpp
?? docs/changelog/2026-11-21/20261121_231800-cpu-and-build-efficiency-investigation.md
?? docs/changelog/2026-11-21/20261121_235111-bloodtest-full-mode-issue-and-implementation-plan.md
?? docs/changelog/2026-11-22/20261122_000927-mimita-fun-database-outage-investigation.md
?? docs/changelog/2026-11-22/20261122_001700-issue-421e4a6513c2-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_010605-mimita-fun-postgresql-recovery-and-alert-gaps.md
?? docs/changelog/2026-11-22/20261122_011200-issue-36579a10979a-independent-review.md
?? docs/changelog/2026-11-22/20261122_011811-fix-isolated-test-run-process-identity.md
?? docs/changelog/2026-11-22/20261122_012642-issue-c55b45bd457f-independent-review.md
?? docs/changelog/2026-11-22/20261122_013800-bounded-website-agent-release-path.md
?? docs/changelog/2026-11-22/20261122_014000-cpp-latency-cache-and-snapshot-followup.md
?? docs/changelog/2026-11-22/20261122_014500-ctx-4f1aa0089f64-active-gameplay-cpu-evidence.md
?? docs/changelog/2026-11-22/20261122_014700-ctx-4f1aa0089f64-routing-review.md
?? docs/changelog/2026-11-22/20261122_021111-issue-9306252422c4-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_021456-generation440-preworker-timing-and-resource-pressure.md
?? docs/changelog/2026-11-22/20261122_021504-issue-4891874dd02b-build-wait-review.md
?? docs/issues/bug/20261121-investigate-recurring-mimita-fun-login-and-datab-issue-1759b2cc6e65.md
?? docs/issues/bug/20261121-investigate-recurring-mimita-fun-login-and-datab-issue-f8b4b3a4716d.md
?? docs/issues/bug/20261122-fun-counter-strike-npcs-get-stuck-instead-of-rea-issue-04bce0df0d33.md
?? docs/issues/bug/20261122-fun-counter-strike-npcs-get-stuck-instead-of-rea-issue-cb16e118f63c.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-421e4a6513c2.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-17abc587c89a.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-4891874dd02b.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-00d61cbc12a2.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-4409099b6172.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-565c90a0d817.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-9306252422c4.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d220df93f7cb.md
?? docs/issues/tech-debt/20261122-wake-envelope-messages-are-duplicated-into-unlin-issue-f643532871ac.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   4 +-
 config/analytics.json                              |   2 +-
 config/movement/movement-source.json               |   6 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             |  12 +-
 devscripts/dev_loop/game_build.py                  | 262 +++++++++++++++++++--
 devscripts/dev_loop/gui.py                         |  32 +++
 devscripts/dev_loop/parallel_test_runs.py          |  20 +-
 devscripts/packet_registry_codegen.py              |  59 +++--
 devscripts/source_manifest.py                      | 241 +++++++++++++++++--
 ...1_234000-daemon-status-journal-cpu-reduction.md |  32 +++
 docs/issues/README.md                              |  42 ++--
 ...urpose-and-damage-flags-s-issue-93bd29a36d24.md |  34 +++
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  38 +++
 ...asses-through-and-is-not--issue-04079f42c6f9.md |  26 ++
 ...nch-opencode-api-key-expi-issue-36579a10979a.md |  42 ++++
 ...on-projectile-gore-and-sl-issue-2e7b0cd6f868.md |  45 ++++
 ...wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |  42 ++++
 ...move-avoidable-build-and--issue-e044a931c54b.md |  12 +
 ...cated-ai-website-fixes-an-issue-260667351e04.md |  10 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  16 ++
 ...nd-observe-context-bundle-issue-6907072bb155.md |  16 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |   6 +
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  58 +++++
 ...ompleteness-and-readiness-issue-3100aa7b5807.md |  24 ++
 ...ping-section-leases-in-sh-issue-c55b45bd457f.md |  34 +++
 ...-and-build-to-proof-links-issue-b396205aaecf.md |  12 +
 ...uild-stage-and-investigat-issue-136d4553ff92.md | 177 ++++++++++++++
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md | 170 +++++++++++++
 docs/regressions/gameplay/README.md                |   4 +-
 .../website-auth-database-REG-2026-09-16.md        |  72 ++++++
 src/network/reliable-gameplay-events.cpp           |  80 +++----
 src/network/server.cpp                             |  79 +++++--
 src/npc/npc.cpp                                    |  22 +-
 34 files changed, 1568 insertions(+), 163 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/movement/movement-source.json
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/packet_registry_codegen.py
M	devscripts/source_manifest.py
M	docs/changelog/2026-11-21/20261121_234000-daemon-status-journal-cpu-reduction.md
M	docs/issues/README.md
M	docs/issues/architecture/20261120-one-contact-path-with-purpose-and-damage-flags-s-issue-93bd29a36d24.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/bug/20261120-visible-melee-contact-passes-through-and-is-not--issue-04079f42c6f9.md
M	docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md
M	docs/issues/bug/20261121-bloodtest-floor-collision-projectile-gore-and-sl-issue-2e7b0cd6f868.md
M	docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/capability/20261122-enable-bounded-authenticated-ai-website-fixes-an-issue-260667351e04.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-agent-wake-next-work-and-observe-context-bundle-issue-6907072bb155.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
M	docs/issues/improvement/20261120-permit-safe-non-overlapping-section-leases-in-sh-issue-c55b45bd457f.md
M	docs/issues/improvement/20261120-runtime-registry-and-build-to-proof-links-issue-b396205aaecf.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/regressions/gameplay/README.md
M	docs/regressions/gameplay/website-auth-database-REG-2026-09-16.md
M	src/network/reliable-gameplay-events.cpp
M	src/network/server.cpp
M	src/npc/npc.cpp
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
    "code_hash": "c9099e1bd97c81f640df89170b2ac41dedb091aef4ba82495173221c55a1a51c",
    "dll": "build\\game\\generation-440\\game-g440.dll",
    "generation": 440,
    "revision": "7e7b5f25b5b9038a9df66a27c961754310d0877221dc1d2decbf7624d4a20fbd",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-440\\game-g440.dll",
    "generation": 440,
    "published_utc": "2026-11-22T01:47:28.618Z",
    "revision": "7e7b5f25b5b9038a9df66a27c961754310d0877221dc1d2decbf7624d4a20fbd",
    "status": "ready"
  },
  "progress": {
    "compiled": 12,
    "elapsed_s": 346.097,
    "phase": "build_complete",
    "result": "success",
    "skipped": 75,
    "updated_utc": "2026-11-22T01:53:27.861Z"
  }
}
```
