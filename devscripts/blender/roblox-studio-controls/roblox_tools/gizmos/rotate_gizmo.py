"""Roblox Studio's Rotate handles: three rings wrapping the part, snapping to
15 degree steps by default.

Like the scale gizmo, the rotation centre is forced with `center_override`
rather than left to `tool_settings.transform_pivot_point`. Without it the rings
are drawn around one point and the part rotates around another - which is
exactly what happens for anyone whose pivot is set to the 3D cursor, and is also
what the swivel pivot needs in order to mean anything.

Each ring is sized on its own, in its own plane - see `_radii`. A single radius
shared by all three (the bounding sphere) is wrong: it is driven by the box's
longest axis, so the rings spanning the two *short* axes come out far larger
than the part they wrap. Neither may a ring be measured off an axis-aligned
box, which swells as the part turns inside it - the rings must not change size
while they are being dragged.
"""

import bpy
from mathutils import Matrix

from ..core.bounds import axis_vectors, edit_mesh_local_aabb, local_aabb_corners, world_corners
from ..core.pivot import pivot_point, transform_objects
from ..core.gizmo_common import (
    AXIS_COLORS,
    orientation_frame,
    style_handle,
)

RADIUS_PADDING = 1.15
MIN_RADIUS = 0.5

# The two local axes each ring's plane is spanned by: the ring turning about X
# lies in the YZ plane, and so on. Indices into the triple `axis_vectors` returns.
PLANE_AXES = {'X': (1, 2), 'Y': (2, 0), 'Z': (0, 1)}


class ROTOOLS_GGT_rotate(bpy.types.GizmoGroup):
    bl_idname = "ROTOOLS_GGT_rotate"
    bl_label = "Roblox Rotate Gizmo"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    # 'SCALE' is what makes a gizmo group respect camera zoom instead of drawing
    # at a constant on-screen size (bpy.types.GizmoGroup.bl_options: "Scale to
    # respect zoom (otherwise zoom independent display size)").
    bl_options = {'3D', 'SCALE'}

    @classmethod
    def poll(cls, context):
        if context.mode == 'EDIT_MESH':
            return bool(context.objects_in_mode)
        return context.mode == 'OBJECT' and context.selected_objects

    def _make_ring_gizmo(self, axis):
        gz = style_handle(self.gizmos.new("GIZMO_GT_dial_3d"), AXIS_COLORS[axis])
        # No 'CLIP': that option cuts each dial off at the view plane through its
        # centre, so only the camera-facing half of every ring is drawn. Roblox's
        # rings are whole circles, and a half ring gives no target to grab on the
        # side facing away.
        gz.draw_options = set()

        op = gz.target_set_operator("transform.rotate")
        op.orient_axis = axis
        op.release_confirm = True
        self.ring_ops[axis] = op
        return gz

    def setup(self, context):
        self.ring_ops = {}
        self.ring_gizmos = {axis: self._make_ring_gizmo(axis) for axis in 'XYZ'}

    def draw_prepare(self, context):
        rotation_3x3, axis_rotations, orient_type = orientation_frame(context)
        pivot = pivot_point(context, rotation_3x3)
        if pivot is None:
            # poll() only gates on there being an object in Edit Mesh, not on
            # anything actually being selected within it - so a selection of
            # zero verts still leaves the group running, and without this the
            # rings would keep drawing at their last (now stale) position.
            for gz in self.ring_gizmos.values():
                gz.hide = True
            return

        radii = self._radii(context, rotation_3x3, pivot)
        for axis, gz in self.ring_gizmos.items():
            gz.hide = False
            gz.scale_basis = radii[axis]
            gz.matrix_basis = Matrix.Translation(pivot) @ axis_rotations[axis]
            op = self.ring_ops[axis]
            op.orient_type = orient_type
            op.center_override = pivot

    def _radii(self, context, rotation_3x3, pivot):
        """Per-ring radius, keyed by axis.

        A ring turning about axis `a` sweeps the selection around the line
        through the pivot along `a`, so its radius is the farthest any of the
        part's corners sits *from that line*:

            r = max over corners of hypot((c - pivot).u, (c - pivot).v)

        with u, v the ring plane's two axes. Since the frame is orthonormal
        that is exactly the perpendicular distance from the rotation axis - and
        that distance is what a rotation about `a` leaves untouched, so the ring
        holds its size for the whole of its own drag. Padding aside, it is also
        the tightest circle that still clears the part.

        The corners have to be the part's own (`world_corners`), never those of
        an axis-aligned box fitted around it. An AABB grows as its contents
        turn - measured that way this same formula made the ring swell ~40% at
        45 degrees and shrink back by 90.

        Edit Mesh is the one exception: a bmesh selection has no fixed "part
        shape" the way an object's bound_box does, so its own local AABB
        corners (`local_aabb_corners`) are the closest thing available and are
        used as-is.
        """
        if context.mode == 'EDIT_MESH':
            aabb = edit_mesh_local_aabb(context, rotation_3x3)
            corners = local_aabb_corners(rotation_3x3, *aabb) if aabb is not None else []
        else:
            objects = transform_objects(context)
            corners = world_corners(objects) if objects else []

        axes = axis_vectors(rotation_3x3)
        furthest = dict.fromkeys('XYZ', 0.0)
        for corner in corners:
            offset = corner - pivot
            for axis, (u, v) in PLANE_AXES.items():
                # Squared, to keep one sqrt per ring rather than one per corner.
                d_sq = offset.dot(axes[u]) ** 2 + offset.dot(axes[v]) ** 2
                if d_sq > furthest[axis]:
                    furthest[axis] = d_sq

        return {
            axis: max(MIN_RADIUS, RADIUS_PADDING * d_sq ** 0.5)
            for axis, d_sq in furthest.items()
        }


def register():
    bpy.utils.register_class(ROTOOLS_GGT_rotate)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_GGT_rotate)
