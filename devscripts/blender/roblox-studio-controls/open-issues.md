### explanation 

2026 11 03 1019 jorj1357 wtf is this well
document for issues relating to  this 
probabl gona be small but since  i tihnk it shouldg et pushed back to being in the actual rotools repo bc its awesome  then its nice to have documentation 

ok issues format
i use EST time so it gona be a little off 
idk UTC to EST but ideal is ISO 8601 time 
but i do this:

yyyy mm dd hhmm (your name/username/whatver u wanna get called by)

(general descritpion of issues)

### issues

2026 11 03 1020 jorj - 

i wnat ot mkae it so 

even if rmb not held down, WASD = camera movement, 
right now when its not held, A = select all like normal blender behavior, not good,

if roblox studio controls mode is enabled i want it to be overriding like all the binds 

so W = forward cam
Q/E = up down
A = left cam

S = back, D = right 

left shift = faster 
left ctrl = slower cam, i tihnk?


C:\mimita-v9\devscripts\blender\roblox-studio-controls\roblox_tools\core\preferences.py
tjhis has a loto f errors and also i think needs a better readme like 

can just call it like 

editor_move_speed = 10.0 or something
so its smth ez i can edit easily 
---

## Proposed plan: Roblox-style camera controls and reliable proof

**Purpose:** Turn the rough notes above into a concrete sequence for fixing the camera controls and the VS Code warnings.

**Audience:** The add-on author and anyone reviewing or testing the Blender 5.0 add-on.

**Boundary:** This is a plan, not a claim that the controls are fixed. Blender does not produce MiMITA's `events.jsonl`; Blender-side tests and a small add-on diagnostic trace will provide the evidence for this feature.

The notes above are kept as the original report. The plan below clarifies the expected behavior and separates code checks from Blender interaction checks.

### What the current source does

- `roblox_tools/tools/select_tool.py` binds RMB and the scroll wheel while the **Roblox Select** workspace tool is active.
- `roblox_tools/operators/navigation.py` starts camera navigation on RMB press. WASD and Q/E movement are processed only while that modal operator is running, so WASD does not move the camera after RMB is released.
- `roblox_tools/core/preferences.py` already exposes camera speed values in Blender Add-on Preferences. Current defaults are WASD `8.0`, vertical multiplier `10.0`, Shift multiplier `4.0`, look sensitivity `0.0025`, scroll fraction `0.12`, and minimum scroll step `0.05`.
- The look code currently calculates pitch with `current_elevation - dy * sensitivity`. Since Blender mouse coordinates are window-relative and the observed up/down feel is still wrong, verify the actual event deltas and view result before changing the sign.

### Target behavior while Roblox Select is active

Treat the selected **Roblox Select** tool as Roblox Controls mode, scoped to the 3D Viewport in Object Mode. When another tool is active, Blender's ordinary shortcuts return.

| Input | Expected result in Roblox Controls mode |
| --- | --- |
| Hold W / A / S / D, with or without RMB | Move the view forward / left / backward / right continuously while the key is held |
| Hold Q / E | Move the view down / up in world vertical direction |
| Hold left Shift | Move faster |
| Hold left Ctrl | Move slower; proposed multiplier is `0.25` until tuned in Blender |
| Hold RMB and move mouse | Rotate the view in place; release RMB to stop and restore the cursor |
| Move mouse right by 300 px | Look right |
| Move mouse up by 300 px | Look up; moving down by 300 px looks down |
| Scroll wheel | Dolly the camera forward/backward in fixed, repeatable steps independent of the cursor target, including near geometry |
| Left-click / drag | Keep RoTools selection, block dragging, and surface snapping |
| G | Keep Blender's normal Move command |
| Disable Roblox Select | Restore Blender's ordinary key behavior, including A for Select All |

The 300 px examples describe a **delta**, not a required absolute screen location. A live check may move from `(500, 500)` to `(800, 500)` inside the 3D Viewport and should record `dx=+300, dy=0`. A vertical check should record `(dx=0, dy=+300)` for an upward move; confirm Blender's reported coordinate direction in the trace rather than guessing from the screen alone.

### Implementation sequence

1. **Fix the editor feedback first.** In VS Code, use Blender 5.0-compatible `bpy` type stubs for Pylance and point analysis at those stubs. The screenshot shows Python 3.14 selected in VS Code; Blender runs its own bundled Python, so the IDE interpreter and Blender runtime must be treated separately. Keep Blender's `bpy.props.*Property(...)` annotation declarations intact unless a Blender runtime check proves one is actually invalid. Recheck the Problems panel after the stub setup and label each remaining item as a real Python issue or an unresolved IDE warning.
2. **Make the settings easy to edit.** Give the camera values a clearly grouped, single set of defaults with names such as `editor_move_speed = 10.0`, `editor_vertical_multiplier = 10.0`, `editor_fast_multiplier = 4.0`, `editor_slow_multiplier = 0.25`, `editor_look_sensitivity`, and the two dolly values. Show the same names and units in Add-on Preferences and README. Avoid keeping one default in the operator and a different default in the preferences. Preserve users' existing saved Blender preference values when identifiers are renamed; test both fresh and already-configured preferences.
3. **Separate camera movement from looking.** Keep the RMB modal operator responsible for mouse look only. Add a 3D Viewport keymap owned by Roblox Select mode for held WASD/QE movement, with press/release tracking and frame-rate-independent motion. Apply Shift/Ctrl speed multipliers while keys are held. Confirm that Blender releases every held key when the viewport loses focus, the tool changes, or the add-on is disabled.
4. **Resolve shortcut precedence deliberately.** While Roblox Select is active, W/A/S/D/Q/E must route to camera movement instead of Blender actions such as A = Select All. Keep the override limited to the 3D Viewport and the active Roblox Select tool. Switching away from that tool must remove or deactivate the add-on bindings and restore Blender's normal shortcuts. Verify this against the resolved Blender key configuration, not just the add-on's declared keymap.
5. **Correct and lock the mouse direction with examples.** Record raw mouse coordinates and calculated deltas. Use the stated 300 px tests to assert right, left, up, and down directions and compare before/after `view_rotation` and `view_location`. Check small movements (1 px), large movements (300 px), cursor capture/warp, and RMB release. Adjust the sign or coordinate source only after the trace identifies the mismatch.
6. **Make the dolly test measurable.** Record camera position and view distance before and after each wheel step at both far and near distances. Confirm repeated forward steps keep moving instead of stopping at a target or collapsing to a zero step, and that the cursor location does not change the destination.

### How an AI can prove it

Use Blender's own Python/API surface and a purpose-built Blender test report. A MiMITA `events.jsonl` file is not required for this add-on.

- **Static check:** Parse/compile every add-on Python file with the Blender 5.0 Python runtime or run Blender 5.0 in background mode. This catches syntax/import errors, but proves no camera interaction.
- **Registration check:** In Blender 5.0 background mode, enable/register and disable/unregister the add-on; assert the operators, preferences, and declared workspace tool register cleanly. This proves loadability, not keymap precedence or modal input.
- **Math checks:** Exercise the movement and mouse-delta math with fixed inputs. For a 300 px horizontal delta, assert the view turns right; for a 300 px positive vertical delta, assert it looks up; for the opposite deltas, assert opposite results. Check WASD/QE direction and speed multipliers from known start values. These isolate math and do not replace real Blender input.
- **Blender UI check:** In the actual Blender 5.0 3D Viewport, use a known empty scene with one cube and floor. While Roblox Select is active, test movement with RMB up, look with RMB held, cursor restoration on release, A not selecting all, Q/E, Shift/Ctrl, wheel at near and far distances, LMB drag/snap, G, and switching back to Blender's standard tool. Save before/after view values for each step.
- **Small optional diagnostic file:** During a deliberate test run only, write JSON Lines to a clearly named file in the user's temp folder, for example `%TEMP%\RoTools\camera-controls-test.jsonl`. Each record should include Blender/add-on version, active tool, input event, raw and relative mouse coordinates, computed `dx/dy`, view transform before/after, expected direction, and pass/fail. Do not log every normal editing event or create a second always-on logger. Keep screenshots or a short screen recording for cursor and feel checks.
- **Result report:** Summarize separate outcomes for source/syntax, registration, math, live UI behavior, and human feel review. A green syntax check must never be reported as proof that the camera controls work.

### Open details to settle during implementation

- Confirm whether Ctrl-slow should be `0.25x` or a different multiplier.
- Confirm whether the proposed base speed of `10.0` Blender units/second feels right in the user's large map; if not, tune it through Preferences and record the chosen value.
- Confirm that Roblox Controls mode should be active exactly while the Roblox Select tool is selected. This is the recommended scope because it matches the user's description and avoids changing shortcuts elsewhere in Blender.

### Blender API references

- [Blender 5.0 Event API](https://docs.blender.org/api/5.0/bpy.types.Event.html) documents window and region mouse coordinates and modifier state.
- [Blender 5.0 Operator API](https://docs.blender.org/api/5.0/bpy.types.Operator.html) documents modal operators and the event loop used for interactive input.
- [Blender 5.0 WorkSpaceTool API](https://docs.blender.org/api/5.0/bpy.types.WorkSpaceTool.html) documents workspace tool keymaps.
- [Blender Add-on Development Setup](https://developer.blender.org/docs/handbook/extensions/addon_dev_setup/) explains that Blender executes scripts in its bundled Python environment and that the IDE interpreter mainly affects language support.
