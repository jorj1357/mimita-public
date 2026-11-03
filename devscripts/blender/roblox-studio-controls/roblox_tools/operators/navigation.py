import bpy
from math import asin
from mathutils import Quaternion, Vector
from time import monotonic
from ..core.preferences import get_pref


WORLD_UP = Vector((0.0, 0.0, 1.0))
MOVE_KEYS = {'W', 'A', 'S', 'D', 'E', 'Q'}
MAX_PITCH = 1.5533430342749532  # 89 degrees
MIN_VIEW_DISTANCE = 0.01


def _look_direction(rv3d):
    return rv3d.view_rotation @ Vector((0.0, 0.0, -1.0))


class ROTOOLS_OT_fly_navigation(bpy.types.Operator):
    """Hold RMB to look and use WASD/QE to fly in the 3D View"""
    bl_idname = "rotools.fly_navigation"
    bl_label = "RoTools Fly Navigation"
    bl_options = {'GRAB_CURSOR', 'BLOCKING'}

    @classmethod
    def poll(cls, context):
        return (context.area is not None and context.area.type == 'VIEW_3D'
                and context.region is not None and context.region.type == 'WINDOW'
                and context.region_data is not None)

    def invoke(self, context, event):
        self.area = context.area
        self.rv3d = context.region_data
        self.window = context.window
        self.pressed = set()
        self.last_x = event.mouse_x
        self.last_y = event.mouse_y
        self.last_time = monotonic()
        self.timer = context.window_manager.event_timer_add(
            1.0 / 60.0, window=context.window
        )
        self.window.cursor_modal_set('NONE')
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def _finish(self, context):
        if self.timer is not None:
            context.window_manager.event_timer_remove(self.timer)
            self.timer = None
        try:
            self.window.cursor_modal_set('DEFAULT')
        except (AttributeError, RuntimeError):
            pass
        if self.area is not None:
            self.area.tag_redraw()

    def _look(self, dx, dy, context):
        if not dx and not dy:
            return
        original_rotation = self.rv3d.view_rotation.copy()
        distance = self.rv3d.view_distance
        original_forward = _look_direction(self.rv3d)
        camera_location = self.rv3d.view_location - original_forward * distance

        sensitivity = get_pref(context, "camera_look_sensitivity")
        rotation = Quaternion(WORLD_UP, dx * sensitivity) @ original_rotation
        direction = _look_direction_from_rotation(rotation)
        current_elevation = asin(max(-1.0, min(1.0, direction.dot(WORLD_UP))))
        elevation = max(-MAX_PITCH, min(MAX_PITCH,
                                        current_elevation - dy * sensitivity))
        right = rotation @ Vector((1.0, 0.0, 0.0))
        rotation = Quaternion(right, elevation - current_elevation) @ rotation

        self.rv3d.view_rotation = rotation
        new_forward = _look_direction(self.rv3d)
        self.rv3d.view_location = camera_location + new_forward * distance

    def _move(self, dt, fast, context):
        rotation = self.rv3d.view_rotation
        forward = _look_direction(self.rv3d)
        right = rotation @ Vector((1.0, 0.0, 0.0))
        horizontal = Vector((0.0, 0.0, 0.0))
        if 'W' in self.pressed:
            horizontal += forward
        if 'S' in self.pressed:
            horizontal -= forward
        if 'D' in self.pressed:
            horizontal += right
        if 'A' in self.pressed:
            horizontal -= right
        vertical = 0.0
        if 'E' in self.pressed:
            vertical += 1.0
        if 'Q' in self.pressed:
            vertical -= 1.0
        speed = get_pref(context, "camera_move_speed")
        if fast:
            speed *= get_pref(context, "camera_fast_multiplier")
        displacement = Vector((0.0, 0.0, 0.0))
        if horizontal.length_squared:
            displacement += horizontal.normalized() * speed
        if vertical:
            displacement += WORLD_UP * vertical * speed * get_pref(
                context, "camera_vertical_multiplier"
            )
        if displacement.length_squared:
            self.rv3d.view_location += displacement * dt

    def modal(self, context, event):
        if context.window != self.window or context.area != self.area:
            self._finish(context)
            return {'CANCELLED'}

        if event.type == 'RIGHTMOUSE' and event.value == 'RELEASE':
            self._finish(context)
            return {'FINISHED'}
        if event.type in {'ESC', 'WINDOW_DEACTIVATE'}:
            self._finish(context)
            return {'CANCELLED'}

        if event.type in MOVE_KEYS:
            if event.value == 'PRESS':
                self.pressed.add(event.type)
            elif event.value == 'RELEASE':
                self.pressed.discard(event.type)

        if event.type == 'MOUSEMOVE':
            dx = event.mouse_x - self.last_x
            dy = event.mouse_y - self.last_y
            self.last_x = event.mouse_x
            self.last_y = event.mouse_y
            self._look(dx, dy, context)
        elif event.type == 'TIMER':
            now = monotonic()
            dt = max(0.0, min(0.05, now - self.last_time))
            self.last_time = now
            self._move(dt, event.shift, context)

        self.area.tag_redraw()
        return {'RUNNING_MODAL'}


class ROTOOLS_OT_fly_dolly(bpy.types.Operator):
    """Move along the view direction without zooming toward the cursor."""
    bl_idname = "rotools.fly_dolly"
    bl_label = "RoTools Fly Dolly"
    bl_options = {'INTERNAL'}

    direction: bpy.props.FloatProperty(default=1.0, options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        return (context.area is not None and context.area.type == 'VIEW_3D'
                and context.region_data is not None)

    def execute(self, context):
        rv3d = context.region_data
        forward = _look_direction(rv3d)
        distance = max(MIN_VIEW_DISTANCE, rv3d.view_distance)
        step = max(get_pref(context, "camera_min_dolly_step"),
                   distance * get_pref(context, "camera_dolly_factor"))
        if self.direction > 0.0:
            shortened = min(step, max(0.0, distance - MIN_VIEW_DISTANCE))
            rv3d.view_distance = max(MIN_VIEW_DISTANCE, distance - shortened)
            remainder = step - shortened
            if remainder > 0.0:
                rv3d.view_location += forward * remainder
        else:
            rv3d.view_distance = distance + step
        context.area.tag_redraw()
        return {'FINISHED'}


def _look_direction_from_rotation(rotation):
    return rotation @ Vector((0.0, 0.0, -1.0))


def register():
    bpy.utils.register_class(ROTOOLS_OT_fly_navigation)
    bpy.utils.register_class(ROTOOLS_OT_fly_dolly)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_OT_fly_dolly)
    bpy.utils.unregister_class(ROTOOLS_OT_fly_navigation)
