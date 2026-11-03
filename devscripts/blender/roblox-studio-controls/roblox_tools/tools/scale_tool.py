import bpy
from bpy.types import WorkSpaceTool

from ..ui.tool_ui import draw_orientation_row, draw_snap_row

DESCRIPTION = (
    "Scale objects like Roblox Studio (drag one of the six box handles; the "
    "part grows out of the face you drag, anchored on the opposite one)"
)
KEYMAP = (
    ("rotools.select", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
    ("rotools.toggle_orientation", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
    ("rotools.cycle_pivot", {"type": 'L', "value": 'PRESS', "ctrl": True, "shift": True}, None),
    ("rotools.duplicate", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
)


def _draw_settings(context, layout, tool):
    draw_orientation_row(context, layout)

    scene = context.scene
    row = layout.row(align=True)
    row.active = scene.rotools_pivot_mode != 'SWIVEL'
    row.prop(scene, "rotools_scale_pivot", expand=True)

    draw_snap_row(context, layout, "rotools_snap_scale")


class ROTOOLS_WT_scale(WorkSpaceTool):
    bl_space_type = 'VIEW_3D'
    bl_context_mode = 'OBJECT'

    bl_idname = "rotools.scale_tool"
    bl_label = "Roblox Scale"
    bl_description = DESCRIPTION
    bl_icon = "ops.transform.resize"
    bl_widget = "ROTOOLS_GGT_scale"
    bl_keymap = KEYMAP

    draw_settings = staticmethod(_draw_settings)


class ROTOOLS_WT_scale_edit(WorkSpaceTool):
    """Same tool as `ROTOOLS_WT_scale`, active in Edit Mesh instead of Object
    Mode - see `ROTOOLS_WT_move_edit` in tools/move_tool.py for why this needs
    to be a second class rather than one tool with two context modes.
    """
    bl_space_type = 'VIEW_3D'
    bl_context_mode = 'EDIT_MESH'

    bl_idname = "rotools.scale_tool"
    bl_label = "Roblox Scale"
    bl_description = DESCRIPTION
    bl_icon = "ops.transform.resize"
    bl_widget = "ROTOOLS_GGT_scale"
    bl_keymap = KEYMAP

    draw_settings = staticmethod(_draw_settings)


def register():
    bpy.utils.register_tool(ROTOOLS_WT_scale, after={"rotools.move_tool"})
    bpy.utils.register_tool(ROTOOLS_WT_scale_edit)


def unregister():
    bpy.utils.unregister_tool(ROTOOLS_WT_scale_edit)
    bpy.utils.unregister_tool(ROTOOLS_WT_scale)
