"""Pick the swivel pivot: click a vertex, edge or face and the Rotate tool
pivots around it from then on.

Modal rather than a one-shot click so the element under the cursor can be
previewed before committing - picking a pivot you cannot see until after you
have used it is not a pivot, it is a guess. The preview is drawn by
`ui/overlay.py`.

Committing switches `scene.rotools_pivot_mode` to SWIVEL, because setting a
swivel and then not using it is never what was meant. The World/Local
orientation setting keeps applying on top, exactly as it does for the Center and
Origin pivots.

Rotate-only: the keymap/button that invoke this only exist on the Rotate tool,
and `poll` enforces it too so the operator search menu can't bypass that. The
picked point does not outlive the tool either - `ui/overlay.py` clears it on
any tool change, since a swivel is only ever meant for the Rotate that set it.
"""

import bpy

from ..core.picking import pick_element
from ..ui import overlay

ROTATE_TOOL_ID = "rotools.rotate_tool"

ELEMENT_KEYS = {
    'A': 'AUTO',
    'V': 'VERTEX',
    'E': 'EDGE',
    'F': 'FACE',
}

ELEMENT_CYCLE = ('AUTO', 'VERTEX', 'EDGE', 'FACE')


class ROTOOLS_OT_set_swivel(bpy.types.Operator):
    """Click a vertex, edge or face to pivot every RoTools tool around it"""
    bl_idname = "rotools.set_swivel"
    bl_label = "Set Swivel"
    bl_options = {'REGISTER', 'UNDO', 'BLOCKING'}

    @classmethod
    def poll(cls, context):
        if context.mode != 'OBJECT' or context.region_data is None:
            return False
        tool = context.workspace.tools.from_space_view3d_mode(context.mode, create=False)
        return tool is not None and tool.idname == ROTATE_TOOL_ID

    def invoke(self, context, event):
        self.pick = None
        self._update(context, event)
        self._status_set(context)
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}

    def _update(self, context, event):
        element = context.scene.rotools_swivel_element
        self.pick = pick_element(
            context,
            (event.mouse_region_x, event.mouse_region_y),
            element,
        )
        overlay.set_preview(self.pick)
        self._header_update(context)

    def _commit(self, context):
        scene = context.scene
        scene.rotools_swivel_point = self.pick.point
        scene.rotools_swivel_normal = self.pick.normal
        scene.rotools_swivel_kind = self.pick.kind
        scene.rotools_swivel_is_set = True
        scene.rotools_pivot_mode = 'SWIVEL'
        self.report(
            {'INFO'},
            "Swivel set on {} of {}".format(self.pick.kind.title(), self.pick.obj.name),
        )

    def _status_draw(self, context):
        layout = self.layout
        row = layout.row(align=True)
        row.label(text="Set Swivel", icon='MOUSE_LMB')
        row.separator()
        row.label(text="Cancel", icon='EVENT_ESC')
        row.separator()
        row.label(text="Cycle Element", icon='EVENT_TAB')
        row.separator()
        row.label(text="Auto / Vertex / Edge / Face", icon='EVENT_A')

    def _status_set(self, context):
        context.workspace.status_text_set(self._status_draw)

    def _status_clear(self, context):
        context.workspace.status_text_set(None)
        if context.area is not None:
            context.area.header_text_set(None)

    def _header_update(self, context):
        if context.area is None:
            return
        element = context.scene.rotools_swivel_element
        target = self.pick.kind.title() if self.pick is not None else "nothing under cursor"
        context.area.header_text_set(f"Set Swivel  |  Element: {element.title()}  |  On: {target}")

    def modal(self, context, event):
        if event.type == 'MOUSEMOVE':
            self._update(context, event)
            return {'RUNNING_MODAL'}

        if event.type == 'TAB' and event.value == 'PRESS':
            scene = context.scene
            index = ELEMENT_CYCLE.index(scene.rotools_swivel_element)
            scene.rotools_swivel_element = ELEMENT_CYCLE[(index + 1) % len(ELEMENT_CYCLE)]
            self._update(context, event)
            return {'RUNNING_MODAL'}

        if event.value == 'PRESS' and event.type in ELEMENT_KEYS:
            context.scene.rotools_swivel_element = ELEMENT_KEYS[event.type]
            self._update(context, event)
            return {'RUNNING_MODAL'}

        if event.type == 'LEFTMOUSE' and event.value == 'PRESS':
            if self.pick is None:
                # Nothing under the cursor - keep going rather than silently
                # dropping out of a mode the user deliberately entered.
                return {'RUNNING_MODAL'}
            self._commit(context)
            overlay.clear_preview()
            self._status_clear(context)
            return {'FINISHED'}

        if event.type in {'RIGHTMOUSE', 'ESC'}:
            overlay.clear_preview()
            self._status_clear(context)
            return {'CANCELLED'}

        return {'RUNNING_MODAL'}


class ROTOOLS_OT_clear_swivel(bpy.types.Operator):
    """Drop the swivel pivot and go back to pivoting on the selection's centre"""
    bl_idname = "rotools.clear_swivel"
    bl_label = "Clear Swivel"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        scene = context.scene
        scene.rotools_swivel_is_set = False
        scene.rotools_swivel_kind = ""
        if scene.rotools_pivot_mode == 'SWIVEL':
            scene.rotools_pivot_mode = 'CENTER'
        return {'FINISHED'}


def register():
    bpy.utils.register_class(ROTOOLS_OT_set_swivel)
    bpy.utils.register_class(ROTOOLS_OT_clear_swivel)


def unregister():
    bpy.utils.unregister_class(ROTOOLS_OT_clear_swivel)
    bpy.utils.unregister_class(ROTOOLS_OT_set_swivel)
