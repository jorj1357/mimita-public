"""The Move tool, registered once per context mode it supports.

`WorkSpaceTool.bl_context_mode` is used by Blender's own `register_tool` as a
plain dict key (`cls._tools[context_mode]`) into a per-mode tool bucket - it
does not accept a tuple of modes (verified: passing one raises
`KeyError: ('OBJECT', 'EDIT_MESH')` immediately). So "the same tool in two
modes" means two classes sharing one `bl_idname`, each registered under its
own `bl_context_mode` - confirmed live to resolve as the same tool activating
in both Object and Edit Mesh.
"""

import bpy
from bpy.types import WorkSpaceTool

from ..ui.tool_ui import draw_orientation_row, draw_snap_row

DESCRIPTION = (
    "Move objects like Roblox Studio (push one of the six arrows sitting on "
    "the part's faces, or drag the centre ring for a free move)"
)
KEYMAP = (
    ("rotools.select", {"type": 'LEFTMOUSE', "value": 'PRESS'}, None),
    ("rotools.toggle_orientation", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
    ("rotools.cycle_pivot", {"type": 'L', "value": 'PRESS', "ctrl": True, "shift": True}, None),
    ("rotools.duplicate", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
)


def _draw_settings(context, layout, tool):
    draw_orientation_row(context, layout)
    draw_snap_row(context, layout, "rotools_snap_move")


class ROTOOLS_WT_move(WorkSpaceTool):
    bl_space_type = 'VIEW_3D'
    bl_context_mode = 'OBJECT'

    bl_idname = "rotools.move_tool"
    bl_label = "Roblox Move"
    bl_description = DESCRIPTION
    bl_icon = "ops.transform.translate"
    bl_widget = "ROTOOLS_GGT_move"
    bl_keymap = KEYMAP

    draw_settings = staticmethod(_draw_settings)


class ROTOOLS_WT_move_edit(WorkSpaceTool):
    """Same tool as `ROTOOLS_WT_move`, active in Edit Mesh instead of Object
    Mode. Move-in-Edit-Mesh v1 dragging is gizmo-only (the arrows/centre ring
    drive Blender's native `transform.translate`, which already works on a
    bmesh selection) - `rotools.select`'s body-click drag stays Object-Mode-only
    for now, so this tool's keymap omits nothing `ROTOOLS_WT_move` has that
    would need Edit Mesh awareness beyond what the gizmo group already
    provides (`gizmos/move_gizmo.py`).
    """
    bl_space_type = 'VIEW_3D'
    bl_context_mode = 'EDIT_MESH'

    bl_idname = "rotools.move_tool"
    bl_label = "Roblox Move"
    bl_description = DESCRIPTION
    bl_icon = "ops.transform.translate"
    bl_widget = "ROTOOLS_GGT_move"
    bl_keymap = KEYMAP

    draw_settings = staticmethod(_draw_settings)


def register():
    bpy.utils.register_tool(ROTOOLS_WT_move, after={"rotools.select_tool"})
    bpy.utils.register_tool(ROTOOLS_WT_move_edit)


def unregister():
    bpy.utils.unregister_tool(ROTOOLS_WT_move_edit)
    bpy.utils.unregister_tool(ROTOOLS_WT_move)
