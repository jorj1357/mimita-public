"""The tool-settings rows shared by the RoTools tools.

Orientation and pivot are scene-wide, not per-tool, so every tool draws the same
rows from here. Duplicating them per tool is how they drift - the Move tool used
to own the only orientation control while Scale and Rotate silently hardcoded
LOCAL, and each tool grew its own subtly different snap row.
"""


def draw_orientation_row(context, layout):
    """World / Local and the active pivot mode."""
    scene = context.scene

    row = layout.row(align=True)
    row.prop(scene, "rotools_orientation", expand=True)

    row = layout.row(align=True)
    row.prop(scene, "rotools_pivot_mode", text="")


def draw_swivel_row(context, layout):
    """The swivel pick/clear controls and, in SWIVEL mode, what is set.

    Rotate-only: swivel can only be picked while the Rotate tool is active
    (see operators/set_swivel.py), so no other tool draws this row.
    """
    scene = context.scene

    row = layout.row(align=True)
    row.operator("rotools.set_swivel", text="Set Swivel", icon='EYEDROPPER')
    sub = row.row(align=True)
    sub.enabled = scene.rotools_swivel_is_set
    sub.operator("rotools.clear_swivel", text="", icon='X')

    if scene.rotools_pivot_mode != 'SWIVEL':
        return

    row = layout.row(align=True)
    row.prop(scene, "rotools_swivel_element", text="")
    if scene.rotools_swivel_is_set:
        row.label(text=scene.rotools_swivel_kind.title(), icon='PIVOT_CURSOR')
    else:
        # SWIVEL with nothing picked falls back to Center; saying so beats
        # leaving the user to wonder why the handles have not moved.
        row.label(text="Not set - using Center", icon='INFO')


def draw_snap_row(context, layout, proxy, elements=True):
    """Blender's own transform snapping, as one toggle plus what it snaps to.

    `proxy` is one of the `rotools_snap_*` scene properties, which drive the
    master `use_snap` and that transform mode's affect flag together - see
    core/scene_state.py for why exposing either one alone does not work.

    This is Blender's snapping, not the dragger's. The two are separate systems
    and these rows should not imply otherwise.
    """
    scene = context.scene
    ts = scene.tool_settings

    row = layout.row(align=True)
    row.prop(scene, proxy, text="Snap", toggle=True, icon='SNAP_INCREMENT')
    sub = row.row(align=True)
    sub.active = getattr(scene, proxy)

    if not elements:
        sub.prop(ts, "snap_angle_increment_3d", text="Increment")
        return

    sub.prop(ts, "snap_elements", text="")
    if 'INCREMENT' in ts.snap_elements and context.space_data is not None:
        sub.prop(context.space_data.overlay, "grid_scale", text="Increment")
