# RoTools Roblox-style camera controls

Time: `2026-11-03T16:21:52Z` (2026-11-03 11:21:52 EST, America/New_York)
Branch: `20261011zt`
HEAD before this session's final record: `54e3895bf4f465e6dba5ca83a1c4e66c113741a7`

## Scope and prior state

Implemented the requested camera controls in `devscripts/blender/roblox-studio-controls` and installed the updated package files for Blender 5.0. The user requested camera movement only while RoTools Roblox Select is active, preserved block selection/drag behavior, and speed defaults ten times the former working speed. The add-on folder was already a dirty worktree at session start; its RoTools 0.5.4 edits are part of this task thread. The unrelated dirty `devscripts/blender-ai` submodule was preserved. The repository gate temporarily incremented `config/analytics.json` launch_count from 3078 to 3080 through its scenarios; that test-only delta was restored, leaving the file clean.

## Changes

- `roblox_tools/tools/select_tool.py`: added tool-scoped bindings for W/A/S/D/Q/E and RMB. The existing LMB `rotools.select` binding with `allow_drag=True` remains unchanged. W/A/S/D/Q/E now start the movement operator only when Roblox Select is active.
- `roblox_tools/operators/navigation.py`: split view math into `_look_view` and `_move_view`; RMB controls look while the held-key operator uses a frame timer. The pitch rotation changed from `Quaternion(right, elevation - current_elevation)` to `Quaternion(right, current_elevation - elevation)`, correcting the math so positive event-Y looks up and negative event-Y looks down. Movement uses forward/right for WASD, world up/down for E/Q, and Shift/Ctrl multipliers.
- `roblox_tools/core/preferences.py`: changed the base move default from `8.0` to `80.0` Blender units/second, added editable Ctrl slow multiplier `0.25`, kept Q/E vertical multiplier `10.0` and Shift multiplier `4.0`, and surfaced all movement/look/dolly values in Add-on Preferences.
- `roblox_tools/__init__.py`: marked source version 0.5.4.
- `README.md` and `open-issues.md`: documented the controls, settings, and evidence boundary.
- `tests/test_navigation.py`: added Blender 5.0 registration/unregistration, left-drag binding, view direction, movement direction/speed, and modifier checks.
- Installed the `.py` package files under `%APPDATA%\Blender Foundation\Blender\5.0\scripts\addons\roblox_tools`; saved Blender's existing `camera_move_speed` preference as `80.0` using `bpy.ops.wm.save_userpref()`.
- Created `C:\Users\guita\Downloads\RoTools-0.5.4-roblox-camera-controls.zip` with `roblox_tools/` at archive root and without bytecode caches.

## Evidence

- Blender 5.0 background run: `tests/test_navigation.py` — PASS for operator registration/unregistration; declared W/A/S/D/Q/E bindings; original left-click/drag binding; +300 horizontal look right; +300 event-Y look up and -300 event-Y look down; fixed camera position during look; W/A/S/D at 80 units/s; Q/E at 800 units/s; Shift 4×; Ctrl 0.25×.
- `python devscripts/quality/agent-gate.py` — GREEN: canonical MiMITA build, focused selftests, lint baseline, config sync checks, and runtime scenarios passed. Its launch-count side effect was reverted.
- ZIP inspection: 32 entries, no `__pycache__` or `.pyc`, includes the corrected pitch sign.
- Blender's current open session reports the add-on's already-loaded 0.4.0 class, even though installed source is 0.5.4. Safe-mode restrictions rejected add-on disable/enable and registration through the Blender scripting bridge. The saved preference is confirmed as 80.0, but this session has not loaded the new classes.

## Acceptance and remaining check

Source, registration in background Blender, and isolated math are PASS. Repository gate is PASS. Live viewport acceptance is still required after Blender reload/restart: verify A does not Select All, W does not switch tools, held WASD and Q/E move continuously, Shift/Ctrl speed, RMB physical look direction and cursor restoration, left-drag surface placement, and G. The user's current `.blend` is unsaved; it was not saved or closed. Do not claim those live input checks passed.

## Documents and review

Read `AGENTS.md`, `docs/ROUTER.md`, the routed task-completion procedure, time/format standard, regressions README, runtime scenario validation, JSON configuration architecture, debug logging, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/documentation-checker-v1.md`. No MiMITA behavior specification owns this Blender add-on.

The documentation TODO scan found the existing unrelated note at `docs/architecture/player-npc-systems/npc-movement.md:22`: `9 8 2026 1019 est jorj todo - explain`. Suggested expansion: explain the desired NPC movement behavior and link the owning specification and acceptance evidence. This unrelated source document was not edited.
