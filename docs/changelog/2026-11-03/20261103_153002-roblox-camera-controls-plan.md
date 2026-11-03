# Roblox camera controls plan and Blender-side proof

**Time:** 2026-11-03 10:30 America/New_York (2026-11-03T15:30:02Z)
**Branch / base:** `20261011zt` / `684f7101dc8046e9b745095d243ceed52244c1f2`
**Scope:** `devscripts/blender/roblox-studio-controls/open-issues.md` and this changelog.

## Existing worktree changes preserved

At task start, existing modifications were present in `config/analytics.json`, `devscripts/README.md`, the `devscripts/blender-ai` submodule, `devscripts/dev-loop-watch.py`, `devscripts/dev-loop.py`, `devscripts/dev_loop_gui.py`, `devscripts/dev_loop_terminal.py`, and `docs/specs/dev-loop/dev-loop.md`. Existing untracked items included `devscripts/blender/roblox-studio-controls/`, `devscripts/dev_loop/`, the changelogs `20261103_134448-encrypted-backup-rollout-blocked.md`, `20261103_140123-drive-backup-and-monitor-status.md`, `20261103_141217-utc-build-dashboard-launch-organization.md`, `20261103_142412-rclone-drive-auth-prep.md`, and `20261103_151130-rot-tools-source-copy.md`, plus `docs/regressions/2026-11-03/`. They were preserved.

During the repository gate, additional worktree changes appeared in `config/accounts/default.json`, `config/audio/music-settings.json`, and `docs/changelog/2026-11-03/20261103_152655-vps-layout-rclone-readiness.md`. Their origin was not established in this task; they were left untouched.

## Change

Added a 69-line proposed plan at the bottom of `devscripts/blender/roblox-studio-controls/open-issues.md` (lines 46-114), retaining the user’s original rough notes above it. The plan records the current behavior in `tools/select_tool.py`, `operators/navigation.py`, and `core/preferences.py`; spells out expected Roblox Select behavior; gives an implementation order for VS Code diagnostics, settings, keymaps, mouse sign, and scroll dolly; and proposes separate static, registration, math, and live Blender UI checks. It defines a test-only JSONL trace under the temp folder, since this Blender add-on is separate from MiMITA’s `events.jsonl` pipeline.

The plan calls out that the screenshot's Python 3.14 IDE selection and Blender's bundled Python/type information should be separated during diagnosis. It recommends keeping Blender's property annotations until an actual runtime/API error is found. No add-on code, Blender install, or behavior was changed, and no claim of a working camera interaction is made.

## Documents and references reviewed

- `AGENTS.md`, `docs/ROUTER.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/regressions/README.md`.
- `docs/skills/documentation-checker-v1.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/debug-logging/debug-logging.md`, and `docs/architecture/json-configuration/json-configuration.md`.
- Blender 5.0 API references linked in the new plan: Event, Operator, and WorkSpaceTool; Blender Add-on Development Setup.

## Validation evidence

- Confirmed all source references in the plan exist in the local add-on copy; the added plan has no trailing whitespace.
- `git diff --check` reported only line-ending conversion warnings in pre-existing modified files; no source files were changed in this task.
- `python devscripts/quality/agent-gate.py` passed: canonical build, focused selftests, lint baseline, JSON sync checks, and `grenade_tick_contract_v1` and `npc_wall_containment_v1` live scenarios. This gate validates MiMITA, not Blender camera controls.

## Human review still needed

Review the proposed behavior and decisions in `open-issues.md`, especially Ctrl-slow multiplier and whether Roblox Select should be the sole on/off switch. The 300 px mouse examples, keyboard overrides, cursor release, dolly behavior, and the VS Code Problems panel still need direct Blender 5.0/VS Code verification during implementation.
