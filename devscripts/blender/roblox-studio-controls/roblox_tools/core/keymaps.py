"""RoTools shortcuts for tool switching."""

import bpy
from .preferences import get_pref

BINDINGS = (
    ('ONE', True, "rotools.select_tool"),
    ('TWO', True, "rotools.move_tool"),
    ('THREE', True, "rotools.scale_tool"),
    ('FOUR', True, "rotools.rotate_tool"),
)
addon_keymaps = []


def register():
    wm = bpy.context.window_manager
    kc = wm.keyconfigs.addon
    if kc is None or not get_pref(bpy.context, "use_tool_shortcuts"):
        return
    km = kc.keymaps.new(name="Object Mode", space_type='EMPTY')
    for key, ctrl, tool_id in BINDINGS:
        kmi = km.keymap_items.new("rotools.switch_tool", key, 'PRESS', ctrl=ctrl)
        kmi.properties.tool_id = tool_id
        addon_keymaps.append((km, kmi))


def refresh():
    unregister()
    register()
    bpy.context.window_manager.keyconfigs.update()


def unregister():
    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()
