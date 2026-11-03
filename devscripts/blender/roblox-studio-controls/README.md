# RoTools Roblox Studio Controls (local source copy)

This folder contains the RoTools 0.5.4 add-on source used for the Blender 5.0 camera-control work, copied here so it is easy to review and discuss.

## Where to look

- `roblox_tools/operators/navigation.py` — WASD camera movement without RMB, Q/E vertical movement, Shift/Ctrl speed modifiers, RMB camera look, and scroll-wheel dolly. Movement math is in `_move_view`; RMB rotation is in `_look_view`; key-hold handling is in `ROTOOLS_OT_camera_move.modal`; wheel movement is in `ROTOOLS_OT_fly_dolly.execute`.
- `roblox_tools/tools/select_tool.py` — binds the camera keys, RMB, and wheel actions while the RoTools Roblox Select tool is active; its original left-click/drag selection binding remains in place.
- `roblox_tools/core/preferences.py` — editable Blender Add-on Preferences and the default camera values listed below.
- `roblox_tools/core/keymaps.py` — RoTools tool-switch shortcuts.

## Camera defaults

These values are editable in Blender under **Edit → Preferences → Add-ons → RoTools → Camera Navigation**:

| Setting | Default | Effect |
| --- | ---: | --- |
| Editor Move Speed | 80.0 | Base view movement in Blender units per second; 10× the original 8.0 default |
| Q/E Vertical Speed Multiplier | 10.0 | Multiplies the base speed for vertical movement |
| Shift Speed Multiplier | 4.0 | Multiplies movement speed while Shift is held |
| Ctrl Slow Multiplier | 0.25 | Multiplies movement speed while Ctrl is held |
| Mouse Look Sensitivity | 0.0025 | Rotation per mouse pixel while RMB is held |
| Scroll Dolly Percentage | 0.12 | Fraction of current view distance for each wheel step |
| Minimum Scroll Step | 0.05 | Minimum forward/back distance per wheel step |

Q/E use the base speed × the 10.0 vertical multiplier; Shift multiplies movement by 4.0, while Ctrl multiplies by 0.25. The view math is kept separate from event handling so its directions can be checked with repeatable pixel and key-duration inputs.

## Editing and using this copy

Blender loads the add-on from its enabled add-on folder, currently:

`%APPDATA%\Blender Foundation\Blender\5.0\scripts\addons\roblox_tools`

This repository copy is separate from that installed folder. Editing files here does not change the running Blender add-on by itself. After changing code, copy the `roblox_tools` package into the installed add-on folder (or install a ZIP containing the package at its root), then reload/restart the add-on in Blender.

The camera settings are stored as Blender Add-on Preferences; this version does not read a separate JSON config file. If you want the settings moved into a plain config file later, we can work from this visible source and define that format together.

## Audience and boundaries

This folder is for you to inspect and edit the RoTools camera controls, then share specific behavior or configuration requests.

This folder does not synchronize edits into Blender automatically and does not add a standalone JSON configuration loader. Blender continues to use the add-on package installed in its user add-ons directory.
## Scope and verification

This is the RoTools 0.5.4 source. Verification status belongs in the session changelog; Python syntax and math checks do not substitute for checking the controls in Blender 5.0.




