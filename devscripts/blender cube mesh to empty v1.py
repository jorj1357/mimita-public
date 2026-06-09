import bpy

# ---------------------------------------------------
# Converts all mesh objects inside collections
# containing "spawn" into Empty Axes
#
# Example:
# Collection "spawn"
# Collection "spawn.001"
# Collection "player_spawns"
#
# Cube -> Empty at same transform
# ---------------------------------------------------

for collection in bpy.data.collections:

    if "spawn" not in collection.name.lower():
        continue

    print(f"\nProcessing collection: {collection.name}")

    objects_to_convert = list(collection.objects)

    for obj in objects_to_convert:

        # only convert meshes
        if obj.type != "MESH":
            continue

        print(f"Converting {obj.name}")

        # save transforms
        loc = obj.location.copy()
        rot = obj.rotation_euler.copy()
        scale = obj.scale.copy()
        parent = obj.parent

        # create empty
        empty = bpy.data.objects.new(obj.name, None)

        empty.empty_display_type = 'PLAIN_AXES'
        empty.location = loc
        empty.rotation_euler = rot
        empty.scale = scale
        empty.parent = parent

        # link empty to same collections
        for c in obj.users_collection:
            c.objects.link(empty)

        # remove old mesh object
        bpy.data.objects.remove(obj, do_unlink=True)

print("\nDone converting spawn meshes to empties.")