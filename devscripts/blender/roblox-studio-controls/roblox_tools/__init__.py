bl_info = {
    "name": "RoTools - Roblox Studio Style Tools",
    "author": "RoTools",
    "version": (0, 5, 4),
    "blender": (4, 0, 0),
    "location": "View3D > Toolbar",
    "description": "Roblox Studio style tools with viewport camera controls and continuous dolly",
    "category": "3D View",
}

import bpy

from .operators import select as op_select
from .operators import drag as op_drag
from .operators import duplicate as op_duplicate
from .operators import toggle_orientation as op_toggle_orientation
from .operators import set_swivel as op_set_swivel
from .operators import switch_tool as op_switch_tool
from .operators import navigation as op_navigation
from .tools import select_tool, move_tool, scale_tool, rotate_tool
from .gizmos import move_gizmo, scale_gizmo, rotate_gizmo
from .core import preferences, scene_state, keymaps
from .ui import overlay

MODULES = (
    preferences,
    scene_state,
    op_select,
    op_drag,
    op_duplicate,
    op_toggle_orientation,
    op_set_swivel,
    op_switch_tool,
    op_navigation,
    move_gizmo,
    scale_gizmo,
    rotate_gizmo,
    overlay,
    select_tool,
    move_tool,
    scale_tool,
    rotate_tool,
    keymaps,
)


def register():
    for m in MODULES:
        m.register()


def unregister():
    for m in reversed(MODULES):
        m.unregister()


if __name__ == "__main__":
    register()




