"""Where the transform handles sit, and what transforms are anchored to.

One `pivot_point` shared by all three gizmos, driven by `scene.rotools_pivot_mode`:

  CENTER  the centre of the selection's bounding box, measured in the current
          orientation frame. A Roblox part's CFrame *is* its centre, so this is
          the mode that matches Studio; it is the default.
  ORIGIN  the median of the selected objects' origins - Blender's own habit,
          kept because an object whose origin was deliberately placed is a
          reasonable thing to pivot on.
  SWIVEL  a vertex / edge midpoint / face centre picked with `rotools.set_swivel`.

Every gizmo also feeds this pivot to its transform operator as
`center_override`, so what is drawn and what is transformed can never disagree.
Without that, rotation and scaling silently follow
`tool_settings.transform_pivot_point` (MEDIAN_POINT by default, but CURSOR if
the user ever pressed the pivot pie menu) while the handles are drawn somewhere
else entirely.
"""

from mathutils import Matrix, Vector

from .bounds import aabb_center, edit_mesh_local_aabb, local_aabb, point_from_local

# An Empty's bound_box is eight zero vectors in Blender 5.2, so a selected
# Empty would otherwise drag the bounding-box centre toward its origin and
# inflate the scale/rotate handle extents. The dragger already excluded them;
# the gizmos did not.
UNBOUNDED_TYPES = {'EMPTY'}


def transform_objects(context):
    """Selected objects the handles should measure and move.

    Shared by the gizmos and the dragger so the two can never disagree about
    what "the selection" means.
    """
    return [obj for obj in context.selected_objects if obj.type not in UNBOUNDED_TYPES]


def origin_median(objects):
    """Median world-space origin of `objects`, or None when there are none."""
    if not objects:
        return None
    total = Vector((0.0, 0.0, 0.0))
    for obj in objects:
        total += obj.matrix_world.translation
    return total / len(objects)


def swivel_point(scene):
    """The picked swivel point, or None when no swivel has been set."""
    if not scene.rotools_swivel_is_set:
        return None
    return Vector(scene.rotools_swivel_point)


def _edit_mesh_pivot(context, scene, rotation_3x3):
    """CENTER/ORIGIN pivot for a bmesh selection, across every object being
    edited (`context.objects_in_mode`). SWIVEL is handled by the caller - a
    picked swivel point is a world-space coordinate regardless of mode.
    """
    objects = context.objects_in_mode
    if not objects:
        return None

    if scene.rotools_pivot_mode == 'ORIGIN':
        # No per-vertex analog to an object's origin - same treatment Object
        # Mode gives a multi-object selection.
        return origin_median(objects)

    if rotation_3x3 is None:
        rotation_3x3 = Matrix.Identity(3)
    aabb = edit_mesh_local_aabb(context, rotation_3x3)
    if aabb is None:
        return None
    mins, maxs = aabb
    return point_from_local(rotation_3x3, *aabb_center(mins, maxs))


def pivot_point(context, rotation_3x3=None):
    """The active pivot, in world space, or None when nothing is selected.

    `rotation_3x3` is the frame CENTER mode measures its bounding box in; pass
    the same frame the handles are drawn in so the pivot lands on the visual
    centre of that box. Defaults to world axes.
    """
    scene = context.scene

    if scene.rotools_pivot_mode == 'SWIVEL':
        point = swivel_point(scene)
        if point is not None:
            return point
        # No swivel picked yet - fall through to CENTER rather than dropping the
        # handles at the world origin.

    if context.mode == 'EDIT_MESH':
        return _edit_mesh_pivot(context, scene, rotation_3x3)

    objects = transform_objects(context)
    if not objects:
        # A selection of nothing but Empties still deserves handles.
        return origin_median(context.selected_objects)

    if scene.rotools_pivot_mode == 'ORIGIN':
        return origin_median(objects)

    if rotation_3x3 is None:
        rotation_3x3 = Matrix.Identity(3)
    mins, maxs = local_aabb(objects, rotation_3x3)
    return point_from_local(rotation_3x3, *aabb_center(mins, maxs))
