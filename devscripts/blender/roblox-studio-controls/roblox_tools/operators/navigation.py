import bpy
from math import asin
from mathutils import Quaternion, Vector
from time import monotonic
from ..core.preferences import get_pref


WORLD_UP = Vector((0.0, 0.0, 1.0))
MOVE_KEYS = {'W', 'A', 'S', 'D', 'E', 'Q'}
MODIFIER_KEYS = {'LEFT_SHIFT', 'RIGHT_SHIFT', 'LEFT_CTRL', 'RIGHT_CTRL'}
MAX_PITCH = 1.5533430342749532  # 89 degrees
MIN_VIEW_DISTANCE = 0.01


def _look_direction(rv3d):
    return rv3d.view_rotation @ Vector((0.0, 0.0, -1.0))


def _look_view(rv3d, dx, dy, sensitivity):
    if not dx and not dy:
        return
    original_rotation = rv3d.view_rotation.copy()
    distance = rv3d.view_distance
    original_forward = _look_direction(rv3d)
    camera_location = rv3d.view_location - original_forward * distance

    rotation = Quaternion(WORLD_UP, dx * sensitivity) @ original_rotation
    direction = _look_direction_from_rotation(rotation)
    current_elevation = asin(max(-1.0, min(1.0, direction.dot(WORLD_UP))))
    # Blender's window mouse Y coordinate increases upward. A positive delta
    # therefore raises the view, matching a physical upward mouse drag.
    elevation = max(-MAX_PITCH, min(MAX_PITCH,
                                    current_elevation + dy * sensitivity))
    right = rotation @ Vector((1.0, 0.0, 0.0))
    rotation = Quaternion(right, current_elevation - elevation) @ rotation

    rv3d.view_rotation = rotation
    new_forward = _look_direction(rv3d)
    rv3d.view_location = camera_location + new_forward * distance


def _move_view(rv3d, pressed, dt, fast, slow, context):
    rotation = rv3d.view_rotation
    forward = _look_direction(rv3d)
    right = rotation @ Vector((1.0, 0.0, 0.0))
    horizontal = Vector((0.0, 0.0, 0.0))
    if 'W' in pressed:
        horizontal += forward
    if 'S' in pressed:
        horizontal -= forward
    if 'D' in pressed:
        horizontal += right
    if 'A' in pressed:
        horizontal -= right
    vertical = 0.0
    if 'E' in pressed:
        vertical += 1.0
    if 'Q' in pressed:
        vertical -= 1.0
    speed = get_pref(context, "camera_move_speed")
    if fast:
        speed *= get_pref(context, "camera_fast_multiplier")
    if slow:
        speed *= get_pref(context, "camera_slow_multiplier")
    displacement = Vector((0.0, 0.0, 0.0))
    if horizontal.length_squared:
        displacement += horizontal.normalized() * speed
    if vertical:
        displacement += WORLD_UP * vertical * speed * get_pref(
            context, "camera_vertical_multiplier"
        )
    if displacement.length_squared:
        rv3d.view_location += displacement * dt


class ROTOOLS_OT_fly_navigation(bpy.types.Operator):
    """Hold RMB to look and use WASD/QE to fly in the 3D View"""
    bl_idname = "rotools.fly_navigation"
    bl_label = "RoTools Fly Navigation"
    bl_options = {'GRAB_CURSOR'}

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
        self.window.cursor_modal_set('NONE')
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def _finish(self, context):
        try:
            self.window.cursor_modal_set('DEFAULT')
        except (AttributeError, RuntimeError):
            pass
        if self.area is not None:
            self.area.tag_redraw()

    def _look(self, dx, dy, context):
        sensitivity = get_pref(context, "camera_look_sensitivity")
        _look_view(self.rv3d, dx, dy, sensitivity)

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
        if event.type == 'MOUSEMOVE':
            dx = event.mouse_x - self.last_x
            dy = event.mouse_y - self.last_y
            self.last_x = event.mouse_x
            self.last_y = event.mouse_y
            self._look(dx, dy, context)
            self.area.tag_redraw()
            return {'RUNNING_MODAL'}

        # Let the independent keyboard-movement modal operator receive keys.
        return {'PASS_THROUGH'}


class ROTOOLS_OT_camera_move(bpy.types.Operator):
    """Move the view while a Roblox Select camera key is held."""
    bl_idname = "rotools.camera_move"
    bl_label = "RoTools Camera Move"
    bl_options = {'INTERNAL'}

    @classmethod
    def poll(cls, context):
        return (context.area is not None and context.area.type == 'VIEW_3D'
                and context.region is not None and context.region.type == 'WINDOW'
                and context.region_data is not None
                and context.mode == 'OBJECT')

    def invoke(self, context, event):
        self.area = context.area
        self.rv3d = context.region_data
        self.window = context.window
        self.pressed = {event.type} if event.type in MOVE_KEYS else set()
        self.last_time = monotonic()
        self.timer = context.window_manager.event_timer_add(
            1.0 / 60.0, window=context.window
        )
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def _finish(self, context):
        self.pressed.clear()
        if self.timer is not None:
            context.window_manager.event_timer_remove(self.timer)
            self.timer = None
        if self.area is not None:
            self.area.tag_redraw()

    def modal(self, context, event):
        if context.window != self.window or context.area != self.area:
            self._finish(context)
            return {'CANCELLED'}
        if event.type in {'ESC', 'WINDOW_DEACTIVATE'}:
            self._finish(context)
            return {'CANCELLED'}

        if event.type in MOVE_KEYS:
            if event.value == 'PRESS':
                self.pressed.add(event.type)
            elif event.value == 'RELEASE':
                self.pressed.discard(event.type)
            if not self.pressed.intersection(MOVE_KEYS):
                self._finish(context)
                return {'FINISHED'}
            return {'RUNNING_MODAL'}

        if event.type in MODIFIER_KEYS:
            if event.value == 'PRESS':
                self.pressed.add(event.type)
            elif event.value == 'RELEASE':
                self.pressed.discard(event.type)
            return {'RUNNING_MODAL'}

        if event.type == 'TIMER':
            try:
                active_tool = context.workspace.tools.from_space_view3d_mode(
                    'OBJECT', create=False
                )
            except (AttributeError, TypeError, RuntimeError):
                active_tool = None
            if active_tool is None or active_tool.idname != 'rotools.select_tool':
                self._finish(context)
                return {'CANCELLED'}

            now = monotonic()
            dt = max(0.0, min(0.05, now - self.last_time))
            self.last_time = now
            fast = (event.shift or bool(
                self.pressed.intersection({'LEFT_SHIFT', 'RIGHT_SHIFT'})
            ))
            slow = (event.ctrl or bool(
                self.pressed.intersection({'LEFT_CTRL', 'RIGHT_CTRL'})
            ))
            _move_view(self.rv3d, self.pressed, dt, fast, slow, context)
            self.area.tag_redraw()
            return {'RUNNING_MODAL'}

        # Keep normal mouse actions available, including RoTools block dragging.
        return {'PASS_THROUGH'}


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
    bpy.utils.register_class(ROTOOLS_OT_camera_move)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_OT_camera_move)
    bpy.utils.unregister_class(ROTOOLS_OT_fly_dolly)
    bpy.utils.unregister_class(ROTOOLS_OT_fly_navigation)
