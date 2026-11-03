import bmesh
from mathutils import Matrix, Vector

AXIS_INDEX = {'X': 0, 'Y': 1, 'Z': 2}


def axis_vectors(rotation_3x3):
    """Columns of a 3x3 rotation matrix as the local X/Y/Z unit vectors, in world space."""
    return rotation_3x3.col[0], rotation_3x3.col[1], rotation_3x3.col[2]


def world_corners(objects):
    """World-space bounding-box corners of `objects`, eight per object.

    The *oriented* corners - the part's own box carried through its world
    matrix - as opposed to the axis-aligned box `local_aabb` fits around them.
    The distinction matters to anything that must not change when an object
    rotates: an AABB swells as its contents turn, these corners do not move
    relative to each other at all.
    """
    for obj in objects:
        mw = obj.matrix_world
        for corner in obj.bound_box:
            yield mw @ Vector(corner)


def local_aabb_from_points(points, rotation_3x3):
    """Axis-aligned bounds (in the given local frame) of `points`. Returns
    (mins, maxs), each a 3-tuple of scalar projections onto the local X/Y/Z
    axes, or None if `points` is empty.

    The shared plumbing both Object Mode (`local_aabb`, corners of whole
    objects) and Edit Mesh (`edit_mesh_local_aabb`, corners of a bmesh
    selection) funnel through - only the point source differs.
    """
    ex, ey, ez = axis_vectors(rotation_3x3)
    mins = [float('inf')] * 3
    maxs = [float('-inf')] * 3
    empty = True
    for p in points:
        empty = False
        for i, axis_vec in enumerate((ex, ey, ez)):
            s = axis_vec.dot(p)
            if s < mins[i]:
                mins[i] = s
            if s > maxs[i]:
                maxs[i] = s
    if empty:
        return None
    return mins, maxs


def local_aabb(objects, rotation_3x3):
    """Axis-aligned bounds (in the given local frame) of all objects' world-space
    bounding-box corners. Returns (mins, maxs), each a 3-tuple of scalar projections
    onto the local X/Y/Z axes."""
    return local_aabb_from_points(world_corners(objects), rotation_3x3)


def bmesh_selected_corners(bm, matrix_world):
    """World-space coordinates of every selected vertex in `bm`.

    Verts are the right primitive regardless of the mesh select mode -
    selecting an edge or face selects its verts too, so this covers vertex,
    edge, and face selections uniformly.
    """
    for v in bm.verts:
        if v.select:
            yield matrix_world @ v.co


def edit_mesh_local_aabb(context, rotation_3x3):
    """Axis-aligned bounds (in the given local frame) of the selected mesh
    elements across every object in Edit Mesh (`context.objects_in_mode`).
    Returns (mins, maxs), or None if nothing is selected anywhere.
    """
    def points():
        for obj in context.objects_in_mode:
            bm = bmesh.from_edit_mesh(obj.data)
            yield from bmesh_selected_corners(bm, obj.matrix_world)

    return local_aabb_from_points(points(), rotation_3x3)


def point_from_local(rotation_3x3, sx, sy, sz):
    """Reconstruct a world-space point from its scalar projections onto the local axes."""
    ex, ey, ez = axis_vectors(rotation_3x3)
    return sx * ex + sy * ey + sz * ez


def world_aabb(objects):
    """World-axis-aligned bounds of `objects` as (Vector min, Vector max).

    Just `local_aabb` in the identity frame - the dragger's broad phase wants
    plain world-axis boxes, so it reuses the same projection routine the scale
    gizmo uses for rotated frames rather than duplicating the corner walk.
    """
    mins, maxs = local_aabb(objects, Matrix.Identity(3))
    return Vector(mins), Vector(maxs)


def aabb_center(mins, maxs):
    """Midpoint of an axis-aligned box, as a 3-list of scalars in the same frame."""
    return [(mins[i] + maxs[i]) / 2.0 for i in range(3)]


def aabb_corners(mins, maxs):
    """The 8 corners of an axis-aligned box."""
    return [
        Vector((x, y, z))
        for x in (mins[0], maxs[0])
        for y in (mins[1], maxs[1])
        for z in (mins[2], maxs[2])
    ]


def local_aabb_corners(rotation_3x3, mins, maxs):
    """The 8 corners of a `local_aabb`-frame box, reconstructed in world space.

    What `world_corners` is for an object's own oriented bound_box: the rotate
    gizmo's Edit Mesh ring radii have no such per-object box to fall back on (a
    bmesh selection is a loose point cloud, not a part with a fixed shape), so
    the local AABB's own corners stand in for it.
    """
    return [
        point_from_local(rotation_3x3, x, y, z)
        for x in (mins[0], maxs[0])
        for y in (mins[1], maxs[1])
        for z in (mins[2], maxs[2])
    ]


def aabb_overlap(min_a, max_a, min_b, max_b):
    """True when two world-axis-aligned boxes intersect. Broad-phase reject."""
    for i in range(3):
        if min_a[i] > max_b[i] or max_a[i] < min_b[i]:
            return False
    return True


def ray_hits_aabb(origin, direction, mins, maxs):
    """Slab test: does the ray touch this box at all?

    Cheap reject so the dragger only walks a BVH for objects the ray could
    plausibly reach, instead of every mesh in the scene each mouse-move.
    """
    t_near = float('-inf')
    t_far = float('inf')
    for i in range(3):
        d = direction[i]
        if abs(d) < 1e-9:
            # Ray is parallel to this slab: it either starts inside it or misses.
            if origin[i] < mins[i] or origin[i] > maxs[i]:
                return False
            continue
        t1 = (mins[i] - origin[i]) / d
        t2 = (maxs[i] - origin[i]) / d
        if t1 > t2:
            t1, t2 = t2, t1
        t_near = max(t_near, t1)
        t_far = min(t_far, t2)
        if t_near > t_far or t_far < 0.0:
            return False
    return True
