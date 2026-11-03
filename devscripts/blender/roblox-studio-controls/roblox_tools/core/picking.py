"""Pick one mesh element - a vertex, an edge midpoint or a face centre - under
the cursor. This is what `rotools.set_swivel` uses to place the swivel pivot.

Deliberately *not* built on `core/snapping.py`. The dragger's BVH is triangulated,
so its "edges" include triangulation diagonals that do not exist in the mesh the
user is looking at; snapping a deliberately picked edge onto an invisible
diagonal would be indefensible. Here we take the face index `scene.ray_cast`
reports and read that polygon straight off the evaluated mesh, so an n-gon's
four real edges are the four candidates.

Verified in Blender 5.2: `scene.ray_cast` returns the *original* object but a
`location`/`normal`/`index` belonging to the **evaluated** object's mesh, so the
polygon lookup has to go through `evaluated_get(depsgraph)`. A subdivided cube
raycast down its Z axis reported index 20 of the evaluated mesh's 24 polygons,
and that polygon's corners contained the reported hit point.
"""

from mathutils import Vector

from .preferences import get_pref
from .view_math import mouse_ray, point_to_region

# Best-to-worst, matching the dragger's own snap priority.
ELEMENT_PRIORITY = ('VERTEX', 'EDGE', 'FACE')


class PickResult:
    """A picked point on a piece of geometry.

    `span` is what the overlay draws to show *which* element was picked: empty
    for a vertex, the two endpoints for an edge, the whole polygon loop for a face.
    """

    __slots__ = ('point', 'normal', 'kind', 'obj', 'span')

    def __init__(self, point, normal, kind, obj, span):
        self.point = point
        self.normal = normal
        self.kind = kind
        self.obj = obj
        self.span = span


def _polygon_under_cursor(context, coord):
    """(world verts loop, world centre, world normal, object) of the face under
    the cursor, or None."""
    rv3d = context.region_data
    if rv3d is None:
        return None

    origin, direction = mouse_ray(context.region, rv3d, coord)
    depsgraph = context.evaluated_depsgraph_get()
    hit, _location, _normal, index, obj, matrix = context.scene.ray_cast(
        depsgraph, origin, direction
    )
    if not hit or obj is None or index < 0:
        return None

    eval_obj = obj.evaluated_get(depsgraph)
    mesh = eval_obj.to_mesh()
    if mesh is None or index >= len(mesh.polygons):
        eval_obj.to_mesh_clear()
        return None

    poly = mesh.polygons[index]
    # `matrix @ ...` yields fresh Vectors, so these outlive to_mesh_clear().
    loop = [matrix @ mesh.vertices[i].co for i in poly.vertices]
    center = matrix @ poly.center
    # Normals transform by the inverse transpose, which is the identity only for
    # uniformly scaled objects - and Roblox-style building scales axes freely.
    normal = (matrix.to_3x3().inverted_safe().transposed() @ poly.normal).normalized()
    eval_obj.to_mesh_clear()

    return loop, center, normal, obj


def _screen_distance(region, rv3d, point, coord):
    """Pixels between a world point and the cursor, or inf if it is behind the view."""
    projected = point_to_region(region, rv3d, point)
    if projected is None:
        return float('inf')
    return (projected - Vector(coord)).length


def pick_element(context, coord, element='AUTO'):
    """The element under `coord`, as a `PickResult`, or None when nothing is hit.

    `element` is 'VERTEX' / 'EDGE' / 'FACE' to force one kind, or 'AUTO' to take
    the nearest vertex, then the nearest edge, then the face - each only if it is
    within the soft-snap pixel margin of the cursor, so AUTO degrades to the face
    when the cursor is out in the middle of it.
    """
    found = _polygon_under_cursor(context, coord)
    if found is None:
        return None
    loop, center, normal, obj = found

    region = context.region
    rv3d = context.region_data

    best_vertex = min(loop, key=lambda v: _screen_distance(region, rv3d, v, coord))
    edges = [(loop[i], loop[(i + 1) % len(loop)]) for i in range(len(loop))]
    best_edge = min(
        edges,
        key=lambda e: _screen_distance(region, rv3d, (e[0] + e[1]) * 0.5, coord),
    )

    candidates = {
        'VERTEX': (best_vertex, []),
        'EDGE': ((best_edge[0] + best_edge[1]) * 0.5, list(best_edge)),
        'FACE': (center, loop),
    }

    if element != 'AUTO':
        point, span = candidates[element]
        return PickResult(point, normal, element, obj, span)

    margin = get_pref(context, "soft_snap_margin")
    for kind in ELEMENT_PRIORITY:
        point, span = candidates[kind]
        if kind == 'FACE' or _screen_distance(region, rv3d, point, coord) <= margin:
            return PickResult(point, normal, kind, obj, span)
    return None
