import bpy

from ..core.preferences import get_pref
from ..core.view_math import mouse_ray


class ROTOOLS_OT_select(bpy.types.Operator):
    """Roblox Studio style select: click, shift-add, ctrl-toggle, drag box-select"""
    bl_idname = "rotools.select"
    bl_label = "Roblox Select"
    bl_options = {'UNDO'}

    # Roblox's Select tool doubles as its dragger, so dragging off a part moves
    # it and dragging off empty space box-selects. Off by default: the Move /
    # Scale / Rotate tools bind this same operator for their click-select, and
    # there a body drag would fight their gizmos.
    allow_drag: bpy.props.BoolProperty(default=False, options={'HIDDEN'})

    def invoke(self, context, event):
        self.start = (event.mouse_region_x, event.mouse_region_y)
        self.shift = event.shift
        self.ctrl = event.ctrl
        self.dragging = False
        self.threshold = get_pref(context, "box_select_threshold")

        self.grab_object = self._object_under_mouse(context) if self.allow_drag else None

        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def _object_under_mouse(self, context):
        """What the press landed on, decided up front so the drag threshold can
        route to the dragger or to box-select without re-testing."""
        rv3d = context.region_data
        if rv3d is None:
            return None
        origin, direction = mouse_ray(context.region, rv3d, self.start)
        depsgraph = context.evaluated_depsgraph_get()
        hit, _location, _normal, _index, obj, _matrix = context.scene.ray_cast(
            depsgraph, origin, direction
        )
        if not hit or obj is None:
            return None
        # `scene.ray_cast` does not care about selectability, but the dragger
        # does - it cancels on anything it cannot select. Treating an
        # unselectable hit as a miss box-selects instead of eating the drag.
        if obj.hide_select:
            return None
        return obj

    def _box_select_mode(self):
        if self.shift and self.ctrl:
            return 'AND'
        if self.shift:
            return 'ADD'
        if self.ctrl:
            return 'SUB'
        return 'SET'

    def _start_drag(self, context):
        # Dragging a part that was not selected picks it up, matching Roblox -
        # shift keeps the rest of the selection along for the ride.
        if not self.grab_object.select_get():
            if not self.shift:
                for obj in context.selected_objects:
                    obj.select_set(False)
            self.grab_object.select_set(True)
            context.view_layer.objects.active = self.grab_object

        return bpy.ops.rotools.drag(
            'INVOKE_DEFAULT',
            start_x=self.start[0],
            start_y=self.start[1],
        )

    def modal(self, context, event):
        if event.type == 'MOUSEMOVE' and not self.dragging:
            dx = event.mouse_region_x - self.start[0]
            dy = event.mouse_region_y - self.start[1]
            if (dx * dx + dy * dy) > self.threshold * self.threshold:
                self.dragging = True
                # A refused drag (nothing draggable in the selection) falls
                # through to box-select rather than leaving a dead click.
                if self.grab_object is not None and 'CANCELLED' not in self._start_drag(context):
                    return {'FINISHED'}
                bpy.ops.view3d.select_box('INVOKE_DEFAULT', mode=self._box_select_mode())
                return {'FINISHED'}
            return {'RUNNING_MODAL'}

        if event.type == 'LEFTMOUSE' and event.value == 'RELEASE':
            if not self.dragging:
                bpy.ops.view3d.select(
                    'INVOKE_DEFAULT',
                    extend=self.shift,
                    toggle=self.ctrl,
                    deselect_all=not (self.shift or self.ctrl),
                )
            return {'FINISHED'}

        if event.type in {'RIGHTMOUSE', 'ESC'}:
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}


def register():
    bpy.utils.register_class(ROTOOLS_OT_select)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_OT_select)
