# UTC build identities, dashboard activity, and dev-loop organization

**Time:** 2026-11-03 10:24 America/New_York (2026-11-03T15:24:47Z)
**Branch / base:** `20261011zt` / `684f7101dc8046e9b745095d243ceed52244c1f2`
**Scope:** `devscripts/` and the dev-loop specification, plus this changelog and regression record.

## Existing worktree changes preserved

At task start, `devscripts/blender-ai` was modified and two unrelated changelogs were untracked: `docs/changelog/2026-11-03/20261103_134448-encrypted-backup-rollout-blocked.md` and `docs/changelog/2026-11-03/20261103_140123-drive-backup-and-monitor-status.md`. They were left untouched.

While the live GUI/game launch was being verified, `config/analytics.json` incremented `launch_count` from 3069 to 3071 through normal game startup, and `docs/changelog/2026-11-03/20261103_142412-rclone-drive-auth-prep.md` appeared as an additional untracked file. Neither was edited by this task.

## Changes

- `devscripts/dev-loop.py` remains the entry point and delegates CLI parsing, build execution, artifact identity/selection, launch selection, runtime identity, JSON state, status formatting, GUI, and terminal responsibilities to `devscripts/dev_loop/`. `devscripts/dev_loop_gui.py` and `devscripts/dev_loop_terminal.py` preserve their prior import paths as compatibility shims. Existing CLI flags and aliases remain available.
- Published artifacts use UTC ISO 8601 IDs. A separate safe folder selector is used on disk; sidecar and SHA-256 validation protect launch selection; legacy numeric folders remain discoverable. Invalid newest artifacts fall back to the newest valid prior artifact.
- The GUI wraps long activity lines, keeps a vertical scrollbar, and draws a gutter using the Tk text index at each visible row so wrapped rows repeat the logical entry number. It displays at most ten recent activity entries and retains the compact 400x500 window size. Control requests are correlated with their final daemon result.
- The script inventory and dev-loop specification document the extracted module ownership, compatibility approach, timestamp IDs, and phased reference-audited organization of other scripts. No other script files were relocated in this phase.
- Added `devscripts/dev_loop/test_artifacts.py` and regression record `docs/regressions/2026-11-03/dev-loop-utc-folder-launch-REG.md`.

## Validation evidence

- Focused Python tests: 4 artifact tests passed, covering timestamp uniqueness, safe folders, legacy discovery/order, sidecar SHA validation, and fallback selection.
- `python -m compileall` passed for the stable entry point, watcher, package, and compatibility shims.
- `python devscripts/dev-loop.py --help` retained the existing CLI options and `--cmd` alias.
- Live daemon status reported latest UTC ID `2026-11-03T14:11:32.294522Z`, safe artifact folder `2026-11-03T14-11-32.294522Z`, and build identity `2026-11-03T14:11:32.294522Z:f5eaf35e57a3f82496c039a22977295c0cef65ab3bb02f122ac635b5148d58ef`; the executable identity was valid and source-current.
- `python devscripts/quality/agent-gate.py --fast` passed: canonical build, seven focused selftests, lint baseline, and weapon/gamemode sync. The earlier full gate completed build/selftests/lint/sync but its two runtime scenarios timed out waiting for a live run (`grenade_tick_contract_v1.json`, `npc_wall_containment_v1.json`); no baseline was changed.
- A real launch request correlated to request ID `2026-11-03T14:11:05.509Z-27719284` ended in `NEEDS_USER_ACTION` because Windows Security consent windows were detected. The dev-loop journal is `logs/2026-11-03/20261103_141105_01/events.jsonl`. The game did not launch, so successful runtime launch is not claimed.
- Follow-up confirmed that the detector had falsely classified two unrelated background windows titled exactly `Windows Security`; Blender was foreground and the user reported no popup. Detection now requires specific firewall-alert/consent wording, and the pre-launch snapshot no longer blocks launch. The retry (`2026-11-03T14:18:38.249Z-1c248da8`, journal `logs/2026-11-03/20261103_141838_01/events.jsonl`) recorded no firewall prompt and passed artifact/configuration/preflight/staging/reconciliation, but its daemon was restarted before `server_process_start`; no game process was observed. Launch is therefore not yet verified.
- The launch exception boundary then identified two missing references introduced by extraction: stable-path activation used `_EXECUTABLE_IDENTITY_CACHE` after that cache moved into `dev_loop.runtime`, and server setup used `tempfile.mkstemp()` without the entry-point import. Added `invalidate_executable_identity(path)` to `dev_loop/runtime.py`, wired stable-path replacement to it, and restored `import tempfile`. Launch exceptions now produce a failed control result and traceback in the canonical journal rather than silently ending the daemon.
- Verified the dashboard control path through the GUI's own Launch button callback (`MIMITA_DEV_LOOP_GUI_TEST_ACTION=launch_latest`). Correlated request `dashboard-06c570e9` completed from the `MiMITA Dev Loop Dashboard — mimita-v9` window; journal `logs/2026-11-03/20261103_142636/events.jsonl` records server PID 3596 and client PID 31572 ready, client `Connected`, managed mode `ACTIVE`, matching UTC build ID `2026-11-03T14:24:42.089012Z`, SHA-256 `f5eaf35e57a3f82496c039a22977295c0cef65ab3bb02f122ac635b5148d58ef`, and `FIREWALL_CLEAR`. The final control record has `requested_by=dashboard` and `status=completed`. Both dashboard and game client windows have titles; rendered gameplay was not directly visually inspected.
- Final follow-up validation after the launch fixes: Python compilation passed, all 4 focused artifact tests passed, CLI help retained `--request`, `--dashboard`, and `--cmd`, and `git diff --check` passed with line-ending conversion warnings. Final live status remains `running`, server/client ready, build IDs matched, firewall result `FIREWALL_CLEAR`.
- Windows shell follow-up: the live dashboard HWND had extended style `0x80` (`WS_EX_TOOLWINDOW`) and lacked `WS_EX_APPWINDOW`, explaining its omission from task switching. The GUI now registers process AppUserModelID `MiMITA.DevLoop.Dashboard`, applies shell styles to `GetAncestor(hwnd, GA_ROOT)`, clears tool-window style, forces a frame refresh, and reapplies styles after restore. The new window HWND 465244 was visible with extended style `0x40000`; the computer-use window inventory now lists it as `MiMITA Dev Loop Dashboard — mimita-v9`, and the Alt+Tab inventory contains the same independent window. Win+Tab uses the same app-window registration; the native task-view overlay was not visually inspected.
- GUI resize/wrapping behavior was not visually inspected in a targetable desktop window; the widget layout and wrapping/gutter logic are present, but visual acceptance remains open.
- `git diff --check` passed with only line-ending conversion warnings.

## Human review still needed

Inspect the dashboard at compact and larger sizes to confirm the activity gutter and wrapping visually. Runtime readiness, launch identity, and GUI control completion are verified; human visual gameplay review remains open.
