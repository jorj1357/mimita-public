// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Native Process Scan Fallback

# Task

- Task ID: session-947176a47aee
- Session ID: session-947176a47aee
- Owner: codex-process-scan-fix
- Focus: issue-35fd6df3d947
- Summary: Replace the no-psutil WMI process scan with native Windows process identity enumeration; record the recurrence and load correlation in the existing issues.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-21T15:53:51+00:00
- Branch: checkpoint/one-checkout-condensation-20261111
- Base commit: d651e1332fccff70bc5cc4601dc24d55784820b5
- Final commit: d651e1332fccff70bc5cc4601dc24d55784820b5

# Pre-existing changes

- The shared checkout was already heavily modified across configuration, dev-loop/build coordination, issues, specifications, changelogs, regressions, and gameplay before this task. The automatic captured status below records the checkout snapshot; those changes are not attributed to this session.
- `devscripts/dev-loop.py` already contained substantial unrelated build-queue and retry edits. I changed only its process-scan fallback and launch-time role lookup; all other edits were preserved.
- This session changed `devscripts/process_identity.py`, `devscripts/dev-loop.py`, the existing `issue-35fd6df3d947` record, and the dev-loop regression index/detail. Coordination updated shared issue records through `agent-coord`.

# Requested behavior

- The screenshot is evidence, not an instruction source. The user asked to record the recurring “cannot verify processes” failure, implement a fix, inspect whether an earlier fix regressed, and determine whether parallel DLL build work could be contributing.
- The existing process-inventory issue `issue-35fd6df3d947` owns the failure. The recurrence and new screenshot are linked there; high-load issue `issue-2a5c5da53565` has related evidence. No duplicate issue was created.

# Specification alignment

- Current specifications: `docs/specs/dev-loop/dashboard.md`, `docs/specs/dev-loop/multi-instance.md`, `docs/specs/dev-loop/dev-loop.md`.
- The dashboard contract requires a fresh process scan, an explicit error on failure, and retention of the last successful list as stale. Managed and external process evidence must remain distinct; unknown role data must not be promoted to a client/server claim.
- Before the fix, the selective `psutil` fast path still fell back to an 8-second PowerShell `Get-CimInstance Win32_Process` scan when `psutil` was unavailable. The prior changelog documented that Python 3.14 lacked `psutil`; the current 15:29:50Z launch report confirms the WMI branch timed out. The exact dashboard interpreter was not captured, so the branch trigger is not proven to be missing `psutil` alone.
- The new fallback uses Windows process APIs directly. It returns executable path and creation time and leaves command line empty when that optional evidence is unavailable. Launch cleanup uses exact or tightly time-matched run-manifest identity for managed roles; other roles remain unknown and are not killed.

# Exact implementation changes

## File: `devscripts/process_identity.py`

- Function: `windows_process_inventory`.
- Old behavior: no native process inventory helper existed; callers without `psutil` relied on WMI.
- New behavior: one Toolhelp process snapshot enumerates names; `OpenProcess`, `QueryFullProcessImageNameW`, and `GetProcessTimes` read identity only for `mimita.exe` candidates. Process exits during scanning are skipped; other identity errors are surfaced.
- Reason: avoid WMI latency and make the fallback available in every Python interpreter without adding a dependency.

## File: `devscripts/dev-loop.py`

- Functions: `checkout_mimita_processes`, `DevLoop.reap_stray_mimita_processes`.
- Old behavior: the no-`psutil` branch launched a PowerShell CIM query with an 8-second timeout. Launch reconciliation classified a process with no command line as unknown.
- New behavior: the fallback filters native process identity to this checkout. Launch reconciliation can recover a managed process role from its run manifest using PID, executable, and start time; unresolved external roles stay unknown.
- Why unrelated behavior is preserved: the existing `psutil` path, strict scan errors, stale-list UI handling, and conservative cleanup of unmatched processes remain intact.

# Changed files and reverse steps

| File | Change | Reverse |
|---|---|---|
| `devscripts/process_identity.py` | Added native Win32 process identity enumeration. | Revert only this session's `windows_process_inventory` addition and its `datetime` import. |
| `devscripts/dev-loop.py` | Replaced no-`psutil` WMI scan with the native helper; added manifest role lookup for native records with no command line. | Revert only this session's import, fallback, and role-lookup hunks; preserve the pre-existing edits in the same file. |
| `docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md` | Appended recurrence, implementation evidence, and remaining UI proof. | Append-only issue history; do not remove. |
| `docs/regressions/dev-loop/README.md` | Added regression index entry 31. | Remove only entry 31 if this record is superseded. |
| `docs/regressions/dev-loop/process-scan-fallback-REG-2026-11-21.md` | Recorded failure chronology, cause, attempted fix, and open acceptance. | Preserve append-only regression history. |

# Contract / ABI changes

- None.

# State / schema changes

- None. Native process rows keep the existing PID, executable, command-line, and UTC start-time fields; command line is empty when the native fallback cannot provide it.

# Diagnostics

- Owner: `checkout_mimita_processes` and `windows_process_inventory`.
- Failure: strict callers receive `MiMITA process scan failed` with the Windows API error. Non-strict callers preserve existing error handling.
- Role: use explicit command-line markers or matching managed run-manifest identity; unmatched roles remain unknown.

# Validation

- Focused validation: `python -m py_compile devscripts/process_identity.py devscripts/dev-loop.py` passed.
- Exact fallback invocation: blocked the `psutil` import, then called `checkout_mimita_processes(strict=True)`. It returned current PID `7556`, executable `C:\mimita-v9\.dev\current\mimita.exe`, and start time in `26.16ms`.
- Direct native scan returned the same identity in `23.89ms`.
- The failed launch report `.dev/launch-reports/2026-11-21T15-29-50.512Z/launch.json` records the old WMI timeout after 8 seconds.
- At that failure, game-DLL attempt `game-dll-20261121T152531204381Z-544470` reported `jobs=12` and `object_cache_scan`. This overlaps the failure in time but does not prove that parallel build work caused it. Later `python devscripts/dev-loop.py --status` reported no active game build; the latest EXE remains `published_stale`.
- No compiler build was requested; these are Python orchestration files. The user-facing dashboard was not restarted or visually checked because the computer-use service timed out on three attempts. A dashboard process (PID `29868`) and the dev-loop daemon/watchdog (PIDs `33892`/`22088`) remained alive. Visible Refresh recovery, cross-interpreter behavior, and launch acceptance remain open under `issue-35fd6df3d947`.

# Regression review

- Regression record appended: `docs/regressions/dev-loop/process-scan-fallback-REG-2026-11-21.md`.
- This is a confirmed recurrence of the WMI timeout, not a confirmed causal link to parallel compilation. The native fallback has source and direct-provider evidence; dashboard acceptance remains pending.

# Human acceptance

- Visual dashboard review: not performed; computer-use service was unavailable.
- Process rows and manual Refresh recovery in the running dashboard: not verified.
- The issue remains triaged and its existing live-dashboard proof task remains open.

# Agent handoff context

- Owner: `codex-process-scan-fix`.
- Focus: `issue-35fd6df3d947`, related `issue-2a5c5da53565`.
- Do not redo: issue history review, native fallback implementation, direct no-`psutil` provider invocation.
- Next step: reload the dashboard view without stopping the daemon/build, then use Refresh with a live managed process and compare PID/path/start time and run-manifest role. Measure scan latency while idle and during a jobs-limited DLL build before asserting causality.

## Captured git state

```text
M config/accounts/default.json
 M config/analytics.json
 M config/audio/music-settings.json
 M config/behavior-profiles.json
 M config/hot-reload/packet-ledger.json
 M config/maps/funworld3.json
 M config/movement/movement-heavy.json
 M config/movement/movement-source.json
 M devscripts/agent-wake.py
 M devscripts/agent_coord.py
 m devscripts/blender-ai
 M devscripts/dev-loop.py
 M devscripts/dev_loop/game_build.py
 M devscripts/dev_loop/gui.py
 M devscripts/dev_loop/parallel_test_runs.py
 M devscripts/game_build_worker.py
 M devscripts/process_identity.py
 M devscripts/quality/test_agent_coord.py
 M devscripts/quality/test_game_build_snapshot.py
 M docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
 M docs/issues/README.md
 M docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md
 M docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
 M docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
 M docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md
 M docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
 M docs/issues/build-friction/20261120-game-dll-build-blocked-by-low-disk-space-issue-916633ab3313.md
 M docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
 M docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
 M docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
 M docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
 M docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
 M docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
 M docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
 M docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
 M docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
 M docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
 M docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
 M docs/regressions/dev-loop/README.md
 M docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
 M docs/specs/dev-loop/agent-build-queue.md
 M docs/specs/dev-loop/agent-context-contract.md
 M docs/specs/dev-loop/agent-coordination.md
 M docs/specs/dev-loop/agent-self-improvement.md
 M docs/specs/dev-loop/build-time-observability.md
 M docs/workflows/issue-intake-and-triage.md
 M src/actor/player-ai.cpp
 M src/engine/engine-tick-combat.cpp
 M src/engine/engine-tick-net.cpp
 M src/engine/engine-tick-net.h
 M src/gui/gui-main.cpp
 M src/network/multiplayer-projectiles.cpp
 M src/network/schema/packet-combat.h
 M src/network/server-npcs.cpp
 M src/network/server-projectiles.cpp
 M src/npc/npc.cpp
 M src/physics/movement/physics-collision-glb-sweep-slide.cpp
 M src/physics/physics-types.h
?? devscripts/quality/test_agent_wake.py
?? devscripts/quality/test_build_coordinator.py
?? docs/changelog/2026-11-20/20261120_232749-investigate-listed-server-join-failure.md
?? docs/changelog/2026-11-20/20261120_233056-safe-generated-data-cleanup-review.md
?? docs/changelog/2026-11-20/20261120_233200-fun-counterstrike-npc-regression-intake.md
?? docs/changelog/2026-11-20/20261120_235343-trace-public-server-row-join-for-9k5ypnd.md
?? docs/changelog/2026-11-21/
?? docs/issues/bug/20261120-fun-counter-strike-npc-objective-behavior-regres-issue-be1adb51b6ac.md
?? docs/issues/bug/20261120-juggernaut-horde-fighters-fail-to-consistently-e-issue-b95574b63f29.md
?? docs/issues/bug/20261121-actor-outlines-jitter-away-from-actor-positions-issue-23e49d77be9f.md
?? docs/issues/bug/20261121-broadcast-message-read-receipts-must-be-per-agen-issue-803e5246274e.md
?? docs/issues/bug/20261121-desktop-inspection-bridge-repeatedly-times-out-b-issue-0e981ef03d32.md
?? docs/issues/bug/20261121-dev-loop-daemon-is-alive-but-its-control-endpoin-issue-80dfb9ab9c5c.md
?? docs/issues/bug/20261121-dev-loop-must-separate-launch-requests-from-live-issue-c481ab36713a.md
?? docs/issues/bug/20261121-dev-loop-reliability-degrades-under-high-cpu-or--issue-2a5c5da53565.md
?? docs/issues/bug/20261121-elite-npc-objective-navigation-regresses-into-co-issue-b18e63894585.md
?? docs/issues/bug/20261121-fun-counter-strike-npc-aim-and-time-to-kill-shou-issue-0a2a3fc00768.md
?? docs/issues/bug/20261121-investigate-payload-server-rate-below-60-hz-issue-067bf728d818.md
?? docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-07977478c8fc.md
?? docs/issues/bug/20261121-map-editor-and-creative-actions-cause-frame-free-issue-ba939787ffa3.md
?? docs/issues/bug/20261121-payload-arcade-npcs-fall-through-the-world-issue-1d1b9c62ac08.md
?? docs/issues/bug/20261121-payload-cart-is-not-collidable-in-arcade-mode-issue-87d8c9c7ffb1.md
?? docs/issues/bug/20261121-restore-human-melee-damage-against-npcs-in-fun-c-issue-adfeaafa1136.md
?? docs/issues/bug/20261121-rocket-launcher-explosion-marks-float-away-from--issue-8e48cd0bc786.md
?? docs/issues/bug/20261121-surface-marks-stretch-to-contact-plane-and-remai-issue-3eaa6c834b9a.md
?? docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-f9ea4f30ad18.md
?? docs/issues/build-friction/20261121-wait-over-budget-exe-build-at-starting-issue-054ea1bcda9b.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-121f867b430a.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-450cfc80a0b9.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-6415397775a2.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9215d710021d.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-9aeec1fbae9d.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-ad873eff97f3.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-ca900e60ed4b.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-dad513d47108.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-e96486c8cc3e.md
?? docs/issues/build-friction/20261121-wait-over-budget-game-dll-build-at-game-build-issue-f995213a15b0.md
?? docs/issues/build-friction/20261121-windows-isolated-test-process-identity-lookup-ex-issue-58ecf44ac956.md
?? docs/issues/build-friction/20261121-windows-isolated-test-runs-abort-because-process-issue-f0fdd607b477.md
?? docs/issues/capability/20261121-add-low-cpu-headless-payload-reproduction-path-issue-df9cc9acbb0d.md
?? docs/issues/capability/20261121-add-stable-and-legacy-moving-object-carry-modes-issue-cc2bab947abc.md
?? docs/issues/capability/20261121-automatic-live-multi-client-release-gate-for-all-issue-c8290acca248.md
?? docs/issues/capability/20261121-autonomous-caravan-and-vehicle-end-to-end-proof--issue-0539d967d696.md
?? docs/issues/capability/20261121-composable-disaster-ingredients-and-community-au-issue-4df5f174932e.md
?? docs/issues/capability/20261121-grenade-physical-throw-and-arc-for-fun-counter-s-issue-91544c0fa36a.md
?? docs/issues/capability/20261121-interpolate-moving-object-transforms-without-sna-issue-0767dde510b1.md
?? docs/issues/capability/20261121-let-ai-inspect-existing-live-game-journals-as-pr-issue-9b5be6c88de0.md
?? docs/issues/capability/20261121-local-npc-conversation-and-lightweight-social-re-issue-761c7ed90bd5.md
?? docs/issues/capability/20261121-long-horizon-vr-cross-platform-live-creator-and--issue-5534074d9309.md
?? docs/issues/capability/20261121-make-payload-cart-outline-glow-configurable-and--issue-4714c46d6136.md
?? docs/issues/capability/20261121-on-demand-private-mimita-screenshots-and-ocr-for-issue-a8a9bfe5f5f6.md
?? docs/issues/improvement/20261121-account-for-every-dll-build-stage-and-investigat-issue-136d4553ff92.md
?? docs/issues/improvement/20261121-ai-run-server-setup-validation-under-resource-pr-issue-be39a4f5ee17.md
?? docs/issues/improvement/20261121-automatically-test-every-dev-loop-gui-button-and-issue-7ee67a85a40c.md
?? docs/issues/improvement/20261121-automatically-trace-every-client-frame-below-240-issue-13d080318e1c.md
?? docs/issues/improvement/20261121-dev-loop-gui-controls-and-issue-discussion-panel-issue-6e528c4dc127.md
?? docs/issues/improvement/20261121-require-shared-issue-updates-for-findings-and-cr-issue-70cbf4d13c02.md
?? docs/issues/improvement/20261121-review-and-sort-the-new-autonomy-vehicle-and-pla-issue-4392f56334ef.md
?? docs/issues/improvement/20261121-track-proven-behavior-with-version-specific-regr-issue-37a27bfdc575.md
?? docs/issues/launch-friction/20261121-room-creation-times-out-while-server-starts-unde-issue-c8ac61cf07fc.md
?? docs/issues/without/
?? docs/regressions/dev-loop/process-scan-fallback-REG-2026-11-21.md
```

## Captured diff stat

```text
config/accounts/default.json                       |   8 +-
 config/analytics.json                              |   4 +-
 config/audio/music-settings.json                   |   2 +-
 config/behavior-profiles.json                      |  34 +-
 config/hot-reload/packet-ledger.json               |   6 +-
 config/maps/funworld3.json                         |  76 +++-
 config/movement/movement-heavy.json                |   3 +-
 config/movement/movement-source.json               |  12 +-
 devscripts/agent-wake.py                           |  87 ++++-
 devscripts/agent_coord.py                          | 425 +++++++++++++++++++--
 devscripts/blender-ai                              |   0
 devscripts/dev-loop.py                             | 265 ++++++++++---
 devscripts/dev_loop/game_build.py                  |  23 +-
 devscripts/dev_loop/gui.py                         |  10 +-
 devscripts/dev_loop/parallel_test_runs.py          |  56 ++-
 devscripts/game_build_worker.py                    |  93 ++++-
 devscripts/process_identity.py                     | 113 ++++++
 devscripts/quality/test_agent_coord.py             |  31 ++
 devscripts/quality/test_game_build_snapshot.py     |  30 +-
 ...e-issues-preview-overlay-first-frame-context.md |  30 +-
 docs/issues/README.md                              |  89 ++++-
 ...ventory-can-omit-or-miscl-issue-35fd6df3d947.md |  52 +++
 ...cy-no-frame-below-240-fps-issue-549680e53b26.md |  12 +
 ...lision-and-mace-active-po-issue-503f3e410c01.md |  18 +
 ...nch-opencode-api-key-expi-issue-36579a10979a.md |   6 +
 ...ly-first-line-to-opencode-issue-62ab3daa615d.md | 123 ++++++
 ...blocked-by-low-disk-space-issue-916633ab3313.md |   6 +
 ...wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md |   6 +
 ...e-dll-build-at-game-build-issue-0322fa45b0e8.md |  18 +
 ...move-avoidable-build-and--issue-e044a931c54b.md |  10 +
 ...ative-editing-and-credite-issue-421c5be1669f.md |   6 +
 ...d-continuous-issue-progre-issue-91e3384b219b.md | 215 +++++++++++
 ...s-to-the-latest-file-atom-issue-52b59bdd440c.md |  36 ++
 ...ait-issues-and-proof-firs-issue-9a4503f6ed68.md |  26 ++
 ...le-restart-and-self-recov-issue-6fc8f853a674.md |  18 +
 ...ner-preview-overlays-and--issue-06c8cd76dc9d.md |  26 ++
 ...opment-and-networking-con-issue-cf9cd77eb056.md |  56 +++
 ...vior-fixes-discoverable-a-issue-0cdac76fb006.md |  38 ++
 docs/regressions/dev-loop/README.md                |   9 +-
 .../counterstrike-npc-targeting-REG-2026-10-04.md  |  27 ++
 docs/specs/dev-loop/agent-build-queue.md           |  11 +
 docs/specs/dev-loop/agent-context-contract.md      |  42 ++
 docs/specs/dev-loop/agent-coordination.md          |  68 +++-
 docs/specs/dev-loop/agent-self-improvement.md      |  54 +++
 docs/specs/dev-loop/build-time-observability.md    |  22 ++
 docs/workflows/issue-intake-and-triage.md          |  31 ++
 src/actor/player-ai.cpp                            | 168 ++++++--
 src/engine/engine-tick-combat.cpp                  |   8 +-
 src/engine/engine-tick-net.cpp                     |   9 +
 src/engine/engine-tick-net.h                       |   5 +
 src/gui/gui-main.cpp                               |  54 ++-
 src/network/multiplayer-projectiles.cpp            |  15 +-
 src/network/schema/packet-combat.h                 |   2 +-
 src/network/server-npcs.cpp                        |   7 +-
 src/network/server-projectiles.cpp                 |   5 +
 src/npc/npc.cpp                                    |  11 +-
 .../movement/physics-collision-glb-sweep-slide.cpp | 218 ++++++++++-
 src/physics/physics-types.h                        |   2 +
 58 files changed, 2585 insertions(+), 252 deletions(-)
```

## Captured changed paths

```text
M	config/accounts/default.json
M	config/analytics.json
M	config/audio/music-settings.json
M	config/behavior-profiles.json
M	config/hot-reload/packet-ledger.json
M	config/maps/funworld3.json
M	config/movement/movement-heavy.json
M	config/movement/movement-source.json
M	devscripts/agent-wake.py
M	devscripts/agent_coord.py
M	devscripts/blender-ai
M	devscripts/dev-loop.py
M	devscripts/dev_loop/game_build.py
M	devscripts/dev_loop/gui.py
M	devscripts/dev_loop/parallel_test_runs.py
M	devscripts/game_build_worker.py
M	devscripts/process_identity.py
M	devscripts/quality/test_agent_coord.py
M	devscripts/quality/test_game_build_snapshot.py
M	docs/changelog/2026-11-20/20261120_060500-launch-failure-issues-preview-overlay-first-frame-context.md
M	docs/issues/README.md
M	docs/issues/bug/20261120-dev-loop-gui-process-inventory-can-omit-or-miscl-issue-35fd6df3d947.md
M	docs/issues/bug/20261120-frame-consistency-no-frame-below-240-fps-issue-549680e53b26.md
M	docs/issues/bug/20261120-melee-damage-arcade-collision-and-mace-active-po-issue-503f3e410c01.md
M	docs/issues/bug/20261120-wake-adapter-cannot-launch-opencode-api-key-expi-issue-36579a10979a.md
M	docs/issues/bug/20261120-wake-prompt-delivers-only-first-line-to-opencode-issue-62ab3daa615d.md
M	docs/issues/build-friction/20261120-game-dll-build-blocked-by-low-disk-space-issue-916633ab3313.md
M	docs/issues/build-friction/20261120-measure-every-dev-loop-wait-stage-and-reduce-avo-issue-7d3dcfd7433b.md
M	docs/issues/build-friction/20261120-wait-over-budget-game-dll-build-at-game-build-issue-0322fa45b0e8.md
M	docs/issues/build-friction/20261120-waiting-is-the-enemy-remove-avoidable-build-and--issue-e044a931c54b.md
M	docs/issues/capability/20261120-map-world-live-collaborative-editing-and-credite-issue-421c5be1669f.md
M	docs/issues/improvement/20261120-agent-awareness-gaps-and-continuous-issue-progre-issue-91e3384b219b.md
M	docs/issues/improvement/20261120-apply-disjoint-proposals-to-the-latest-file-atom-issue-52b59bdd440c.md
M	docs/issues/improvement/20261120-automatic-over-budget-wait-issues-and-proof-firs-issue-9a4503f6ed68.md
M	docs/issues/improvement/20261120-autonomy-always-available-restart-and-self-recov-issue-6fc8f853a674.md
M	docs/issues/improvement/20261120-build-queue-rotating-owner-preview-overlays-and--issue-06c8cd76dc9d.md
M	docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md
M	docs/issues/improvement/20261120-make-small-desktop-behavior-fixes-discoverable-a-issue-0cdac76fb006.md
M	docs/regressions/dev-loop/README.md
M	docs/regressions/npc-movement/counterstrike-npc-targeting-REG-2026-10-04.md
M	docs/specs/dev-loop/agent-build-queue.md
M	docs/specs/dev-loop/agent-context-contract.md
M	docs/specs/dev-loop/agent-coordination.md
M	docs/specs/dev-loop/agent-self-improvement.md
M	docs/specs/dev-loop/build-time-observability.md
M	docs/workflows/issue-intake-and-triage.md
M	src/actor/player-ai.cpp
M	src/engine/engine-tick-combat.cpp
M	src/engine/engine-tick-net.cpp
M	src/engine/engine-tick-net.h
M	src/gui/gui-main.cpp
M	src/network/multiplayer-projectiles.cpp
M	src/network/schema/packet-combat.h
M	src/network/server-npcs.cpp
M	src/network/server-projectiles.cpp
M	src/npc/npc.cpp
M	src/physics/movement/physics-collision-glb-sweep-slide.cpp
M	src/physics/physics-types.h
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
    "code_hash": "d70d49fd185ff12108e1f95c11c42f66e52c201f296d222d9cbba819c9e9a2f6",
    "dll": "build\\game\\generation-420\\game-g420.dll",
    "generation": 420,
    "revision": "f0ca5e5eec1b1ecc957729592bb8f4d67e951aa83f4ad0295cfb2e75d1534c3f",
    "status": "ok"
  },
  "game_ready": {
    "dll": "build\\game\\generation-420\\game-g420.dll",
    "generation": 420,
    "published_utc": "2026-11-21T13:59:59.761Z",
    "revision": "f0ca5e5eec1b1ecc957729592bb8f4d67e951aa83f4ad0295cfb2e75d1534c3f",
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
