"""Roblox Studio style free-drag: grab a part anywhere on its surface and slide
it across other surfaces, with the selection's bounding box resting flush on
whatever is under the cursor.

This is driven from `rotools.select` rather than being a tool of its own -
in Roblox Studio the Select tool *is* the dragger, so dragging a part moves it
and dragging empty space rubber-band selects. See operators/select.py.

Placement, per mouse-move:

  1. Ray from the cursor into `DragScene`, which excludes the dragged objects
     and includes the synthetic ground plane, giving a drop point + normal.
  2. Swing the selection so the axis it is resting on lines up with that normal
     (surface align; hold Alt to keep the original orientation).
  3. Slide it along the normal until its rotated bounding box sits flush on the
     drop point - no penetration, no floating gap.
  4. Snap the *reference point* (the bbox corner nearest the original click, or
     the pivot when the click was on it) onto the best vertex/edge/face/grid
     target and carry the whole selection by that offset.

With nothing under the cursor there is no surface to rest on, so step 1 falls
back to the camera-facing plane through the original grab point and step 3 is
skipped.

While the drag runs:

  Shift    flip snapping for as long as it is held
  Alt      keep the original orientation
  R        spin the selection 90 degrees about the surface normal
  T        tip it onto the next of its six faces
  Ctrl+D   stamp a copy at the current position and keep dragging
"""

import bpy
from math import radians
from mathutils import Quaternion, Matrix, Vector

from ..core.bounds import world_aabb, aabb_corners
from ..core.pivot import pivot_point, transform_objects
from ..core.preferences import get_pref
from ..core.snapping import DragScene, resolve_snap
from ..core.view_math import (
    mouse_ray,
    view_plane_point,
    point_to_region,
    pixels_to_world,
)

# Click within this many pixels of the selection pivot to drag by the pivot
# instead of by a bounding-box corner.
PIVOT_GRAB_PIXELS = 12.0

QUARTER_TURN = radians(90)

WORLD_UP = Vector((0.0, 0.0, 1.0))

MODIFIER_KEYS = {'LEFT_SHIFT', 'RIGHT_SHIFT', 'LEFT_ALT', 'RIGHT_ALT'}


def _signed_axes(matrix):
    """The object's six signed local axes in world space, most-upward first.

    Index 0 is the axis a part is currently resting on, which is the one to
    swing onto a new surface's normal - using the resting axis rather than the
    clicked face's normal is what makes a part dragged from the floor onto a
    wall tip over and lie against the wall. The rest of the list is what `T`
    steps through, so tipping cycles which face ends up against the surface.
    """
    rotation = matrix.to_3x3().normalized()
    axes = [rotation.col[i] * sign for i in range(3) for sign in (1.0, -1.0)]
    axes.sort(key=lambda axis: -axis.z)
    return axes


def _drag_roots(objects):
    """`objects` minus any whose parent (or grandparent, ...) is also in the set.

    Setting `matrix_world` on a child and then on its parent applies the drag
    twice: verified in Blender 5.2, a child 2 units from its parent, both moved
    by +10 on X, landed at X=22 instead of 12 when the child was written first -
    and `context.selected_objects` gives no ordering guarantee. Writing only the
    roots is correct because the children ride along with their parents.
    """
    selected = set(objects)
    roots = []
    for obj in objects:
        parent = obj.parent
        while parent is not None:
            if parent in selected:
                break
            parent = parent.parent
        else:
            roots.append(obj)
    return roots


class ROTOOLS_OT_drag(bpy.types.Operator):
    """Drag the selection across surfaces, Roblox Studio style"""
    bl_idname = "rotools.drag"
    bl_label = "Roblox Drag"
    bl_options = {'REGISTER', 'UNDO', 'GRAB_CURSOR', 'BLOCKING'}

    # Where the mouse went down, which is a few pixels back from where the drag
    # threshold was crossed. Grabbing from the press position keeps the part
    # from jumping by the threshold distance on the first frame.
    start_x: bpy.props.IntProperty(options={'HIDDEN'})
    start_y: bpy.props.IntProperty(options={'HIDDEN'})

    @classmethod
    def poll(cls, context):
        return context.mode == 'OBJECT' and context.selected_objects

    def invoke(self, context, event):
        region = context.region
        rv3d = context.region_data
        if rv3d is None:
            return {'CANCELLED'}

        origin, direction = mouse_ray(region, rv3d, (self.start_x, self.start_y))
        depsgraph = context.evaluated_depsgraph_get()
        hit, location, _normal, _index, obj, _matrix = context.scene.ray_cast(
            depsgraph, origin, direction
        )
        if not hit or obj is None or not obj.select_get():
            return {'CANCELLED'}

        # Everything selected is measured; only the roots are written to.
        self.objects = transform_objects(context)
        if not self.objects:
            return {'CANCELLED'}
        self.movers = _drag_roots(self.objects)

        self.grab_point = location.copy()
        self.start_matrices = [o.matrix_world.copy() for o in self.movers]
        self.rest_axes = _signed_axes(obj.matrix_world)

        self.tip_index = 0
        self.spin_steps = 0
        self.stamps = []
        self.snap_kind = None
        self.reference_point = self.grab_point.copy()

        # Everything below is stored relative to the grab point, so the whole
        # selection travels rigidly with the cursor.
        mins, maxs = world_aabb(self.objects)
        self.corner_offsets = [c - self.grab_point for c in aabb_corners(mins, maxs)]
        self.reference_offset = self._pick_reference(context, region, rv3d, mins, maxs)

        scene = context.scene
        self.drag_scene = DragScene(
            context,
            self.objects,
            use_ground=scene.rotools_drag_use_ground,
            ground_z=scene.rotools_drag_ground_z,
        )
        self.margin_pixels = get_pref(context, "soft_snap_margin")

        self._apply(context, event)
        self._status_set(context)
        self._header_update(context)
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def _pick_reference(self, context, region, rv3d, mins, maxs):
        """Offset from the grab point to the point snapping actually aligns.

        Normally the bounding-box corner nearest the click, so grabbing a part
        by its top-left corner snaps that corner. Clicking on the pivot instead
        drags by the pivot as a zero-size reference point.
        """
        pivot = pivot_point(context)
        if pivot is not None:
            pivot_2d = point_to_region(region, rv3d, pivot)
            if pivot_2d is not None:
                dx = pivot_2d.x - self.start_x
                dy = pivot_2d.y - self.start_y
                if (dx * dx + dy * dy) <= PIVOT_GRAB_PIXELS ** 2:
                    return pivot - self.grab_point

        corners = aabb_corners(mins, maxs)
        nearest = min(corners, key=lambda c: (c - self.grab_point).length_squared)
        return nearest - self.grab_point

    @staticmethod
    def _modifiers(event):
        """(shift, alt) held state, correct on the modifier key's own event.

        `event.shift` still reads True on the LEFT_SHIFT *release* that turns it
        off, so during a shift/alt event the value field is the authority.
        """
        shift, alt = event.shift, event.alt
        if event.type in {'LEFT_SHIFT', 'RIGHT_SHIFT'}:
            shift = event.value == 'PRESS'
        elif event.type in {'LEFT_ALT', 'RIGHT_ALT'}:
            alt = event.value == 'PRESS'
        return shift, alt

    def _orientation(self, scene, surface, alt):
        """(rotation, up) for this frame.

        `up` is the axis R spins about: the drop surface's normal, or world +Z
        when free-dragging so a spin still means something in mid-air.

        Alignment happens when the Surface Align toggle asks for it, *or* when
        the user has tipped the part with T - tipping is a request to put a
        different face against the surface, so it implies the alignment even
        when the toggle is off.
        """
        up = WORLD_UP if surface is None else surface.normal.normalized()

        aligning = surface is not None and scene.rotools_drag_surface_align and not alt
        if aligning or self.tip_index != 0:
            resting = self.rest_axes[self.tip_index % len(self.rest_axes)]
            rotation = resting.rotation_difference(up)
        else:
            rotation = Quaternion()

        if self.spin_steps % 4:
            rotation = Quaternion(up, QUARTER_TURN * self.spin_steps) @ rotation
        return rotation, up

    def _apply(self, context, event):
        region = context.region
        rv3d = context.region_data
        scene = context.scene
        coord = (event.mouse_region_x, event.mouse_region_y)
        shift, alt = self._modifiers(event)

        origin, direction = mouse_ray(region, rv3d, coord)
        surface = self.drag_scene.ray_cast(origin, direction)
        rotation, up = self._orientation(scene, surface, alt)

        if surface is None:
            # Nothing under the cursor: free-drag on the camera-facing plane
            # through the original grab point.
            position = view_plane_point(region, rv3d, coord, self.grab_point)
        else:
            # Push out along the normal until the lowest rotated bbox corner
            # rests exactly on the drop point. Only the normal component moves,
            # so the grab point stays under the cursor.
            depth = min((rotation @ offset).dot(up) for offset in self.corner_offsets)
            position = surface.point - up * depth

        reference = position + (rotation @ self.reference_offset)
        radius = pixels_to_world(region, rv3d, reference, self.margin_pixels)
        # Shift flips snapping for as long as it is held - a per-frame override,
        # deliberately not written back to the scene toggles. Held over snapping
        # that was on it gives free placement; held over snapping that was off it
        # gives the grid.
        flip = shift
        snap = resolve_snap(
            self.drag_scene,
            reference,
            radius,
            scene.rotools_drag_grid_size,
            use_soft=(scene.rotools_drag_soft_snap != flip) and surface is not None,
            use_grid=scene.rotools_drag_grid_size > 0.0 and (scene.rotools_drag_grid_snap != flip),
        )
        offset = snap.point - reference
        if surface is not None:
            # Snapping may only slide the selection *along* the surface, never
            # off it: the placement above already put the box exactly flush, and
            # the normal component of a snap offset would undo that - sinking a
            # part into the floor to reach a vertex just below it, or letting
            # grid rounding lift it off the surface to a round Z.
            #
            # This is also what makes a FACE snap the harmless no-op the
            # priority order assumes it is. The offset to the nearest point on
            # the surface being rested on is purely along the normal, so
            # projecting it away leaves exactly zero.
            offset -= up * offset.dot(up)
        position += offset

        # One rigid transform: rotate about the grab point, then carry it to the
        # new position. Applied on top of each object's start matrix so per-object
        # scale and relative offsets survive untouched.
        transform = (
            Matrix.Translation(position)
            @ rotation.to_matrix().to_4x4()
            @ Matrix.Translation(-self.grab_point)
        )
        for obj, start in zip(self.movers, self.start_matrices):
            obj.matrix_world = transform @ start

        self.snap_kind = snap.kind
        self.reference_point = reference + offset

    def _stamp(self, context):
        """Leave a copy of the selection behind and keep dragging the original.

        A full copy, matching what Ctrl+D means everywhere else in Blender. The
        copies are already coincident with the originals - `obj.copy()` carries
        the transform over - so there is no matrix work here, only re-pointing
        parents that were themselves copied, so the stamp is self-contained
        rather than hanging off the parts still being dragged.
        """
        copies = {}
        for obj in self.objects:
            copy = obj.copy()
            if obj.data is not None:
                copy.data = obj.data.copy()
            for collection in obj.users_collection:
                collection.objects.link(copy)
            copies[obj] = copy

        for obj, copy in copies.items():
            if obj.parent in copies:
                copy.parent = copies[obj.parent]

        self.stamps.extend(copies.values())

    def _discard_stamps(self):
        if not self.stamps:
            return
        data = [obj.data for obj in self.stamps if obj.data is not None]
        bpy.data.batch_remove(self.stamps)
        # The copied meshes are unused now that their objects are gone; leaving
        # them orphaned after a *cancelled* drag is litter nobody asked for.
        bpy.data.batch_remove([block for block in data if block.users == 0])
        self.stamps.clear()

    def _restore(self):
        for obj, start in zip(self.movers, self.start_matrices):
            obj.matrix_world = start
        self._discard_stamps()

    def _status_draw(self, context):
        """Key hints in the status bar, the way Blender's own modal tools do it.

        `status_text_set` hands a callable the real status bar layout, so this
        can use the EVENT_*/MOUSE_* icons rather than a bare string.
        """
        layout = self.layout
        row = layout.row(align=True)
        row.label(text="Confirm", icon='MOUSE_LMB')
        row.separator()
        row.label(text="Cancel", icon='EVENT_ESC')
        row.separator()
        row.label(text="Free / Snap", icon='EVENT_SHIFT')
        row.separator()
        row.label(text="Keep Orientation", icon='EVENT_ALT')
        row.separator()
        row.label(text="Spin 90", icon='EVENT_R')
        row.separator()
        row.label(text="Tip Over", icon='EVENT_T')
        row.separator()
        row.label(text="Stamp Copy", icon='EVENT_D')

    def _status_set(self, context):
        context.workspace.status_text_set(self._status_draw)

    def _status_clear(self, context):
        context.workspace.status_text_set(None)
        if context.area is not None:
            context.area.header_text_set(None)

    def _header_update(self, context):
        """Live readout, so it is visible *which* rule placed the object."""
        if context.area is None:
            return
        point = self.reference_point
        parts = [
            "Drag",
            "Snap: {}".format(self.snap_kind.title() if self.snap_kind else "Free"),
            "X {:.3f}  Y {:.3f}  Z {:.3f}".format(point.x, point.y, point.z),
        ]
        if self.spin_steps % 4:
            parts.append("Spin {}".format((self.spin_steps % 4) * 90))
        if self.tip_index:
            parts.append("Tipped")
        if self.stamps:
            parts.append("Stamped {}".format(len(self.stamps)))
        context.area.header_text_set("  |  ".join(parts))

    def modal(self, context, event):
        if event.type in {'RIGHTMOUSE', 'ESC'}:
            self._restore()
            self._status_clear(context)
            return {'CANCELLED'}

        if event.type == 'LEFTMOUSE' and event.value == 'RELEASE':
            self._status_clear(context)
            return {'FINISHED'}

        # Everything below changes the placement without the mouse necessarily
        # moving, so each one re-applies immediately rather than waiting for the
        # next nudge - the part has to react the instant Shift goes down.
        changed = event.type == 'MOUSEMOVE' or event.type in MODIFIER_KEYS
        if event.value == 'PRESS':
            if event.type == 'R':
                self.spin_steps += 1
                changed = True
            elif event.type == 'T':
                self.tip_index = (self.tip_index + 1) % len(self.rest_axes)
                changed = True
            elif event.type == 'D' and event.ctrl:
                self._stamp(context)
                changed = True

        if changed:
            self._apply(context, event)
            self._header_update(context)

        return {'RUNNING_MODAL'}


def register():
    bpy.utils.register_class(ROTOOLS_OT_drag)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_OT_drag)
