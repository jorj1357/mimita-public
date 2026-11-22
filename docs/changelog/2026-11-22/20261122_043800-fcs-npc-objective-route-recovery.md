// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# FCS NPC objective route recovery

# Task

- Task ID: session-767d0984527b
- Session ID: session-767d0984527b
- Owner: Codex
- Focus: Fun Counter Strike NPC navigation and objective behavior
- Summary: Kept objective-directed NPC fallback movement aligned with the navigator commitment. Generation 454 runtime produced two NPC bomb plants, 67 NPC-on-NPC kills, and 320 successful objective fallback applications; recorded runtime proof.
- Status: in-progress
- Date, time, timezone: 2026-11-22T04:37:56+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: 9fd637d595a308483a9f82c88349a5915c82a0ca
- Final commit: 9fd637d595a308483a9f82c88349a5915c82a0ca

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
| `config/hot-reload/packet-ledger.json` |  |  |  | git checkout -- config/hot-reload/packet-ledger.json |
| `devscripts/blender-ai` |  |  |  | git checkout -- devscripts/blender-ai |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `devscripts/quality/test_game_build_snapshot.py` |  |  |  | git checkout -- devscripts/quality/test_game_build_snapshot.py |
| `docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md` |  |  |  | git checkout -- docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md |
| `docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md` |  |  |  | git checkout -- docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md` |  |  |  | git checkout -- docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | git checkout -- docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md |
| `docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md` |  |  |  | git checkout -- docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md |
| `docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md` |  |  |  | git checkout -- docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md |
| `docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md` |  |  |  | git checkout -- docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md |
| `src/gui/ui-system-buttons.cpp` |  |  |  | git checkout -- src/gui/ui-system-buttons.cpp |
| `src/gui/ui-system-internal.h` |  |  |  | git checkout -- src/gui/ui-system-internal.h |
| `src/gui/ui-system.cpp` |  |  |  | git checkout -- src/gui/ui-system.cpp |
| `src/gui/ui-text-input.cpp` |  |  |  | git checkout -- src/gui/ui-text-input.cpp |
| `src/main-systems.cpp` |  |  |  | git checkout -- src/main-systems.cpp |
| `src/network/multiplayer-tick.cpp` |  |  |  | git checkout -- src/network/multiplayer-tick.cpp |
| `src/npc/npc-behavior-module.cpp` |  |  |  | git checkout -- src/npc/npc-behavior-module.cpp |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `docs/changelog/2026-11-22/20261122_032700-fcs-npc-events-and-plant-proof-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_032700-fcs-npc-events-and-plant-proof-review.md' |
| `docs/changelog/2026-11-22/20261122_035016-issue-ec19949cbd21-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035016-issue-ec19949cbd21-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_035017-issue-8aae0621645f-exe-build-setup-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035017-issue-8aae0621645f-exe-build-setup-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_035023-cpp-latency-pressure-profile-fcs-npc-evidence-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035023-cpp-latency-pressure-profile-fcs-npc-evidence-review.md' |
| `docs/changelog/2026-11-22/20261122_035200-ctx-1f0fb5aa9253-zero-cpu-reading-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035200-ctx-1f0fb5aa9253-zero-cpu-reading-review.md' |
| `docs/changelog/2026-11-22/20261122_035600-issue-a59a0058df76-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_035600-issue-a59a0058df76-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_040332-issue-f534b31a9a1a-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_040332-issue-f534b31a9a1a-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_040900-issue-0968dd48e9d8-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_040900-issue-0968dd48e9d8-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_041142-issue-75220121d993-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_041142-issue-75220121d993-independent-review.md' |
| `docs/changelog/2026-11-22/20261122_041300-issue-f92715ba3c7f-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_041300-issue-f92715ba3c7f-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_042300-fcs-network-and-console-ui-followup.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_042300-fcs-network-and-console-ui-followup.md' |
| `docs/changelog/2026-11-22/20261122_042500-issue-4d9eb4b1a8ad-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_042500-issue-4d9eb4b1a8ad-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_042700-issue-bc45be2f32a3-build-wait-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_042700-issue-bc45be2f32a3-build-wait-review.md' |
| `docs/changelog/2026-11-22/20261122_043000-cpp-latency-cache-stability-and-build-measurement.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_043000-cpp-latency-cache-stability-and-build-measurement.md' |
| `docs/changelog/2026-11-22/20261122_043000-issue-49ded3edbe47-independent-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-22/20261122_043000-issue-49ded3edbe47-independent-review.md' |
| `docs/issues/build-friction/20261122-exe-build-setup-runs-a-failing-presentation-dll--issue-ae248907f1f1.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-exe-build-setup-runs-a-failing-presentation-dll--issue-ae248907f1f1.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-4d9eb4b1a8ad.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-4d9eb4b1a8ad.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-f92715ba3c7f.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-f92715ba3c7f.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-0968dd48e9d8.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-0968dd48e9d8.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-266f4fcaf861.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-266f4fcaf861.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2b6479feedc1.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2b6479feedc1.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-60f01796d952.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-60f01796d952.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6202f0e8d9f4.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6202f0e8d9f4.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-628187de3d6d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-628187de3d6d.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bc45be2f32a3.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bc45be2f32a3.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ce4c245a8a70.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ce4c245a8a70.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d2466e463a18.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d2466e463a18.md' |
| `docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-f534b31a9a1a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-f534b31a9a1a.md' |

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
- Focus: Fun Counter Strike NPC navigation and objective behavior
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / 9fd637d595a308483a9f82c88349a5915c82a0ca / 9fd637d595a308483a9f82c88349a5915c82a0ca
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-22T02:37:06.972018Z", "latest_successful_build_id": "2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344", "latest_successful_source_generation": 1}, "game_current": {"build_mode": "debug", "code_hash": "02be857864157c6c542ed6b1e89e0f84f1b36e3a55f5713c006c43ce6a711583", "dll": "build\\game\\generation-455\\game-g455.dll", "generation": 455, "revision": "25913fdc429a4945f86a31ccad4b0785635f6f4cd7a63ab35603b16dc972f8c3", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-455\\game-g455.dll", "generation": 455, "published_utc": "2026-11-22T04:30:17.216Z", "revision": "25913fdc429a4945f86a31ccad4b0785635f6f4cd7a63ab35603b16dc972f8c3", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 43.014, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-22T04:04:00.047Z"}}
- Generation: 455
- Artifact: build\game\generation-455\game-g455.dll
- Journal path: C:\mimita-v9\logs\2026-11-22\20261122_042716_fcs-route-proof\server-01\events.jsonl
- Session ID: session-767d0984527b

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
 M config/hot-reload/packet-ledger.json
 m devscripts/blender-ai
 M devscripts/game_build_worker.py
 M devscripts/quality/test_game_build_snapshot.py
 M docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md
 M docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md
 M docs/issues/README.md
 M docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md
 M docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md
 M docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
 M docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
 M docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
 M docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md
 M src/gui/ui-system-buttons.cpp
 M src/gui/ui-system-internal.h
 M src/gui/ui-system.cpp
 M src/gui/ui-text-input.cpp
 M src/main-systems.cpp
 M src/network/multiplayer-tick.cpp
 M src/npc/npc-behavior-module.cpp
 M src/npc/npc.cpp
?? docs/changelog/2026-11-22/20261122_032700-fcs-npc-events-and-plant-proof-review.md
?? docs/changelog/2026-11-22/20261122_035016-issue-ec19949cbd21-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_035017-issue-8aae0621645f-exe-build-setup-wait-review.md
?? docs/changelog/2026-11-22/20261122_035023-cpp-latency-pressure-profile-fcs-npc-evidence-review.md
?? docs/changelog/2026-11-22/20261122_035200-ctx-1f0fb5aa9253-zero-cpu-reading-review.md
?? docs/changelog/2026-11-22/20261122_035600-issue-a59a0058df76-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_040332-issue-f534b31a9a1a-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_040900-issue-0968dd48e9d8-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_041142-issue-75220121d993-independent-review.md
?? docs/changelog/2026-11-22/20261122_041300-issue-f92715ba3c7f-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_042300-fcs-network-and-console-ui-followup.md
?? docs/changelog/2026-11-22/20261122_042500-issue-4d9eb4b1a8ad-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_042700-issue-bc45be2f32a3-build-wait-review.md
?? docs/changelog/2026-11-22/20261122_043000-cpp-latency-cache-stability-and-build-measurement.md
?? docs/changelog/2026-11-22/20261122_043000-issue-49ded3edbe47-independent-review.md
?? docs/issues/build-friction/20261122-exe-build-setup-runs-a-failing-presentation-dll--issue-ae248907f1f1.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-compiling-issue-4d9eb4b1a8ad.md
?? docs/issues/build-friction/20261122-wait-over-budget-exe-build-queue-at-build-pendin-issue-f92715ba3c7f.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-0968dd48e9d8.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-266f4fcaf861.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-2b6479feedc1.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-60f01796d952.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6202f0e8d9f4.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-628187de3d6d.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-bc45be2f32a3.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ce4c245a8a70.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-d2466e463a18.md
?? docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-f534b31a9a1a.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   2 +-
 config/analytics.json                              |   2 +-
 config/hot-reload/packet-ledger.json               | 160 ++++++++++-----------
 devscripts/blender-ai                              |   0
 devscripts/game_build_worker.py                    | 144 +++++++++++++++----
 devscripts/quality/test_game_build_snapshot.py     |  28 ++++
 ...ld-wait-reviewer-fcs-npc-kill-plant-evidence.md |  43 ++++--
 ...-autonomous-gameplay-investigation-and-proof.md |   6 +
 docs/issues/README.md                              |  33 +++--
 ...ntology-for-project-and-a-issue-75220121d993.md |  40 ++++++
 ...objective-behavior-regres-issue-be1adb51b6ac.md |  56 ++++++++
 ...move-avoidable-build-and--issue-e044a931c54b.md |   6 +
 ...budget-exe-build-at-setup-issue-8aae0621645f.md |  50 +++++++
 ...e-dll-build-at-game-build-issue-6bcfb6252dfa.md |  25 ++++
 ...e-dll-build-at-game-build-issue-a59a0058df76.md |  28 ++++
 ...e-dll-build-at-game-build-issue-ec19949cbd21.md |  45 ++++++
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  14 ++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  57 ++++++++
 ...ompleteness-and-readiness-issue-3100aa7b5807.md |  10 ++
 ...uild-stage-and-investigat-issue-136d4553ff92.md |  44 +++++-
 ...ta-cpu-use-during-idle-an-issue-ed3ac775283d.md |   6 +
 ...te-special-contact-paths--issue-49ded3edbe47.md |  39 +++++
 src/gui/ui-system-buttons.cpp                      |   4 +
 src/gui/ui-system-internal.h                       |   1 +
 src/gui/ui-system.cpp                              |  38 ++++-
 src/gui/ui-text-input.cpp                          |   2 +
 src/main-systems.cpp                               |   8 +-
 src/network/multiplayer-tick.cpp                   |   8 +-
 src/npc/npc-behavior-module.cpp                    |   2 +
 src/npc/npc.cpp                                    |  54 +++++--
 30 files changed, 807 insertions(+), 148 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/hot-reload/packet-ledger.json
M	devscripts/blender-ai
M	devscripts/game_build_worker.py
M	devscripts/quality/test_game_build_snapshot.py
M	docs/changelog/2026-11-22/20261122_033000-build-wait-reviewer-fcs-npc-kill-plant-evidence.md
M	docs/gold/2026-11-21-autonomous-gameplay-investigation-and-proof.md
M	docs/issues/README.md
M	docs/issues/architecture/20261120-nodes-and-connections-ontology-for-project-and-a-issue-75220121d993.md
M	docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/build-friction/20261122-wait-over-budget-exe-build-at-setup-issue-8aae0621645f.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-6bcfb6252dfa.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-a59a0058df76.md
M	docs/issues/build-friction/20261122-wait-over-budget-game-dll-build-at-game-build-issue-ec19949cbd21.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-issue-context-completeness-and-readiness-issue-3100aa7b5807.md
M	docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
M	docs/issues/improvement/20261121-measure-and-reduce-mimita-cpu-use-during-idle-an-issue-ed3ac775283d.md
M	docs/issues/tech-debt/20261120-tech-debt-audit-duplicate-special-contact-paths--issue-49ded3edbe47.md
M	src/gui/ui-system-buttons.cpp
M	src/gui/ui-system-internal.h
M	src/gui/ui-system.cpp
M	src/gui/ui-text-input.cpp
M	src/main-systems.cpp
M	src/network/multiplayer-tick.cpp
M	src/npc/npc-behavior-module.cpp
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-22T02:37:06.972018Z",
    "latest_successful_build_id": "2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344",
    "latest_successful_source_generation": 1
  },
  "game_current": {
    "build_mode": "debug",
    "code_hash": "02be857864157c6c542ed6b1e89e0f84f1b36e3a55f5713c006c43ce6a711583",
    "dll": "build\\game\\generation-455\\game-g455.dll",
    "generation": 455,
    "revision": "25913fdc429a4945f86a31ccad4b0785635f6f4cd7a63ab35603b16dc972f8c3",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-455\\game-g455.dll",
    "generation": 455,
    "published_utc": "2026-11-22T04:30:17.216Z",
    "revision": "25913fdc429a4945f86a31ccad4b0785635f6f4cd7a63ab35603b16dc972f8c3",
    "status": "ready"
  },
  "progress": {
    "compiled": 0,
    "elapsed_s": 43.014,
    "phase": "build_complete",
    "result": "noop",
    "skipped": 87,
    "updated_utc": "2026-11-22T04:04:00.047Z"
  }
}
```
