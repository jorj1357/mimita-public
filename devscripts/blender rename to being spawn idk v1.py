import bpy
import re

print("START")

for collection in bpy.data.collections:

    # only spawn collections
    if "spawn" not in collection.name.lower():
        continue

    print(f"\nSPAWN COLLECTION: {collection.name}")

    # find parent map collection
    map_num = None

    for parent_collection in bpy.data.collections:

        # check children collections
        if collection.name in parent_collection.children.keys():

            print(f"  parent collection: {parent_collection.name}")

            match = re.match(r"map(\d+)", parent_collection.name.lower())

            if match:
                map_num = match.group(1)
                break

    if map_num is None:
        print("  NO MAP FOUND")
        continue

    print(f"  FOUND MAP: map{map_num}")

    spawn_index = 1

    for obj in collection.objects:

        new_name = f"spawn{map_num}_{spawn_index}"

        print(f"    {obj.name} -> {new_name}")

        obj.name = new_name

        spawn_index += 1

print("\nDONE")