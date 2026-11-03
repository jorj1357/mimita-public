"""Viewport overlay for the swivel pivot.

Two things get drawn, both from one `POST_VIEW` handler on `SpaceView3D`:

  * the **swivel marker** - a screen-constant axis cross plus a dot at the
    picked point - whenever the pivot mode is SWIVEL and a point has been set,
  * the **live pick preview** while `rotools.set_swivel` is running, including
    the edge or face loop the cursor is currently over, so it is obvious *which*
    element a click would take.

Screen-constant means the cross arms are sized through
`core/view_math.pixels_to_world`, per this project's rule that no pixel
threshold ever becomes a flat world-space number.

Uniform names for the two builtin shaders used here are from the 5.2 API
reference: `POLYLINE_UNIFORM_COLOR` takes `viewportSize`, `lineWidth`, `color`;
`POINT_UNIFORM_COLOR` takes `color`, `size`. Both take a `vec3 pos` attribute.
"""

import bpy
import gpu
from gpu_extras.batch import batch_for_shader

from ..core.gizmo_common import AXIS_COLORS, HIGHLIGHT_COLOR
from ..core.preferences import get_pref
from ..core.view_math import pixels_to_world

# The overlay is a RoTools thing; it should not litter Blender's own tools.
ROTOOLS_TOOL_IDS = {
    "rotools.select_tool",
    "rotools.move_tool",
    "rotools.scale_tool",
    "rotools.rotate_tool",
}

PREVIEW_COLOR = HIGHLIGHT_COLOR + (1.0,)
MARKER_ALPHA = 0.9
LINE_WIDTH = 2.0

_handle = None
# Set by `rotools.set_swivel` while its modal is running; a `PickResult` or None.
_preview = None
# The active tool's idname as of the last redraw. A sentinel (not None) so the
# first redraw after registration never counts as a change - only an actual
# switch should drop the swivel.
_UNOBSERVED = object()
_last_tool_idname = _UNOBSERVED


def set_preview(pick):
    global _preview
    _preview = pick
    _tag_redraw()


def clear_preview():
    set_preview(None)


def _tag_redraw():
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type == 'VIEW_3D':
                area.tag_redraw()


def _active_tool_idname(context):
    tool = context.workspace.tools.from_space_view3d_mode(context.mode, create=False)
    return tool.idname if tool is not None else None


def _rotools_tool_active(context):
    return _active_tool_idname(context) in ROTOOLS_TOOL_IDS


def _clear_swivel_on_tool_change(context):
    """Swivel only means anything while Rotate is active - see set_swivel.py -
    so any switch away from (or between) tools drops it rather than leaving a
    stale pick that silently applies if the user swivels back to Rotate later.
    """
    global _last_tool_idname
    idname = _active_tool_idname(context)
    if _last_tool_idname is _UNOBSERVED:
        _last_tool_idname = idname
        return
    if idname == _last_tool_idname:
        return
    _last_tool_idname = idname

    scene = context.scene
    if not scene.rotools_swivel_is_set:
        return
    scene.rotools_swivel_is_set = False
    scene.rotools_swivel_kind = ""
    if scene.rotools_pivot_mode == 'SWIVEL':
        scene.rotools_pivot_mode = 'CENTER'


def _draw_lines(region, segments, color):
    if not segments:
        return
    shader = gpu.shader.from_builtin('POLYLINE_UNIFORM_COLOR')
    shader.uniform_float("viewportSize", (region.width, region.height))
    shader.uniform_float("lineWidth", LINE_WIDTH)
    shader.uniform_float("color", color)
    batch_for_shader(shader, 'LINES', {"pos": segments}).draw(shader)


def _draw_points(points, color, size):
    if not points:
        return
    shader = gpu.shader.from_builtin('POINT_UNIFORM_COLOR')
    shader.uniform_float("color", color)
    shader.uniform_float("size", size)
    batch_for_shader(shader, 'POINTS', {"pos": points}).draw(shader)


def _axis_cross(region, rv3d, point, rotation_3x3, pixels):
    """Six segment endpoints: a screen-constant cross on the frame's own axes."""
    arm = pixels_to_world(region, rv3d, point, pixels)
    segments = []
    for i in range(3):
        axis = rotation_3x3.col[i].normalized() * arm
        segments.append(point - axis)
        segments.append(point + axis)
    return segments


def _loop_segments(loop):
    """A closed line loop as flat LINES pairs."""
    if len(loop) < 2:
        return []
    segments = []
    for i in range(len(loop)):
        segments.append(loop[i])
        segments.append(loop[(i + 1) % len(loop)])
    return segments


def _draw():
    context = bpy.context
    region = context.region
    rv3d = context.region_data
    if region is None or rv3d is None:
        return

    _clear_swivel_on_tool_change(context)
    if not _rotools_tool_active(context):
        return

    scene = context.scene
    pixels = get_pref(context, "swivel_marker_size")

    gpu.state.blend_set('ALPHA')
    # Drawn through geometry, like Blender's own 3D cursor: a pivot you cannot
    # see because it is inside the part is worse than one that floats over it.
    gpu.state.depth_test_set('NONE')
    try:
        if _preview is not None:
            _draw_lines(region, _loop_segments(_preview.span), PREVIEW_COLOR)
            _draw_points([_preview.point], PREVIEW_COLOR, pixels + 4)
            _draw_marker(region, rv3d, context, _preview.point, pixels)
        elif scene.rotools_pivot_mode == 'SWIVEL' and scene.rotools_swivel_is_set:
            _draw_marker(region, rv3d, context, scene.rotools_swivel_point, pixels)
    finally:
        gpu.state.depth_test_set('LESS_EQUAL')
        gpu.state.blend_set('NONE')


def _draw_marker(region, rv3d, context, point, pixels):
    from mathutils import Vector

    from ..core.gizmo_common import orientation_frame

    point = Vector(point)
    rotation_3x3, _axis_rotations, _orient_type = orientation_frame(context)
    segments = _axis_cross(region, rv3d, point, rotation_3x3, pixels)
    for i in range(3):
        color = AXIS_COLORS['XYZ'[i]] + (MARKER_ALPHA,)
        _draw_lines(region, segments[i * 2:i * 2 + 2], color)
    _draw_points([point], HIGHLIGHT_COLOR + (1.0,), pixels * 0.7)


def register():
    global _handle
    _handle = bpy.types.SpaceView3D.draw_handler_add(_draw, (), 'WINDOW', 'POST_VIEW')


def unregister():
    global _handle, _preview, _last_tool_idname
    _preview = None
    _last_tool_idname = _UNOBSERVED
    if _handle is not None:
        bpy.types.SpaceView3D.draw_handler_remove(_handle, 'WINDOW')
        _handle = None
