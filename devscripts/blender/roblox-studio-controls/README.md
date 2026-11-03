# RoTools Roblox Studio Controls (local source copy)

This folder contains the RoTools 0.5.3 add-on source used for the Blender 5.0 camera-control work, copied here so it is easy to review and discuss.

## Where to look

- `roblox_tools/operators/navigation.py` — hold-RMB camera look, WASD movement, Q/E vertical movement, Shift speed, and scroll-wheel dolly behavior. The mouse look calculation is in `ROTOOLS_OT_fly_navigation._look`; movement is in `_move`; wheel movement is in `ROTOOLS_OT_fly_dolly.execute`.
- `roblox_tools/tools/select_tool.py` — binds RMB press and wheel actions while the RoTools Roblox Select tool is active.
- `roblox_tools/core/preferences.py` — editable Blender Add-on Preferences and the default camera values listed below.
- `roblox_tools/core/keymaps.py` — RoTools tool-switch shortcuts.

## Camera defaults

These values are editable in Blender under **Edit → Preferences → Add-ons → RoTools → Camera Navigation**:

| Setting | Default | Effect |
| --- | ---: | --- |
| WASD Camera Speed | 8.0 | Base view movement in Blender units per second |
| Q/E Vertical Speed Multiplier | 10.0 | Multiplies the base speed for vertical movement |
| Shift Speed Multiplier | 4.0 | Multiplies movement speed while Shift is held |
| Mouse Look Sensitivity | 0.0025 | Rotation per mouse pixel while RMB is held |
| Scroll Dolly Percentage | 0.12 | Fraction of current view distance for each wheel step |
| Minimum Scroll Step | 0.05 | Minimum forward/back distance per wheel step |

Vertical movement uses base speed × vertical multiplier, and Shift multiplies that result. The mouse-look direction calculation is in `_look`; its current vertical term is `current_elevation - dy * sensitivity`. Review that line if dragging the mouse up/down still feels reversed on your Blender setup.

## Editing and using this copy

Blender loads the add-on from its enabled add-on folder, currently:

`%APPDATA%\Blender Foundation\Blender\5.0\scripts\addons\roblox_tools`

This repository copy is separate from that installed folder. Editing files here does not change the running Blender add-on by itself. After changing code, copy the `roblox_tools` package into the installed add-on folder (or install a ZIP containing the package at its root), then reload/restart the add-on in Blender.

The camera settings are stored as Blender Add-on Preferences; this version does not read a separate JSON config file. If you want the settings moved into a plain config file later, we can work from this visible source and define that format together.

## Audience and boundaries

This folder is for you to inspect and edit the RoTools camera controls, then share specific behavior or configuration requests.

This folder does not synchronize edits into Blender automatically and does not add a standalone JSON configuration loader. Blender continues to use the add-on package installed in its user add-ons directory.
## Scope and verification

This is a source copy of RoTools 0.5.3 with the camera navigation changes. It is provided for inspection and iteration; it does not claim that the mouse direction is accepted as correct on your machine. The currently enabled Blender copy remains in the user add-ons folder above.


