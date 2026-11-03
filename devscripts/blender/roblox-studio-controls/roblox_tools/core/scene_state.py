"""Scene-level settings shared by every RoTools tool.

These live on the Scene rather than in addon preferences because they describe
the thing being built - grid size, ground height, which pivot the handles hang
off - not how the user likes the tool to feel.

`rotools_orientation` and `rotools_pivot_mode` are deliberately *shared* across
Move / Scale / Rotate rather than per-tool: Roblox Studio has one Local-space
toggle in the Model tab that every transform tool obeys.

The swivel point itself (`rotools_swivel_*`) is the one exception: it can only
be picked while Rotate is active, and `ui/overlay.py` drops it on any tool
change, so it never survives into Move/Scale/Select - see
`operators/set_swivel.py`.
"""

import bpy
from math import radians

ORIENTATION_ITEMS = (
    ('GLOBAL', "World", "Transform along the world X/Y/Z axes", 'ORIENTATION_GLOBAL', 0),
    ('LOCAL', "Local", "Transform along the active object's own rotated X/Y/Z axes", 'ORIENTATION_LOCAL', 1),
)

PIVOT_ITEMS = (
    ('CENTER', "Center",
     "Pivot on the centre of the selection's bounding box, the way a Roblox part pivots on its own centre",
     'PIVOT_BOUNDBOX', 0),
    ('ORIGIN', "Origin", "Pivot on the median of the selected objects' origins",
     'PIVOT_MEDIAN', 1),
    ('SWIVEL', "Swivel", "Pivot on a vertex, edge or face picked with Set Swivel",
     'PIVOT_CURSOR', 2),
)

SWIVEL_ELEMENT_ITEMS = (
    ('AUTO', "Auto", "Snap to whichever of vertex / edge / face is nearest the cursor",
     'EYEDROPPER', 0),
    ('VERTEX', "Vertex", "Snap to the nearest corner of the face under the cursor",
     'SNAP_VERTEX', 1),
    ('EDGE', "Edge", "Snap to the midpoint of the nearest edge of the face under the cursor",
     'SNAP_EDGE', 2),
    ('FACE', "Face", "Snap to the centre of the face under the cursor",
     'SNAP_FACE', 3),
)

SCALE_PIVOT_ITEMS = (
    ('OPPOSITE_FACE', "Opposite Face", "Grow from the opposite face/edge, like Roblox Studio's Scale tool"),
    ('CENTER', "Center", "Scale evenly around the pivot"),
)

# Blender's own default for ToolSettings.snap_angle_increment_3d, verified
# against its RNA (0.0872665 rad = 5 degrees). Only a scene still sitting on
# this value gets nudged to Roblox's 15 degrees - see _set_default_rotate_increment.
BLENDER_DEFAULT_ANGLE_INCREMENT = radians(5)
ROBLOX_ANGLE_INCREMENT = radians(15)

# Names of scenes this addon nudged from the Blender default to the Roblox one,
# so unregister() can revert exactly those - not scenes the user set to 15
# degrees themselves. See _set_default_rotate_increment / unregister.
_nudged_scene_names = set()


# --- transform-snap proxies -------------------------------------------------
# Blender splits transform snapping in two: `use_snap` is the master ("Snap
# during transform") and `use_snap_translate` / `use_snap_rotate` /
# `use_snap_scale` say which modes obey it. Verified in 5.2: the two rotate/scale
# flags default to **False**, so a tool row that exposes only `use_snap_rotate`
# (as the Rotate tool's did) toggles a flag that does nothing while the master is
# off, and one that exposes only `use_snap` (as Scale's did) leaves scaling
# unsnapped. Roblox has one Snap button per tool, so these proxies present one:
# reading true only when the transform will really snap, and writing both.


def _snap_getter(flag):
    def get(self):
        ts = self.tool_settings
        return ts.use_snap and getattr(ts, flag)
    return get


def _snap_setter(flag):
    def set(self, value):
        ts = self.tool_settings
        setattr(ts, flag, value)
        # Switching a tool's snap on implies the master; switching it off leaves
        # the master alone so the other tools keep theirs.
        if value:
            ts.use_snap = True
    return set


def _snap_proxy(flag, description):
    return bpy.props.BoolProperty(
        name="Snap",
        description=description,
        get=_snap_getter(flag),
        set=_snap_setter(flag),
    )


def register():
    bpy.types.Scene.rotools_orientation = bpy.props.EnumProperty(
        name="Orientation",
        description="Axis frame every RoTools transform handle uses",
        items=ORIENTATION_ITEMS,
        default='GLOBAL',
    )
    bpy.types.Scene.rotools_pivot_mode = bpy.props.EnumProperty(
        name="Pivot",
        description="What the Move / Scale / Rotate handles hang off, and what "
                    "rotation and scaling are anchored to",
        items=PIVOT_ITEMS,
        default='CENTER',
    )
    bpy.types.Scene.rotools_scale_pivot = bpy.props.EnumProperty(
        name="Scale Anchor",
        items=SCALE_PIVOT_ITEMS,
        default='OPPOSITE_FACE',
    )

    # --- swivel -----------------------------------------------------------
    # A picked point on some piece of geometry that Move / Scale / Rotate can
    # pivot around, set by `rotools.set_swivel`.
    bpy.types.Scene.rotools_swivel_element = bpy.props.EnumProperty(
        name="Swivel Element",
        description="Which mesh element Set Swivel snaps the picked point to",
        items=SWIVEL_ELEMENT_ITEMS,
        default='AUTO',
    )
    bpy.types.Scene.rotools_swivel_is_set = bpy.props.BoolProperty(
        name="Swivel Set",
        default=False,
    )
    bpy.types.Scene.rotools_swivel_point = bpy.props.FloatVectorProperty(
        name="Swivel Point",
        description="World-space point the swivel pivot sits on",
        size=3,
        subtype='XYZ',
    )
    bpy.types.Scene.rotools_swivel_normal = bpy.props.FloatVectorProperty(
        name="Swivel Normal",
        description="Surface normal at the picked swivel point, drawn by the marker",
        size=3,
        subtype='XYZ',
        default=(0.0, 0.0, 1.0),
    )
    bpy.types.Scene.rotools_swivel_kind = bpy.props.StringProperty(
        name="Swivel Kind",
        default="",
    )

    bpy.types.Scene.rotools_snap_move = _snap_proxy(
        "use_snap_translate", "Snap movement to the snapping settings below")
    bpy.types.Scene.rotools_snap_scale = _snap_proxy(
        "use_snap_scale", "Snap scaling to the snapping settings below")
    bpy.types.Scene.rotools_snap_rotate = _snap_proxy(
        "use_snap_rotate", "Snap rotation to the angle increment below")

    # --- dragger ----------------------------------------------------------
    bpy.types.Scene.rotools_drag_grid_snap = bpy.props.BoolProperty(
        name="Grid Snap",
        description="Round the dragged reference point to the grid increment",
        default=True,
    )
    bpy.types.Scene.rotools_drag_grid_size = bpy.props.FloatProperty(
        name="Grid Size",
        description="Position snap increment, in Roblox studs (1 stud = 1 Blender unit)",
        default=1.0,
        min=0.0,
        soft_max=16.0,
        subtype='DISTANCE',
    )
    bpy.types.Scene.rotools_drag_soft_snap = bpy.props.BoolProperty(
        name="Soft Snap",
        description="Magnetic pull toward nearby vertices and edge midpoints, "
                    "which applies when the grid increment is off",
        default=True,
    )
    bpy.types.Scene.rotools_drag_surface_align = bpy.props.BoolProperty(
        name="Surface Align",
        description="Tip the dragged selection so it lies against the surface it "
                    "is dropped on. Hold Alt while dragging to override",
        default=True,
    )
    bpy.types.Scene.rotools_drag_use_ground = bpy.props.BoolProperty(
        name="Ground Plane",
        description="Treat a horizontal plane as a collidable surface, so parts "
                    "dragged over empty space land on it like a Roblox baseplate",
        default=True,
    )
    bpy.types.Scene.rotools_drag_ground_z = bpy.props.FloatProperty(
        name="Ground Height",
        description="Z height of the synthetic ground plane",
        default=0.0,
        subtype='DISTANCE',
    )

    # Roblox Studio's own default rotate-snap increment. bpy.data isn't
    # accessible yet during registration (restricted context), so defer a tick.
    bpy.app.timers.register(_set_default_rotate_increment, first_interval=0)


def _set_default_rotate_increment():
    """Nudge Blender's 5 degree rotate increment to Roblox's 15 degrees.

    Only for scenes still sitting on Blender's own default. This used to
    overwrite every scene on every addon enable, so a user who set their own
    increment lost it on the next Blender start - "set a default" is what was
    wanted, not "reassert a default forever".
    """
    for scene in bpy.data.scenes:
        ts = scene.tool_settings
        if abs(ts.snap_angle_increment_3d - BLENDER_DEFAULT_ANGLE_INCREMENT) < 1e-6:
            ts.snap_angle_increment_3d = ROBLOX_ANGLE_INCREMENT
            _nudged_scene_names.add(scene.name)


def unregister():
    # One-shot (it returns None), so in normal use it has already fired. A
    # disable-and-reload completed inside the same tick would otherwise leave a
    # callback queued against the dead module.
    if bpy.app.timers.is_registered(_set_default_rotate_increment):
        bpy.app.timers.unregister(_set_default_rotate_increment)

    # Revert only the scenes this addon actually nudged, and only if nothing
    # has changed the value since - a scene the user re-set to 15 degrees
    # themselves after the nudge is left alone.
    for scene in bpy.data.scenes:
        if scene.name not in _nudged_scene_names:
            continue
        ts = scene.tool_settings
        if abs(ts.snap_angle_increment_3d - ROBLOX_ANGLE_INCREMENT) < 1e-6:
            ts.snap_angle_increment_3d = BLENDER_DEFAULT_ANGLE_INCREMENT
    _nudged_scene_names.clear()

    del bpy.types.Scene.rotools_orientation
    del bpy.types.Scene.rotools_pivot_mode
    del bpy.types.Scene.rotools_scale_pivot
    del bpy.types.Scene.rotools_swivel_element
    del bpy.types.Scene.rotools_swivel_is_set
    del bpy.types.Scene.rotools_swivel_point
    del bpy.types.Scene.rotools_swivel_normal
    del bpy.types.Scene.rotools_swivel_kind
    del bpy.types.Scene.rotools_snap_move
    del bpy.types.Scene.rotools_snap_scale
    del bpy.types.Scene.rotools_snap_rotate
    del bpy.types.Scene.rotools_drag_grid_snap
    del bpy.types.Scene.rotools_drag_grid_size
    del bpy.types.Scene.rotools_drag_soft_snap
    del bpy.types.Scene.rotools_drag_surface_align
    del bpy.types.Scene.rotools_drag_use_ground
    del bpy.types.Scene.rotools_drag_ground_z
