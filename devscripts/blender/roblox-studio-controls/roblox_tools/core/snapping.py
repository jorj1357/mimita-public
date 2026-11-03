"""Collision + snapping engine for the Roblox-style dragger.

Three layers, cheapest first:

  broad phase   world-AABB overlap (or a ray/AABB slab test) to decide which
                objects are worth looking at at all
  narrow phase  a per-object `BVHTree`, built lazily on first touch and cached
                for the lifetime of one drag - nothing but the dragged objects
                moves while a drag is running, so the cache stays valid and we
                never rebuild per frame
  fine pass     read the hit triangle's own corners straight out of the cached
                geometry and snap to those (the coarse -> fine handoff: the BVH
                narrows a whole object down to one triangle, then we look at
                that triangle's vertices and edge midpoints)

Trees are built in *world* space from the evaluated mesh. That costs one
transform at build time and buys two things: the hot loop never converts
between spaces, and a BVH face index maps directly back to the three
world-space corners of that triangle for the fine pass.

The synthetic ground is not a mesh - it is an analytic infinite Z-up plane, so
the dragger always has a surface to land on and you can never slide off the
edge of a finite stand-in quad.
"""

from mathutils import Vector
from mathutils.bvhtree import BVHTree

from .bounds import world_aabb, aabb_overlap, ray_hits_aabb

# Sentinel used as `SurfaceHit.obj` for hits on the synthetic ground plane,
# which has no bpy Object behind it.
GROUND = 'GROUND'

# Best-to-worst. The dragger takes the first of these it finds within margin,
# so a corner always wins over an edge, an edge over a flat face, and the grid
# only gets a say when no geometry is close enough to matter.
SNAP_PRIORITY = ('VERTEX', 'EDGE', 'FACE')


class SurfaceHit:
    """A point on something the dragged selection is allowed to land on."""

    __slots__ = ('point', 'normal', 'obj', 'distance')

    def __init__(self, point, normal, obj, distance):
        self.point = point
        self.normal = normal
        self.obj = obj
        self.distance = distance


class SnapResult:
    """Where the reference point ends up, and what pulled it there.

    `kind` is one of 'VERTEX'/'EDGE'/'FACE'/'GRID', or None when nothing was in
    range and the point is returned untouched.
    """

    __slots__ = ('kind', 'point')

    def __init__(self, kind, point):
        self.kind = kind
        self.point = point


def _build_tree(obj, depsgraph):
    """World-space (BVHTree, verts, triangles) for `obj`, or None if it has no faces."""
    eval_obj = obj.evaluated_get(depsgraph)
    mesh = eval_obj.to_mesh()
    if mesh is None:
        return None

    mesh.calc_loop_triangles()
    matrix = obj.matrix_world
    # `matrix @ v.co` returns fresh Vectors, so these outlive to_mesh_clear().
    verts = [matrix @ v.co for v in mesh.vertices]
    tris = [tuple(tri.vertices) for tri in mesh.loop_triangles]
    eval_obj.to_mesh_clear()

    if not tris:
        return None
    return BVHTree.FromPolygons(verts, tris, all_triangles=True), verts, tris


def snap_to_grid(point, size):
    """Round a point to the nearest multiple of `size` on every axis."""
    return Vector([round(point[i] / size) * size for i in range(3)])


def _keep_closest(found, kind, point, distance):
    existing = found.get(kind)
    if existing is None or distance < existing[1]:
        found[kind] = (point.copy(), distance)


class DragScene:
    """Everything one drag is allowed to collide with, plus its caches.

    Built once per drag. `dragged` objects are excluded from every query, which
    is why the dragger cannot use `scene.ray_cast` for its per-frame drop
    target: that has no way to ignore the geometry currently under the cursor.

    An object opts out of being a collision/snap target with a `Collidable`
    custom property (Object Properties / N-panel > Custom Properties > New >
    Boolean) set to False - deliberately a raw ID property rather than a
    registered bpy.props.BoolProperty, so there is no addon-owned UI or
    register/unregister to maintain; Blender's own Custom Properties panel is
    the whole interface. Absent means collidable, matching Roblox's own
    CanCollide default.
    """

    def __init__(self, context, dragged, use_ground=True, ground_z=0.0):
        self.depsgraph = context.evaluated_depsgraph_get()
        self.use_ground = use_ground
        self.ground_z = ground_z

        dragged_names = {obj.name for obj in dragged}
        self.candidates = [
            obj for obj in context.view_layer.objects
            if obj.type == 'MESH'
            and obj.name not in dragged_names
            and obj.visible_get()
            and obj.get("Collidable", True)
        ]

        self._aabbs = {}
        self._trees = {}

    def aabb(self, obj):
        """Cached world AABB. Broad phase only ever needs this, never the tree."""
        cached = self._aabbs.get(obj.name)
        if cached is None:
            cached = world_aabb((obj,))
            self._aabbs[obj.name] = cached
        return cached

    def tree(self, obj):
        """Cached (BVHTree, verts, tris), built on first use. None if unusable."""
        if obj.name not in self._trees:
            self._trees[obj.name] = _build_tree(obj, self.depsgraph)
        return self._trees[obj.name]

    def ray_cast(self, origin, direction):
        """Nearest surface along the ray, ignoring the dragged objects.

        `direction` must be normalized so the distances reported by the BVH and
        by the analytic ground plane are comparable.
        """
        best = None
        for obj in self.candidates:
            mins, maxs = self.aabb(obj)
            if not ray_hits_aabb(origin, direction, mins, maxs):
                continue
            entry = self.tree(obj)
            if entry is None:
                continue
            location, normal, _index, distance = entry[0].ray_cast(origin, direction)
            if location is None:
                continue
            if best is None or distance < best.distance:
                best = SurfaceHit(location, normal, obj, distance)

        ground = self._ground_ray(origin, direction)
        if ground is not None and (best is None or ground.distance < best.distance):
            best = ground
        return best

    def _ground_ray(self, origin, direction):
        """Intersection with the synthetic baseplate: an infinite Z-up plane."""
        if not self.use_ground or abs(direction.z) < 1e-9:
            return None
        distance = (self.ground_z - origin.z) / direction.z
        if distance <= 0.0:
            return None
        return SurfaceHit(
            origin + direction * distance,
            Vector((0.0, 0.0, 1.0)),
            GROUND,
            distance,
        )

    def nearest_features(self, point, radius):
        """Closest vertex / edge midpoint / face point to `point` within `radius`.

        Returns {kind: (world point, distance)} for whichever kinds were found.
        The face entry is the BVH's own nearest-surface answer; the vertex and
        edge entries are read off that same hit triangle - one `find_nearest`
        per candidate object buys all three.
        """
        found = {}
        lo = Vector((point.x - radius, point.y - radius, point.z - radius))
        hi = Vector((point.x + radius, point.y + radius, point.z + radius))

        for obj in self.candidates:
            mins, maxs = self.aabb(obj)
            if not aabb_overlap(lo, hi, mins, maxs):
                continue
            entry = self.tree(obj)
            if entry is None:
                continue

            tree, verts, tris = entry
            location, _normal, index, distance = tree.find_nearest(point, radius)
            if location is None:
                continue
            _keep_closest(found, 'FACE', location, distance)

            a, b, c = (verts[i] for i in tris[index])
            for corner in (a, b, c):
                _keep_closest(found, 'VERTEX', corner, (corner - point).length)
            for start, end in ((a, b), (b, c), (c, a)):
                midpoint = (start + end) * 0.5
                _keep_closest(found, 'EDGE', midpoint, (midpoint - point).length)

        return {kind: hit for kind, hit in found.items() if hit[1] <= radius}


def resolve_snap(drag_scene, reference, radius, grid_size, use_soft, use_grid):
    """Pick where `reference` should land.

    Hard grid snap, when it is switched on, wins outright - an increment that
    silently loses to whatever vertex happens to be nearby is not an increment,
    and that is exactly how it reads in use: positions that are almost, but not
    quite, on the grid. This is the priorities doc's own rule for soft snap
    ("magnetic pull ... *when hard snap is off*"); its separate
    "vertex > edge > face > grid fallback" line is about ordering *within* the
    soft pass, which is preserved below.
    """
    if use_grid and grid_size > 0.0:
        return SnapResult('GRID', snap_to_grid(reference, grid_size))

    if use_soft:
        found = drag_scene.nearest_features(reference, radius)
        for kind in SNAP_PRIORITY:
            hit = found.get(kind)
            if hit is not None:
                return SnapResult(kind, hit[0])

    return SnapResult(None, reference.copy())
