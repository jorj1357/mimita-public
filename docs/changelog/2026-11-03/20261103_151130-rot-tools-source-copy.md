# RoTools source copy for Blender camera controls

**Time:** 2026-11-03 10:11 America/New_York (2026-11-03T15:11:30Z)
**Branch / base:** `20261011zt` / `684f7101dc8046e9b745095d243ceed52244c1f2`
**Scope:** `devscripts/blender/roblox-studio-controls/` and this changelog.

## Existing worktree changes preserved

At task start, pre-existing modifications were present in `config/analytics.json`, `devscripts/README.md`, the `devscripts/blender-ai` submodule, `devscripts/dev-loop-watch.py`, `devscripts/dev-loop.py`, `devscripts/dev_loop_gui.py`, `devscripts/dev_loop_terminal.py`, and `docs/specs/dev-loop/dev-loop.md`. Pre-existing untracked files/directories included `devscripts/dev_loop/`, four changelogs (`20261103_134448-encrypted-backup-rollout-blocked.md`, `20261103_140123-drive-backup-and-monitor-status.md`, `20261103_141217-utc-build-dashboard-launch-organization.md`, `20261103_142412-rclone-drive-auth-prep.md`), and `docs/regressions/2026-11-03/`. These unrelated edits were preserved.

The requested destination already existed and was empty. The enabled Blender add-on remains in the user add-ons folder; it was not overwritten.

## Changes

- Added `devscripts/blender/roblox-studio-controls/README.md` for the user, describing the source layout, control owners, Blender preferences and defaults, how to copy/reload source, and the fact that the existing add-on does not load a standalone JSON file.
- Copied the 31 Python source files of RoTools 0.5.3 into `devscripts/blender/roblox-studio-controls/roblox_tools/` from the current staging package. Relevant owners: `operators/navigation.py` (`ROTOOLS_OT_fly_navigation._look`, `_move`, and `ROTOOLS_OT_fly_dolly.execute`), `tools/select_tool.py` (RMB and wheel bindings while Select is active), and `core/preferences.py` (the six camera controls). The current pitch term remains `current_elevation - dy * sensitivity`; the user has said look direction still does not feel right, so this is an inspection copy, not a behavior fix or acceptance claim.
- No external GitHub action, installed add-on edit, MiMITA feature change, or JSON loader was made.

## Validation evidence

- All 31 copied Python files matched the source staging files by SHA-256.
- Python AST syntax parse passed for all 31 files.
- `python devscripts/quality/agent-gate.py` passed: canonical build, focused selftests, lint baseline, configuration sync checks, and both live scenarios (`grenade_tick_contract_v1` and `npc_wall_containment_v1`). No baseline edits were made.
- Documentation review against `docs/skills/documentation-checker-v1.md`: the README states its purpose and audience, explains scope boundaries, and points to the current relevant source files. Routes and instructions do not conflict with the repository documentation reviewed (`docs/ROUTER.md`, task completion, time formatting, regression guide, quality gates, and JSON configuration architecture).

## Human review still needed

Open the repository folder and review `README.md` plus `roblox_tools/operators/navigation.py`. The user still needs to report what they expect for vertical mouse drag and which of these observed actions differs (RMB start/look/release, WASD, Q/E, Shift, or wheel). This source copy does not update Blender automatically; live Blender acceptance remains open.
