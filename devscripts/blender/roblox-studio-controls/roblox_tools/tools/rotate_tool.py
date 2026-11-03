import bpy
from bpy.types import WorkSpaceTool

from ..ui.tool_ui import draw_orientation_row, draw_snap_row, draw_swivel_row

DESCRIPTION = (
    "Rotate objects like Roblox Studio (drag a ring to rotate around that "
    "axis, in 15 degree steps by default)"
)
KEYMAP = (
    ("rotools.select", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
    ("rotools.toggle_orientation", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
    ("rotools.cycle_pivot", {"type": 'L', "value": 'PRESS', "ctrl": True, "shift": True}, None),
    ("rotools.set_swivel", {"type": 'V', "value": 'PRESS'}, None),
    ("rotools.duplicate", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
)


def _draw_settings(context, layout, tool):
    draw_orientation_row(context, layout)
    draw_swivel_row(context, layout)
    draw_snap_row(context, layout, "rotools_snap_rotate", elements=False)


class ROTOOLS_WT_rotate(WorkSpaceTool):
    bl_space_type = 'VIEW_3D'
    bl_context_mode = 'OBJECT'

    bl_idname = "rotools.rotate_tool"
    bl_label = "Roblox Rotate"
    bl_description = DESCRIPTION
    bl_icon = "ops.transform.rotate"
    bl_widget = "ROTOOLS_GGT_rotate"
    bl_keymap = KEYMAP

    draw_settings = staticmethod(_draw_settings)


class ROTOOLS_WT_rotate_edit(WorkSpaceTool):
    """Same tool as `ROTOOLS_WT_rotate`, active in Edit Mesh instead of Object
    Mode - see `ROTOOLS_WT_move_edit` in tools/move_tool.py for why this needs
    to be a second class rather than one tool with two context modes.

    `rotools.set_swivel` stays in the keymap even though its `poll` is still
    Object-Mode-only (core/operators/set_swivel.py) - same precedent as
    `rotools.duplicate` in `ROTOOLS_WT_move_edit`: an inactive keymap entry,
    not a broken one, until swivel-picking grows Edit Mesh support of its own.
    """
    bl_space_type = 'VIEW_3D'
    bl_context_mode = 'EDIT_MESH'

    bl_idname = "rotools.rotate_tool"
    bl_label = "Roblox Rotate"
    bl_description = DESCRIPTION
    bl_icon = "ops.transform.rotate"
    bl_widget = "ROTOOLS_GGT_rotate"
    bl_keymap = KEYMAP

    draw_settings = staticmethod(_draw_settings)


def register():
    bpy.utils.register_tool(ROTOOLS_WT_rotate, after={"rotools.scale_tool"})
    bpy.utils.register_tool(ROTOOLS_WT_rotate_edit)


def unregister():
    bpy.utils.unregister_tool(ROTOOLS_WT_rotate_edit)
    bpy.utils.unregister_tool(ROTOOLS_WT_rotate)
