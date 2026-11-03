import bpy
from bpy.types import WorkSpaceTool

from ..ui.tool_ui import draw_orientation_row


class ROTOOLS_WT_select(WorkSpaceTool):
    bl_space_type = 'VIEW_3D'
    bl_context_mode = 'OBJECT'

    bl_idname = "rotools.select_tool"
    bl_label = "Roblox Select"
    bl_description = (
        "Select and drag objects like Roblox Studio (click / shift-add / "
        "ctrl-toggle / drag a part to move it across surfaces / drag empty "
        "space to box-select; hold WASD/QE to move the camera, RMB to look, "
        "and scroll to dolly)"
    )
    bl_icon = "ops.generic.select"
    bl_widget = None
    bl_keymap = (
        # allow_drag is what separates this tool from Move/Scale/Rotate, which
        # bind the same operator for plain click-select.
        (
            "rotools.select",
            {"type": 'LEFTMOUSE', "value": 'PRESS'},
            {"properties": [("allow_drag", True)]},
        ),
        ("rotools.toggle_orientation", {"type": 'L', "value": 'PRESS', "ctrl": True}, None),
        ("rotools.cycle_pivot", {"type": 'L', "value": 'PRESS', "ctrl": True, "shift": True}, None),
        ("rotools.duplicate", {"type": 'D', "value": 'PRESS', "ctrl": True}, None),
        ("rotools.fly_navigation", {"type": 'RIGHTMOUSE', "value": 'PRESS', "any": True}, None),
        # These tool-scoped bindings intentionally shadow Blender's A select-all
        # and W tool/menu shortcuts while Roblox Select is active.
        ("rotools.camera_move", {"type": 'W', "value": 'PRESS', "any": True}, None),
        ("rotools.camera_move", {"type": 'A', "value": 'PRESS', "any": True}, None),
        ("rotools.camera_move", {"type": 'S', "value": 'PRESS', "any": True}, None),
        ("rotools.camera_move", {"type": 'D', "value": 'PRESS', "any": True}, None),
        ("rotools.camera_move", {"type": 'Q', "value": 'PRESS', "any": True}, None),
        ("rotools.camera_move", {"type": 'E', "value": 'PRESS', "any": True}, None),
        ("rotools.fly_dolly", {"type": 'WHEELUPMOUSE', "value": 'PRESS'},
         {"properties": [("direction", 1.0)]}),
        ("rotools.fly_dolly", {"type": 'WHEELDOWNMOUSE', "value": 'PRESS'},
         {"properties": [("direction", -1.0)]}),
    )

    def draw_settings(context, layout, tool):
        scene = context.scene

        row = layout.row(align=True)
        row.prop(scene, "rotools_drag_grid_snap", text="Grid", toggle=True, icon='SNAP_INCREMENT')
        sub = row.row(align=True)
        sub.active = scene.rotools_drag_grid_snap
        sub.prop(scene, "rotools_drag_grid_size", text="")

        row = layout.row(align=True)
        row.prop(scene, "rotools_drag_soft_snap", text="Soft Snap", toggle=True, icon='SNAP_VERTEX')
        # Soft snap is the grid's fallback, not its peer: resolve_snap takes the
        # grid outright when it is on. Greying it out says so without lying about
        # the toggle's stored value.
        row.active = not scene.rotools_drag_grid_snap
        row = layout.row(align=True)
        row.prop(scene, "rotools_drag_surface_align", text="Align", toggle=True, icon='SNAP_NORMAL')

        row = layout.row(align=True)
        row.prop(scene, "rotools_drag_use_ground", text="Ground", toggle=True, icon='MESH_PLANE')
        sub = row.row(align=True)
        sub.active = scene.rotools_drag_use_ground
        sub.prop(scene, "rotools_drag_ground_z", text="")

        draw_orientation_row(context, layout)


def register():
    bpy.utils.register_tool(ROTOOLS_WT_select, after={"builtin.select_box"}, separator=True, group=True)


def unregister():
    bpy.utils.unregister_tool(ROTOOLS_WT_select)

