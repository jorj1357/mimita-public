// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Dashboard Process Refresh Fast Path

# Task

- Task ID: session-3a4a68ed8da5
- Session ID: session-3a4a68ed8da5
- Owner: dashboard-process-buttons-and-ai-autonomy
- Focus: issue-7ee67a85a40c; issue-35fd6df3d947; issue-6fc8f853a674
- Summary: Reduce stale process-detail refreshes by using selective psutil enumeration when available; capture remaining cross-interpreter and UI proof gaps.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-21T01:29:08+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

# Pre-existing changes

- The checkout already had numerous unrelated staged/unstaged changes before this work; the generated exact status and diff summary below records the snapshot. I edited only `devscripts/dev-loop.py` for this task, plus this changelog; issue records were updated through agent-coord.

# Requested behavior

- User requested an issue for the Process detail Refresh error and stale process rows, working Start client/Start server/Stop/Restart/Refresh buttons, safe dashboard restart without stopping dev-loop/build, and repeated AI issue review/evidence gathering. They also requested automatic low-FPS tracing below 240 FPS.
- Screenshot evidence: process scan failed with PowerShell `Get-CimInstance Win32_Process`, stale rows remained visible, and Refresh was available.
- Existing issue `issue-7ee67a85a40c` covers all-button automated verification and already records this Refresh symptom. Related current evidence was added to it; `issue-35fd6df3d947` tracks inventory correctness, `issue-6fc8f853a674` tracks safe restart/recovery, `issue-70cbf4d13c02` tracks shared issue updates, and `issue-91e3384b219b` tracks continuous agent awareness/progress. Existing low-FPS tracing issue `issue-13d080318e1c` captures the requested below-240 FPS automation.

# Specification alignment

- Current specification paths: `docs/specs/dev-loop/dashboard.md`, `docs/specs/dev-loop/multi-instance.md`, `docs/specs/dev-loop/agent-coordination.md`, `docs/workflows/issue-intake-and-triage.md`.
- Exact requirements: Refresh starts an immediate scan; on scan error the UI retains the last known list and marks it stale; successful scans update/remove exited processes; errors remain visible and must not become a false empty list.
- Why the change follows the specification: scanner discovery now uses selective process metadata for MiMITA candidates when psutil is installed, reducing dependence on a broad PowerShell CIM query while preserving the existing output schema and GUI stale-on-error behavior.
- Conflicts or decisions: Python 3.14 currently lacks psutil and therefore still uses the WMI fallback. Full cross-interpreter coverage and UI button proof remain open.

# Exact implementation changes

## File: `devscripts/dev-loop.py`

- Lines/functions/headings: `checkout_mimita_processes`, line 800.
- Old content or behavior: a PowerShell `Get-CimInstance Win32_Process` command scanned process records and could exceed its 8-second timeout, yielding `PROCESS SCAN FAILED` and stale GUI rows.
- New content or behavior: when psutil is available, enumerate names/PIDs first and request executable, command line, and start time only for exact `mimita.exe` candidates under the checkout. Keep the old WMI fallback when psutil is unavailable and retain the strict error behavior.
- Reason: direct source invocation reproduced the timeout; selective psutil enumeration returned the matching MiMITA process records in approximately 1.1–1.6 seconds in observed runs.
- Why unrelated behavior is preserved: same process record fields and strict/non-strict interface; no button command handlers or unrelated process lifecycle code changed.

## Runtime and validation evidence

- Source-level direct invocation before the patch reproduced `subprocess.TimeoutExpired` at 8 seconds. A narrow CIM query completed in about 2.3 seconds, indicating load-sensitive latency.
- After the patch, direct `checkout_mimita_processes(strict=True)` returned 7 records in about 1.63 seconds, and after dashboard restart returned 11 records in about 1.14 seconds.
- Restarted only the dashboard: old PID 28468 was replaced by 36288. Watchdog 23328, daemon 28216, server 33940, client 24476, and run `20261120_232200` remained unchanged. No build was active, so preservation during an active build is not proven.
- `--ensure-dashboard` initially printed `pid=0` even though the new dashboard process and lock later showed PID 36288. This evidence was appended to the safe-restart issue.
- CUA desktop inspection timed out after the documented retry/reset, so I could not click Refresh or visually verify button behavior. Start/Stop/Restart button behavior and all-button automated acceptance remain open under `issue-7ee67a85a40c`.
- No build was requested. The running game still uses the previously published build; source changes are not build/runtime-proven.

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
| `devscripts/dev_loop/parallel_test_runs.py` |  |  |  | git checkout -- devscripts/dev_loop/parallel_test_runs.py |
| `devscripts/game_build_worker.py` |  |  |  | git checkout -- devscripts/game_build_worker.py |
| `devscripts/quality/test_game_build_snapshot.py` |  |  |  | git checkout -- devscripts/quality/test_game_build_snapshot.py |
| `docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md` |  |  |  | git checkout -- docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md |
| `docs/issues/README.md` |  |  |  | git checkout -- docs/issues/README.md |
| `docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md` |  |  |  | git checkout -- docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md |
| `docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md` |  |  |  | git checkout -- docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md |
| `docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md` |  |  |  | git checkout -- docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md` |  |  |  | git checkout -- docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md |
| `docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md` |  |  |  | git checkout -- docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md |
| `docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md |
| `docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md |
| `docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md |
| `docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` |  |  |  | git checkout -- docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md |
| `docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md` |  |  |  | git checkout -- docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md |
| `docs/specs/dev-loop/agent-build-queue.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-build-queue.md |
| `docs/specs/dev-loop/agent-context-contract.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-context-contract.md |
| `docs/specs/dev-loop/agent-coordination.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-coordination.md |
| `docs/specs/dev-loop/agent-self-improvement.md` |  |  |  | git checkout -- docs/specs/dev-loop/agent-self-improvement.md |
| `docs/specs/dev-loop/build-time-observability.md` |  |  |  | git checkout -- docs/specs/dev-loop/build-time-observability.md |
| `docs/workflows/issue-intake-and-triage.md` |  |  |  | git checkout -- docs/workflows/issue-intake-and-triage.md |
| `src/network/multiplayer-projectiles.cpp` |  |  |  | git checkout -- src/network/multiplayer-projectiles.cpp |
| `src/network/schema/packet-combat.h` |  |  |  | git checkout -- src/network/schema/packet-combat.h |
| `src/network/server-projectiles.cpp` |  |  |  | git checkout -- src/network/server-projectiles.cpp |
| `src/npc/npc.cpp` |  |  |  | git checkout -- src/npc/npc.cpp |
| `devscripts/quality/test_build_coordinator.py` |  |  |  | Remove-Item -LiteralPath 'devscripts/quality/test_build_coordinator.py' |
| `docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md' |
| `docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md' |
| `docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md' |
| `docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md' |
| `docs/changelog/2026-11-21/` |  |  |  | Remove-Item -LiteralPath 'docs/changelog/2026-11-21/' |
| `docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md' |
| `docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md' |
| `docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md' |
| `docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md' |
| `docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md' |
| `docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md' |
| `docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md' |
| `docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md' |
| `docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md' |
| `docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md' |
| `docs/issues/build-friction/20261121-windows-isolated-test-runs-abort-because-process-issue-f0fdd607b477.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/build-friction/20261121-windows-isolated-test-runs-abort-because-process-issue-f0fdd607b477.md' |
| `docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md' |
| `docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md' |
| `docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md' |
| `docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md` |  |  |  | Remove-Item -LiteralPath 'docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md' |

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

- Owner: dashboard-process-buttons-and-ai-autonomy
- Focus: issue-7ee67a85a40c; issue-35fd6df3d947; issue-6fc8f853a674
- Files to check:
- What another agent should not redo:
- Next exact step:

# Automatic evidence

- Branch/base/final: checkpoint/one-checkout-condensation-20261111 / d651e1332fccff70bc5cc4601dc24d55784820b5 / d651e1332fccff70bc5cc4601dc24d55784820b5
- Git status: captured below
- Diff stat: captured below
- Build identity: {"dev_loop": {"build_authority": "dev-loop", "build_status": "published_stale", "latest_build_stale": true, "latest_successful_build": "2026-11-20T20:20:51.788061Z", "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55", "latest_successful_source_generation": 38}, "game_current": {"build_mode": "release", "code_hash": "95fb913f80de54772285da4373d367b47dfbcf882c1d2636f52ff692b791a644", "dll": "build\\game\\generation-417\\game-g417.dll", "generation": 417, "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff", "status": "ok"}, "game_ready": {"dll": "build\\game\\generation-417\\game-g417.dll", "generation": 417, "published_utc": "2026-11-21T00:11:58.032Z", "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff", "status": "ready"}, "progress": {"compiled": 0, "elapsed_s": 31.195, "phase": "build_complete", "result": "noop", "skipped": 87, "updated_utc": "2026-11-20T21:40:39.596Z"}}
- Generation: 417
- Artifact: build\game\generation-417\game-g417.dll
- Journal path: C:\mimita-v9\logs\2026-11-21\20261121_010023\client-01\events.jsonl
- Session ID: session-3a4a68ed8da5

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
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/game_build_worker.py
 M devscripts/quality/test_game_build_snapshot.py
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md
 M docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
 M docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
 M docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
 M docs/specs/dev-loop/agent-build-queue.md
 M docs/specs/dev-loop/agent-context-contract.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/workflows/issue-intake-and-triage.md
 M src/network/multiplayer-projectiles.cpp
 M src/network/schema/packet-combat.h
 M src/network/server-projectiles.cpp
 M src/npc/npc.cpp
?? devscripts/quality/test_build_coordinator.py
?? docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md
?? docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md
?? docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md
?? docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md
?? docs/changelog/2026-11-21/
?? docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
?? docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
?? docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md
?? docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md
?? docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md
?? docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
?? docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md
?? docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md
?? docs/issues/build-friction/20261121-windows-isolated-test-runs-abort-because-process-issue-f0fdd607b477.md
?? docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
?? docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md
?? docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md
?? docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   6 +-
 config/analytics.json                              |   2 +-
 config/audio/music-settings.json                   |   2 +-
 config/behavior-profiles.json                      |  23 ++-
 config/movement/movement-heavy.json                |   3 +-
 config/movement/movement-source.json               |   3 +-
 config/video-settings.json                         |   2 +-
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             | 154 +++++++++++++++++++--
 devscripts/dev_loop/game_build.py                  |  21 ++-
 devscripts/dev_loop/parallel_test_runs.py          |  56 ++++++--
 devscripts/game_build_worker.py                    |  93 ++++++++++++-
 devscripts/quality/test_game_build_snapshot.py     |  30 +++-
 ...e-issues-preview-overlay-first-frame-context.md |  30 +++-
 docs/issues/README.md                              |  35 +++--
 ...ventory-can-omit-or-miscl-issue-35fd6df3d947.md |  20 +++
 ...cy-no-frame-below-240-fps-issue-549680e53b26.md |   6 +
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md |  35 +++++
 ...e-dll-build-at-game-build-issue-0322fa45b0e8.md |  18 +++
 ...ative-editing-and-credite-issue-421c5be1669f.md |   6 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md |  62 +++++++++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  12 ++
 ...le-restart-and-self-recov-issue-6fc8f853a674.md |  18 +++
 ...ner-preview-overlays-and--issue-06c8cd76dc9d.md |  26 ++++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  40 ++++++
 .../counterstrike-npc-targeting-REG-2026-10-04.md  |  27 ++++
 docs/specs/dev-loop/agent-build-queue.md           |  11 ++
 docs/specs/dev-loop/agent-context-contract.md      |  12 ++
 docs/specs/dev-loop/agent-coordination.md          |  23 +++
 docs/specs/dev-loop/agent-self-improvement.md      |  36 +++++
 docs/specs/dev-loop/build-time-observability.md    |  22 +++
 docs/workflows/issue-intake-and-triage.md          |  16 +++
 src/network/multiplayer-projectiles.cpp            |  15 +-
 src/network/schema/packet-combat.h                 |   2 +-
 src/network/server-projectiles.cpp                 |   5 +
 src/npc/npc.cpp                                    |  11 +-
 36 files changed, 807 insertions(+), 76 deletions(-)
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
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/game_build_worker.py
M	devscripts/quality/test_game_build_snapshot.py
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md
M	docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
M	docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
M	docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
M	docs/specs/dev-loop/agent-build-queue.md
M	docs/specs/dev-loop/agent-context-contract.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/workflows/issue-intake-and-triage.md
M	src/network/multiplayer-projectiles.cpp
M	src/network/schema/packet-combat.h
M	src/network/server-projectiles.cpp
M	src/npc/npc.cpp
```

## Captured build identity

```json
{
  "dev_loop": {
    "build_authority": "dev-loop",
    "build_status": "published_stale",
    "latest_build_stale": true,
    "latest_successful_build": "2026-11-20T20:20:51.788061Z",
    "latest_successful_build_id": "2026-11-20T20:20:51.788061Z:64cd9304582622039486e6a1fe369e2f7d466a2dc0eac7975241f98f0ee0df55",
    "latest_successful_source_generation": 38
  },
  "game_current": {
    "build_mode": "release",
    "code_hash": "95fb913f80de54772285da4373d367b47dfbcf882c1d2636f52ff692b791a644",
    "dll": "build\\game\\generation-417\\game-g417.dll",
    "generation": 417,
    "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-417\\game-g417.dll",
    "generation": 417,
    "published_utc": "2026-11-21T00:11:58.032Z",
    "revision": "6159737d84d4818b288feefa86a39447cab164b6c5b24b19a269d45a262871ff",
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
