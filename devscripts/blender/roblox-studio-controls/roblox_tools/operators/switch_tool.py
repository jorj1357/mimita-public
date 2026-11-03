import bpy


class ROTOOLS_OT_switch_tool(bpy.types.Operator):
    """Switch to a RoTools workspace tool (Select/Move/Scale/Rotate)"""
    bl_idname = "rotools.switch_tool"
    bl_label = "Switch RoTools Tool"
    bl_options = {'UNDO'}

    tool_id: bpy.props.StringProperty()

    def execute(self, context):
        bpy.ops.wm.tool_set_by_id(name=self.tool_id)
        return {'FINISHED'}


def register():
    bpy.utils.register_class(ROTOOLS_OT_switch_tool)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_OT_switch_tool)
