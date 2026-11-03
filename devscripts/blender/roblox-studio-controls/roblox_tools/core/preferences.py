"""Addon preferences, plus the single accessor every reader goes through.

`get_pref` exists so a preference's default lives in exactly one place - the
property declaration below. Call sites used to repeat the default as a literal
fallback (`... if addon else 5`), which silently drifts the moment one of the
two is edited.
"""

import bpy

# `roblox_tools`, whether the addon is installed legacy-style (package
# `roblox_tools.core`) or as an extension (`bl_ext.<repo>.roblox_tools.core`).
# `split(".")[0]` - which this module used to do - returns `bl_ext` in the
# extension case and never finds the preferences.
PACKAGE = __package__.rpartition(".")[0]


def get_prefs(context):
    """This addon's AddonPreferences, or None when it is not registered as an addon."""
    addon = context.preferences.addons.get(PACKAGE)
    return addon.preferences if addon else None


def get_pref(context, name):
    """One preference value, falling back to its own declared default."""
    prefs = get_prefs(context)
    if prefs is None:
        return RoToolsPreferences.bl_rna.properties[name].default
    return getattr(prefs, name)


def _shortcuts_changed(self, context):
    # Imported here rather than at module scope: keymaps reads this module, so
    # a top-level import would be circular.
    from . import keymaps
    keymaps.refresh()


class RoToolsPreferences(bpy.types.AddonPreferences):
    bl_idname = PACKAGE

    box_select_threshold: bpy.props.IntProperty(
        name="Box Select Drag Threshold (px)",
        default=5,
        min=1,
        max=50,
    )

    # A screen-space margin, not a world-space one: the dragger converts it
    # through the current view (core/view_math.pixels_to_world) so the pull
    # feels the same zoomed in as zoomed out.
    soft_snap_margin: bpy.props.IntProperty(
        name="Soft Snap Margin (px)",
        description="How close the reference point must get to a vertex or edge "
                    "midpoint before the dragger snaps onto it",
        default=12,
        min=1,
        max=100,
    )

    # Verified against the resolved user keyconfig: these items sort ahead of
    # Blender's own inside `Object Mode`, so they really do shadow them. Made
    # opt-out rather than unconditional because Ctrl+1..4 is still a real cost
    # for a Blender user.
    use_tool_shortcuts: bpy.props.BoolProperty(
        name="Tool Shortcuts",
        description="Bind Ctrl+1..4 to the RoTools tools. This shadows "
                    "Blender's own Object Mode bindings: Ctrl+1..4 no longer set "
                    "subdivision levels",
        default=True,
        update=_shortcuts_changed,
    )

    swivel_marker_size: bpy.props.IntProperty(
        name="Swivel Marker Size (px)",
        description="On-screen size of the swivel pivot marker and its axis cross",
        default=9,
        min=3,
        max=40,
    )

    camera_move_speed: bpy.props.FloatProperty(
        name="Editor Move Speed (Blender units/s)",
        description="Base camera movement speed in Blender units per second",
        # Ten times the original working default of 8 Blender units/second.
        default=80.0,
        min=0.1,
        max=10000.0,
        precision=2,
    )

    camera_vertical_multiplier: bpy.props.FloatProperty(
        name="Q/E Vertical Speed Multiplier",
        description="Multiplier applied to WASD speed while moving vertically",
        default=10.0,
        min=0.1,
        max=100.0,
        precision=1,
    )

    camera_fast_multiplier: bpy.props.FloatProperty(
        name="Shift Speed Multiplier",
        description="Multiplier applied to camera movement while Shift is held",
        default=4.0,
        min=1.0,
        max=100.0,
        precision=1,
    )

    camera_slow_multiplier: bpy.props.FloatProperty(
        name="Ctrl Slow Multiplier",
        description="Multiplier applied to camera movement while Ctrl is held",
        default=0.25,
        min=0.01,
        max=1.0,
        precision=2,
    )

    camera_look_sensitivity: bpy.props.FloatProperty(
        name="Mouse Look Sensitivity",
        description="Rotation applied per pixel of mouse movement while holding RMB",
        default=0.0025,
        min=0.0001,
        max=0.02,
        precision=4,
    )

    camera_dolly_factor: bpy.props.FloatProperty(
        name="Scroll Dolly Percentage",
        description="Fraction of the current view distance moved per wheel step",
        default=0.12,
        min=0.01,
        max=1.0,
        precision=2,
        subtype='FACTOR',
    )

    camera_min_dolly_step: bpy.props.FloatProperty(
        name="Minimum Scroll Step",
        description="Smallest forward or backward distance for each wheel step",
        default=0.05,
        min=0.001,
        max=1000.0,
        precision=3,
    )

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.prop(self, "box_select_threshold")
        col.prop(self, "soft_snap_margin")
        col.prop(self, "swivel_marker_size")

        camera = layout.box()
        camera.label(text="Camera Navigation", icon='CAMERA_DATA')
        col = camera.column(align=True)
        col.prop(self, "camera_move_speed")
        col.prop(self, "camera_vertical_multiplier")
        col.prop(self, "camera_fast_multiplier")
        col.prop(self, "camera_slow_multiplier")
        col.prop(self, "camera_look_sensitivity")
        col.prop(self, "camera_dolly_factor")
        col.prop(self, "camera_min_dolly_step")
        camera.label(text="Q/E = 10× vertical speed; Shift speeds up; Ctrl slows down")

        box = layout.box()
        box.prop(self, "use_tool_shortcuts")
        sub = box.column(align=True)
        sub.active = self.use_tool_shortcuts
        sub.label(text="Ctrl+1..4 select Select / Move / Scale / Rotate", icon='INFO')
        sub.label(text="While on: Ctrl+1..4 do not set subdivision levels")


def register():
    bpy.utils.register_class(RoToolsPreferences)


def unregister():
    bpy.utils.unregister_class(RoToolsPreferences)
