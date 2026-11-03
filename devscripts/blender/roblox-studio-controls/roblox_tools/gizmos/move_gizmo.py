"""Roblox Studio's Move handles: one arrow per bounding-box face, not three
arrows on a central tripod.

Studio draws six arrows sitting on the six faces of the part's bounding box, so
you push the face you want to move. Blender's own move gizmo draws three
double-ended axes from a centre point. This follows Studio - it is the single
most visible difference between the two tools, and pushing on the near face is
what makes Studio's move read as "shove the part" rather than "adjust a value".

The centre ring stays: it is the free screen-space move, and it is where the
active pivot (Center / Origin / Swivel) is shown.
"""

import bpy
from mathutils import Matrix

from ..core.bounds import AXIS_INDEX, aabb_center, edit_mesh_local_aabb, local_aabb
from ..core.bounds import point_from_local
from ..core.pivot import pivot_point, transform_objects
from ..core.gizmo_common import (
    AXIS_COLORS,
    FLIP_ROTATION,
    orientation_frame,
    style_handle,
)

ARROW_LENGTH = 0.9
# Gap between the bounding-box face and the tail of its arrow, so the handle
# reads as sitting *on* the part rather than buried in it.
HANDLE_GAP = 0.12

HANDLES = [(axis, sign) for axis in 'XYZ' for sign in (1, -1)]


class ROTOOLS_GGT_move(bpy.types.GizmoGroup):
    bl_idname = "ROTOOLS_GGT_move"
    bl_label = "Roblox Move Gizmo"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    # 'SCALE' makes the gizmo group respect camera zoom instead of drawing at a
    # constant on-screen size (bpy.types.GizmoGroup.bl_options).
    bl_options = {'3D', 'SCALE'}

    @classmethod
    def poll(cls, context):
        if context.mode == 'EDIT_MESH':
            return bool(context.objects_in_mode)
        return context.mode == 'OBJECT' and context.selected_objects

    def _make_axis_gizmo(self, axis):
        gz = style_handle(self.gizmos.new("GIZMO_GT_arrow_3d"), AXIS_COLORS[axis])
        gz.draw_style = 'NORMAL'
        gz.length = ARROW_LENGTH
        gz.scale_basis = 1.0

        op = gz.target_set_operator("transform.translate")
        op.release_confirm = True
        op.constraint_axis = tuple(a == axis for a in 'XYZ')
        return gz, op

    def _make_center_gizmo(self):
        gz = style_handle(self.gizmos.new("GIZMO_GT_move_3d"), (1.0, 1.0, 1.0))
        gz.draw_style = 'RING_2D'
        gz.alpha = 0.6
        gz.scale_basis = 0.16

        op = gz.target_set_operator("transform.translate")
        op.release_confirm = True
        self.center_op = op
        return gz

    def setup(self, context):
        self.axis_ops = {}
        self.axis_gizmos = {}
        for handle in HANDLES:
            gz, op = self._make_axis_gizmo(handle[0])
            self.axis_gizmos[handle] = gz
            self.axis_ops[handle] = op
        self.center_gizmo = self._make_center_gizmo()

    def draw_prepare(self, context):
        rotation_3x3, axis_rotations, orient_type = orientation_frame(context)
        pivot = pivot_point(context, rotation_3x3)
        if pivot is None:
            # poll() only gates on there being an object in Edit Mesh, not on
            # anything actually being selected within it - so a selection of
            # zero verts still leaves the group running, and without this the
            # handles would keep drawing at their last (now stale) position.
            for gz in self.axis_gizmos.values():
                gz.hide = True
            self.center_gizmo.hide = True
            return
        for gz in self.axis_gizmos.values():
            gz.hide = False
        self.center_gizmo.hide = False

        # In SWIVEL mode the arrows radiate from the picked point - the whole
        # point of setting a swivel is to work from it. Otherwise they sit on
        # the bounding-box faces, Studio style. A selection of nothing but
        # Empties (Object Mode) or nothing selected (Edit Mesh) has no box to
        # sit on, so it falls back to the pivot too.
        face_positions = None
        if context.scene.rotools_pivot_mode != 'SWIVEL':
            if context.mode == 'EDIT_MESH':
                aabb = edit_mesh_local_aabb(context, rotation_3x3)
            else:
                objects = transform_objects(context)
                aabb = local_aabb(objects, rotation_3x3) if objects else None
            if aabb is not None:
                mins, maxs = aabb
                mid = aabb_center(mins, maxs)
                bounds = {1: maxs, -1: mins}
                face_positions = {}
                for axis, sign in HANDLES:
                    i = AXIS_INDEX[axis]
                    scalars = list(mid)
                    scalars[i] = bounds[sign][i] + sign * HANDLE_GAP
                    face_positions[(axis, sign)] = point_from_local(rotation_3x3, *scalars)

        for (axis, sign), gz in self.axis_gizmos.items():
            self.axis_ops[(axis, sign)].orient_type = orient_type
            if gz.is_modal:
                # Leave a handle's own matrix alone while it is being dragged -
                # Blender is already driving it interactively, and overwriting
                # matrix_basis out from under that fights the drag.
                continue
            position = pivot if face_positions is None else face_positions[(axis, sign)]
            rot = axis_rotations[axis] if sign == 1 else axis_rotations[axis] @ FLIP_ROTATION
            gz.matrix_basis = Matrix.Translation(position) @ rot

        self.center_gizmo.matrix_basis = Matrix.Translation(pivot)
        self.center_op.orient_type = orient_type


def register():
    bpy.utils.register_class(ROTOOLS_GGT_move)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_GGT_move)
