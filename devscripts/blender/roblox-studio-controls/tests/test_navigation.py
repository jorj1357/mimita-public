"""Blender 5.0 registration and camera-math checks.

Run with:
  blender --background --factory-startup --python tests/test_navigation.py

These checks do not replace the live viewport input check.
"""

import math
import sys
from pathlib import Path
from types import SimpleNamespace

import bpy
from mathutils import Quaternion, Vector

ADDON_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ADDON_ROOT))
for module_name in tuple(sys.modules):
    if module_name == "roblox_tools" or module_name.startswith("roblox_tools."):
        del sys.modules[module_name]

import roblox_tools
from roblox_tools.operators.navigation import _look_direction, _look_view, _move_view
from roblox_tools.tools.select_tool import ROTOOLS_WT_select


class FakeRegionView3D:
    def __init__(self):
        # Looking horizontally toward -Y gives a clear world-up and camera-right axis.
        self.view_rotation = Quaternion((1.0, 0.0, 0.0), -math.pi / 2.0)
        self.view_location = Vector((0.0, 0.0, 0.0))
        self.view_distance = 10.0


def close(actual, expected, tolerance=1e-4):
    return (Vector(actual) - Vector(expected)).length <= tolerance


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print("PASS:", message)


roblox_tools.register()
try:
    check(hasattr(bpy.ops.rotools, "camera_move"), "camera movement operator registers")
    check(hasattr(bpy.ops.rotools, "fly_navigation"), "RMB look operator registers")

    bound = {item[1]["type"] for item in ROTOOLS_WT_select.bl_keymap}
    for key in ("W", "A", "S", "D", "Q", "E"):
        check(key in bound, f"Roblox Select binds {key}")
    check(any(item[0] == "rotools.select" and item[1]["type"] == "LEFTMOUSE"
              for item in ROTOOLS_WT_select.bl_keymap),
          "original left-click/drag selection binding remains")

    class EmptyAddons(dict):
        def get(self, key, default=None):
            return None

    fake_context = SimpleNamespace(
        preferences=SimpleNamespace(addons=EmptyAddons())
    )

    rv3d = FakeRegionView3D()
    original_location = rv3d.view_location.copy()
    original_forward = _look_direction(rv3d).copy()
    original_right = rv3d.view_rotation @ Vector((1.0, 0.0, 0.0))
    _look_view(rv3d, 300, 0, 0.0025)
    turned_forward = _look_direction(rv3d)
    check(turned_forward.dot(original_right) > 0.5, "+300 px horizontal delta looks right")
    check(close(rv3d.view_location - _look_direction(rv3d) * rv3d.view_distance,
                original_location - original_forward * rv3d.view_distance),
          "RMB look math keeps the camera position fixed")

    rv3d = FakeRegionView3D()
    _look_view(rv3d, 0, 300, 0.0025)
    positive_y_forward = _look_direction(rv3d)
    check(positive_y_forward.z > 0.5, "+300 event-Y delta looks up")
    rv3d = FakeRegionView3D()
    _look_view(rv3d, 0, -300, 0.0025)
    negative_y_forward = _look_direction(rv3d)
    check(negative_y_forward.z < -0.5, "-300 event-Y delta looks down")

    expected = {
        "W": (0.0, -80.0, 0.0),
        "A": (-80.0, 0.0, 0.0),
        "S": (0.0, 80.0, 0.0),
        "D": (80.0, 0.0, 0.0),
        "Q": (0.0, 0.0, -800.0),
        "E": (0.0, 0.0, 800.0),
    }
    for key, displacement in expected.items():
        rv3d = FakeRegionView3D()
        _move_view(rv3d, {key}, 1.0, False, False, fake_context)
        check(close(rv3d.view_location, displacement), f"{key} moves in the expected direction and speed")

    rv3d = FakeRegionView3D()
    _move_view(rv3d, {"W"}, 1.0, True, False, fake_context)
    check(close(rv3d.view_location, (0.0, -320.0, 0.0)), "Shift applies the 4x speed multiplier")
    rv3d = FakeRegionView3D()
    _move_view(rv3d, {"W"}, 1.0, False, True, fake_context)
    check(close(rv3d.view_location, (0.0, -20.0, 0.0)), "Ctrl applies the 0.25x slow multiplier")
finally:
    roblox_tools.unregister()

check(not hasattr(bpy.types, "ROTOOLS_OT_camera_move"),
      "camera movement operator unregisters cleanly")
check(not hasattr(bpy.types, "ROTOOLS_OT_fly_navigation"),
      "RMB look operator unregisters cleanly")

print("BLENDER_CAMERA_CHECKS: PASS")
