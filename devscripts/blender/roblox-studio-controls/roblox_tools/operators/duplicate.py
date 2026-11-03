"""Ctrl+D duplicate that drags the copy, Roblox Studio style.

A plain `object.duplicate_move` drops the copy with a free, unsnapped grab.
This instead makes a copy in place, selects it, and hands straight into
`rotools.drag` from the cursor's current position - so the copy rests on
surfaces and snaps to the grid exactly like a normal drag, rather than
floating wherever the mouse happens to move it.

If the cursor is not over anything draggable (`rotools.drag` needs a raycast
hit on a selected object), there is nothing to drag onto, so this falls back
to Blender's own `object.duplicate_move`.
"""

import bpy

from ..core.view_math import mouse_ray


def _duplicate_objects(objects):
    """Copy `objects` (and their data), keeping copy-to-copy parenting.

    Mirrors `ROTOOLS_OT_drag._stamp` - a full copy, with parents re-pointed to
    the copied parent rather than the original, so the duplicate is
    self-contained.
    """
    copies = {}
    for obj in objects:
        copy = obj.copy()
        if obj.data is not None:
            copy.data = obj.data.copy()
        for collection in obj.users_collection:
            collection.objects.link(copy)
        copies[obj] = copy

    for obj, copy in copies.items():
        if obj.parent in copies:
            copy.parent = copies[obj.parent]

    return copies


class ROTOOLS_OT_duplicate(bpy.types.Operator):
    """Duplicate the selection and drag the copy, Roblox Studio style"""
    bl_idname = "rotools.duplicate"
    bl_label = "Roblox Duplicate"
    bl_options = {'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.mode == 'OBJECT' and context.selected_objects

    def invoke(self, context, event):
        objects = list(context.selected_objects)

        region = context.region
        rv3d = context.region_data
        coord = (event.mouse_region_x, event.mouse_region_y)

        hit_obj = None
        if rv3d is not None:
            origin, direction = mouse_ray(region, rv3d, coord)
            depsgraph = context.evaluated_depsgraph_get()
            hit, _location, _normal, _index, obj, _matrix = context.scene.ray_cast(
                depsgraph, origin, direction
            )
            if hit:
                hit_obj = obj

        if hit_obj is None or not hit_obj.select_get():
            return bpy.ops.object.duplicate_move('INVOKE_DEFAULT')

        copies = _duplicate_objects(objects)

        active = context.view_layer.objects.active
        for obj in objects:
            obj.select_set(False)
        for copy in copies.values():
            copy.select_set(True)
        context.view_layer.objects.active = copies.get(active, next(iter(copies.values())))

        return bpy.ops.rotools.drag('INVOKE_DEFAULT', start_x=coord[0], start_y=coord[1])


def register():
    bpy.utils.register_class(ROTOOLS_OT_duplicate)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_OT_duplicate)
