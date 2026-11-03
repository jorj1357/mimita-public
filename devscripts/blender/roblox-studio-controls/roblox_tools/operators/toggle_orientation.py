import bpy


class ROTOOLS_OT_toggle_orientation(bpy.types.Operator):
    """Toggle every RoTools tool between World and Local space (Roblox Studio: Ctrl+L)"""
    bl_idname = "rotools.toggle_orientation"
    bl_label = "Toggle Orientation"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        scene = context.scene
        scene.rotools_orientation = (
            'LOCAL' if scene.rotools_orientation == 'GLOBAL' else 'GLOBAL'
        )
        label = "Local" if scene.rotools_orientation == 'LOCAL' else "World"
        self.report({'INFO'}, f"Orientation: {label}")
        return {'FINISHED'}


class ROTOOLS_OT_cycle_pivot(bpy.types.Operator):
    """Step the pivot through Center, Origin and Swivel"""
    bl_idname = "rotools.cycle_pivot"
    bl_label = "Cycle Pivot"
    bl_options = {'REGISTER', 'UNDO'}

    ORDER = ('CENTER', 'ORIGIN', 'SWIVEL')

    def execute(self, context):
        scene = context.scene
        index = self.ORDER.index(scene.rotools_pivot_mode)
        nxt = self.ORDER[(index + 1) % len(self.ORDER)]
        # Skipping SWIVEL when nothing has been picked keeps the cycle honest:
        # it would otherwise land on a mode that silently behaves as Center.
        if nxt == 'SWIVEL' and not scene.rotools_swivel_is_set:
            nxt = self.ORDER[(index + 2) % len(self.ORDER)]
        scene.rotools_pivot_mode = nxt
        self.report({'INFO'}, f"Pivot: {nxt.title()}")
        return {'FINISHED'}


def register():
    bpy.utils.register_class(ROTOOLS_OT_toggle_orientation)
    bpy.utils.register_class(ROTOOLS_OT_cycle_pivot)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_OT_cycle_pivot)
    bpy.utils.unregister_class(ROTOOLS_OT_toggle_orientation)
