"""Import a Mimita cinematic replay JSON into Blender."""

import json
import math
import os
from pathlib import Path

import bpy
from mathutils import Vector

# config v1 6 7 2026
# MAX_FRAMES = 300
# IMPORT_EFFECTS = False
# IMPORT_SOUNDS = False
# IMPORT_WEAPONS = False
# IMPORT_NPCS = False
# IMPORT_MAP = True
# IMPORT_PLAYER = True
# KEYFRAME_EVERY_N_TICKS = 4

# config v2  6 7 2026
# MAX_FRAMES = 120
# MAX_FRAMES = 1200

# IMPORT_EFFECTS = False
# IMPORT_SOUNDS = False
# IMPORT_WEAPONS = False
# IMPORT_NPCS = False

# IMPORT_MAP = True
# IMPORT_PLAYER = True

# KEYFRAME_EVERY_N_TICKS = 6

# config v3  6 7 2026
# MAX_FRAMES = 120
MAX_FRAMES = 1200

IMPORT_EFFECTS = True
IMPORT_SOUNDS = True
IMPORT_WEAPONS = True
IMPORT_NPCS = True

IMPORT_MAP = True
IMPORT_PLAYER = True

KEYFRAME_EVERY_N_TICKS = 1

REPLAY_JSON_PATH = (
    r"C:\important\mimita-priv-v8\replays\06-07-2026\11-12-39-replay.json"
)
REPO_ROOT = Path(__file__).resolve().parent.parent
if not (REPO_ROOT / "assets").is_dir():
    REPO_ROOT = Path(r"C:\important\mimita-priv-v8")

COLLECTION_NAMES = (
    "Mimita_Map",
    "Mimita_Actors",
    "Mimita_Weapons",
    "Mimita_Effects",
    "Mimita_Sounds",
    "Mimita_Cameras",
    "Mimita_SourceCache",
)

GLB_CACHE = {}
ACTORS = {}
WEAPONS = {}
ASSETS_BY_ID = {}


def log(message):
    print(f"[MIMITA] {message}")


def resolve_path(path_value):
    if not path_value:
        return None
    path = Path(os.path.expandvars(str(path_value))).expanduser()
    if not path.is_absolute():
        path = REPO_ROOT / path
    path = path.resolve()
    if not path.exists():
        log(f"Missing file: {path}")
        return None
    return path


def remove_collection(name):
    collection = bpy.data.collections.get(name)
    if collection is None:
        return
    for obj in list(collection.all_objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    bpy.data.collections.remove(collection)


def create_collection(name):
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


def move_to_collection(obj, collection):
    for old_collection in list(obj.users_collection):
        old_collection.objects.unlink(obj)
    collection.objects.link(obj)


def link_object(obj, collection):
    if obj.name not in collection.objects:
        collection.objects.link(obj)


def create_empty(name, collection):
    obj = bpy.data.objects.new(name, None)
    collection.objects.link(obj)
    return obj


def import_glb_source(path):
    path = str(path)
    if path in GLB_CACHE:
        return GLB_CACHE[path]

    before = set(bpy.data.objects)
    try:
        bpy.ops.import_scene.gltf(filepath=path)
    except Exception as exc:
        log(f"GLB import failed for {path}: {exc}")
        return None

    imported = list(set(bpy.data.objects) - before)
    if not imported:
        log(f"GLB contained no objects: {path}")
        return None

    cache_collection = bpy.data.collections["Mimita_SourceCache"]
    source_root = create_empty(f"SOURCE_{Path(path).stem}", cache_collection)
    imported_set = set(imported)
    for obj in imported:
        if obj.parent not in imported_set:
            world_matrix = obj.matrix_world.copy()
            obj.parent = source_root
            obj.matrix_world = world_matrix
        move_to_collection(obj, cache_collection)

    source_root.hide_viewport = True
    source_root.hide_render = True
    source_root.hide_set(True)
    record = {"root": source_root, "objects": imported}
    GLB_CACHE[path] = record
    log(f"Cached GLB source: {path} ({len(imported)} objects)")
    return record


def duplicate_glb(path, name, collection):
    source = import_glb_source(path)
    if source is None:
        return None, []

    instance_root = create_empty(name, collection)
    # fix rotationsnsnsnssnn 6 7 2026 
    instance_root.rotation_euler = (
        math.radians(-90.0),
        0.0,
        0.0
    )
    source_objects = source["objects"]
    source_set = set(source_objects)
    duplicates = {}

    for original in source_objects:
        duplicate = original.copy()
        if original.data is not None:
            duplicate.data = original.data
        collection.objects.link(duplicate)
        duplicates[original] = duplicate

    for original, duplicate in duplicates.items():
        if original.parent in source_set:
            duplicate.parent = duplicates[original.parent]
        else:
            duplicate.parent = instance_root
        duplicate.matrix_local = original.matrix_local.copy()

        # duplicate.rotation_euler.rotate_axis(
        #     "X",
        #     math.radians(90.0)
        # )

    return instance_root, list(duplicates.values())


def create_placeholder(name, collection):
    bpy.ops.mesh.primitive_cube_add(size=1.0)
    obj = bpy.context.active_object
    obj.name = f"{name}_MISSING_MODEL"
    move_to_collection(obj, collection)
    log(f"Using placeholder cube for {name}")
    return obj, [obj]

def set_transform(obj, state):

    obj.location = Vector(
        state.get(
            "position",
            (0.0, 0.0, 0.0)
        )
    )

    rotation = state.get(
        "rotation",
        (0.0, 0.0, 0.0)
    )

    obj.rotation_mode = "XYZ"

    obj.rotation_euler = (
        math.radians(rotation[0]),
        math.radians(rotation[1]),
        math.radians(rotation[2]),
    )

    # player model correction
    # mimita player GLB is sideways in Blender
    obj.rotation_euler.rotate_axis(
        "X",
        math.radians(-90.0)
    )

    obj.scale = Vector(
        state.get(
            "scale",
            (1.0, 1.0, 1.0)
        )
    )


def keyframe_transform(obj, frame):
    obj.keyframe_insert(data_path="location", frame=frame)
    obj.keyframe_insert(data_path="rotation_euler", frame=frame)
    obj.keyframe_insert(data_path="scale", frame=frame)


def keyframe_visibility(obj, frame, visible):
    obj.hide_viewport = not visible
    obj.hide_render = not visible
    obj.keyframe_insert(data_path="hide_viewport", frame=frame)
    obj.keyframe_insert(data_path="hide_render", frame=frame)


def find_asset(predicate):
    for asset in ASSETS_BY_ID.values():
        if predicate(asset):
            return resolve_path(asset.get("path", ""))
    return None


def material_with_color(name, color, emission=0.0):
    material = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*color[:3], color[3])
        if "Emission Color" in bsdf.inputs:
            bsdf.inputs["Emission Color"].default_value = (*color[:3], color[3])
            bsdf.inputs["Emission Strength"].default_value = emission
        elif "Emission" in bsdf.inputs:
            bsdf.inputs["Emission"].default_value = (*color[:3], color[3])
            bsdf.inputs["Emission Strength"].default_value = emission
    return material


def apply_outfit_texture(objects, texture_path):
    if texture_path is None:
        return
    try:
        image = bpy.data.images.load(str(texture_path), check_existing=True)
    except Exception as exc:
        log(f"Could not load outfit texture {texture_path}: {exc}")
        return

    changed = 0
    for obj in objects:
        if obj.type != "MESH":
            continue
        obj.data = obj.data.copy()
        if not obj.data.materials:
            obj.data.materials.append(bpy.data.materials.new(f"{obj.name}_Outfit"))
        for slot_index, source_material in enumerate(list(obj.data.materials)):
            material = (
                source_material.copy()
                if source_material
                else bpy.data.materials.new(f"{obj.name}_Outfit_{slot_index}")
            )
            material.use_nodes = True
            obj.data.materials[slot_index] = material
            nodes = material.node_tree.nodes
            bsdf = nodes.get("Principled BSDF")
            if bsdf is None:
                bsdf = nodes.new("ShaderNodeBsdfPrincipled")
                output = nodes.get("Material Output") or nodes.new(
                    "ShaderNodeOutputMaterial"
                )
                material.node_tree.links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])
            texture = nodes.new("ShaderNodeTexImage")
            texture.image = image
            material.node_tree.links.new(
                texture.outputs["Color"], bsdf.inputs["Base Color"]
            )
            changed += 1
    log(f"Applied outfit texture to {changed} material slots: {texture_path}")


def normalized_name(value):
    return "".join(character for character in value.lower() if character.isalnum())


def find_right_hand(objects):
    ranked = []
    for obj in objects:
        name = normalized_name(obj.name)
        score = 0
        if "righthand" in name or "handright" in name:
            score = 4
        elif "rightarm" in name or "armright" in name or "rarm" in name:
            score = 3
        elif "hand" in name and ("right" in name or name.startswith("r")):
            score = 2
        elif "right" in name and "arm" in name:
            score = 1
        if score:
            ranked.append((score, obj))
    return max(ranked, key=lambda item: item[0])[1] if ranked else None


def ensure_actor(actor_state, outfit_path):
    actor_id = str(actor_state.get("id", "unknown"))
    if actor_id in ACTORS:
        return ACTORS[actor_id]

    model_path = resolve_path(actor_state.get("modelPath", ""))
    if model_path:
        root, parts = duplicate_glb(
            model_path, actor_id, bpy.data.collections["Mimita_Actors"]
        )
    else:
        root, parts = (None, [])
    if root is None:
        root, parts = create_placeholder(
            actor_id, bpy.data.collections["Mimita_Actors"]
        )

    if actor_state.get("type") == "player":
        apply_outfit_texture(parts, outfit_path)

    record = {"root": root, "parts": parts, "part_lookup": {}}
    for part in parts:
        record["part_lookup"][normalized_name(part.name)] = part
    ACTORS[actor_id] = record
    return record


def ensure_weapon(actor_id, actor_record, weapon_path):
    if actor_id in WEAPONS:
        return WEAPONS[actor_id]
    if weapon_path:
        root, parts = duplicate_glb(
            weapon_path,
            f"{actor_id}_revolver",
            bpy.data.collections["Mimita_Weapons"],
        )
    else:
        root, parts = (None, [])
    if root is None:
        root, parts = create_placeholder(
            f"{actor_id}_revolver", bpy.data.collections["Mimita_Weapons"]
        )

    right_hand = find_right_hand(actor_record["parts"])
    root.parent = right_hand or actor_record["root"]
    root.location = (0.0, 0.0, 0.0) if right_hand else (0.35, 0.0, 0.9)
    root.rotation_euler = (0.0, 0.0, 0.0)
    root.scale = (1.0, 1.0, 1.0)
    WEAPONS[actor_id] = {"root": root, "parts": parts}
    log(
        f"Attached revolver for {actor_id} to "
        f"{right_hand.name if right_hand else 'actor root fallback'}"
    )
    return WEAPONS[actor_id]


def apply_limb_transforms(actor_record, actor_state, frame):
    limbs = actor_state.get("limbs") or actor_state.get("bodyParts")
    if not limbs:
        return
    entries = limbs.items() if isinstance(limbs, dict) else (
        (entry.get("name", ""), entry) for entry in limbs
    )
    for name, transform in entries:
        normalized = normalized_name(name)
        target = actor_record["part_lookup"].get(normalized)
        if target is None:
            target = next(
                (
                    part
                    for part_name, part in actor_record["part_lookup"].items()
                    if normalized in part_name or part_name in normalized
                ),
                None,
            )
        if target and isinstance(transform, dict):
            set_transform(target, transform)
            keyframe_transform(target, frame)


def add_primitive(kind, name, collection, location, scale):
    if kind == "sphere":
        bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6)
    elif kind == "cylinder":
        bpy.ops.mesh.primitive_cylinder_add(vertices=12)
    else:
        bpy.ops.mesh.primitive_cube_add()
    obj = bpy.context.active_object
    obj.name = name
    obj.location = Vector(location)
    obj.scale = Vector(scale)
    move_to_collection(obj, collection)
    return obj


def orient_beam(obj, start, end, width):
    start_vector = Vector(start)
    end_vector = Vector(end)
    direction = end_vector - start_vector
    length = direction.length
    obj.location = (start_vector + end_vector) * 0.5
    obj.scale = (width, width, max(length * 0.5, 0.001))
    if length > 0.0001:
        obj.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()


def create_effect(effect, fps, serial):
    effect_type = str(effect.get("type", "")).lower()
    spawn_frame = int(effect.get("spawnTick", 0)) + 1
    lifetime = float(effect.get("lifetime", effect.get("lifetimeSeconds", 0.1)))
    end_frame = spawn_frame + max(1, round(lifetime * fps))
    position = effect.get("position", effect.get("from", (0.0, 0.0, 0.0)))
    initial_scale = effect.get("scale", (0.1, 0.1, 0.1))
    collection = bpy.data.collections["Mimita_Effects"]
    obj = None

    if effect_type in ("muzzle_flash", "muzzleflash"):
        obj = add_primitive(
            "sphere", f"MuzzleFlash_{serial}", collection, position, initial_scale
        )
        obj.data.materials.append(
            material_with_color("Mimita_MuzzleFlash", (1.0, 0.72, 0.12, 1.0), 12.0)
        )
    elif effect_type in ("tracer", "bullettracer", "bullet_tracer"):
        obj = add_primitive(
            "cylinder", f"Tracer_{serial}", collection, position, (0.02, 0.02, 1.0)
        )
        orient_beam(
            obj, effect.get("from", position), effect.get("to", position), 0.025
        )
        obj.data.materials.append(
            material_with_color("Mimita_Tracer", (1.0, 0.8, 0.05, 1.0), 8.0)
        )
    elif effect_type in ("blood", "bloodsplatter", "blood_splatter"):
        obj = add_primitive(
            "cylinder", f"Blood_{serial}", collection, position, initial_scale
        )
        obj.scale.z = min(obj.scale.z, 0.025)
        obj.data.materials.append(
            material_with_color("Mimita_Blood", (0.45, 0.005, 0.01, 1.0))
        )
    elif effect_type in ("impact", "bullet_impact", "world_impact"):
        obj = add_primitive(
            "sphere", f"Impact_{serial}", collection, position, initial_scale
        )
        obj.data.materials.append(
            material_with_color("Mimita_Impact", (0.35, 0.35, 0.35, 1.0))
        )
    elif effect_type in ("debris", "world_debris"):
        obj = add_primitive(
            "cube", f"Debris_{serial}", collection, position, initial_scale
        )
        obj.data.materials.append(
            material_with_color("Mimita_Debris", (0.25, 0.22, 0.18, 1.0))
        )
        velocity = Vector(effect.get("velocity", (0.0, 0.0, 0.0)))
        obj.keyframe_insert(data_path="location", frame=spawn_frame)
        obj.location += velocity * lifetime
        obj.keyframe_insert(data_path="location", frame=end_frame)
    elif effect_type == "footstep":
        obj = add_primitive(
            "cylinder", f"Footstep_{serial}", collection, position, initial_scale
        )
        obj.scale.z = 0.01
        obj.display_type = "WIRE"
        obj.hide_render = True
    else:
        return

    keyframe_visibility(obj, max(1, spawn_frame - 1), False)
    keyframe_visibility(obj, spawn_frame, effect_type != "footstep")
    if effect.get("endScale"):
        obj.keyframe_insert(data_path="scale", frame=spawn_frame)
        obj.scale = Vector(effect["endScale"])
        obj.keyframe_insert(data_path="scale", frame=end_frame)
    keyframe_visibility(obj, end_frame, effect_type != "footstep")
    keyframe_visibility(obj, end_frame + 1, False)


def import_sounds(sound_events):
    scene = bpy.context.scene
    if scene.sequence_editor is None:
        scene.sequence_editor_create()
    sequences = scene.sequence_editor.strips
    for index, event in enumerate(sound_events):
        path = resolve_path(event.get("soundPath", ""))
        if path is None and event.get("assetId"):
            path = resolve_path(ASSETS_BY_ID.get(event["assetId"], {}).get("path", ""))
        frame = int(event.get("tick", 0)) + 1
        label = path.name if path else event.get("assetId", "missing sound")
        try:
            if path is None:
                raise FileNotFoundError(label)
            strip = sequences.new_sound(
                name=f"SOUND_{index}_{label}",
                filepath=str(path),
                channel=1 + (index % 8),
                frame_start=frame,
            )
            strip.volume = float(event.get("volume", 1.0))
            if hasattr(strip, "pitch"):
                strip.pitch = float(event.get("pitch", 1.0))
        except Exception as exc:
            marker = scene.timeline_markers.new(f"SOUND: {label}", frame=frame)
            marker["import_error"] = str(exc)
            log(f"Sound strip fallback marker at frame {frame}: {label}: {exc}")


def configure_camera(camera, state, frame):

    camera.location = Vector(
        state.get(
            "position",
            (0.0, 0.0, 0.0)
        )
    )

    rotation = state.get(
        "rotation",
        (0.0, 0.0, 0.0)
    )

    camera.rotation_mode = "XYZ"

    camera.rotation_euler = (
        math.radians(rotation[0]),
        math.radians(rotation[1]),
        math.radians(rotation[2]),
    )

    # mimita -> blender camera correction
    # camera.rotation_euler.rotate_axis(
    #     "Z",
    #     math.radians(0.0)
    # )

    camera.rotation_euler.rotate_axis(
        "X",
        math.radians(130.0)
    )


    camera.rotation_euler.rotate_axis(
        "Y",
        math.radians(-90.0)
    )

    keyframe_transform(camera, frame)

    fov_radians = math.radians(
        float(
            state.get("fov", 110.0)
        )
    )

    camera.data.lens = (
        camera.data.sensor_width /
        (
            2.0 *
            math.tan(
                max(fov_radians, 0.001) * 0.5
            )
        )
    )

    camera.data.keyframe_insert(
        data_path="lens",
        frame=frame
    )

def main():
    replay_path = resolve_path(REPLAY_JSON_PATH)
    if replay_path is None:
        raise FileNotFoundError(REPLAY_JSON_PATH)
    with replay_path.open("r", encoding="utf-8") as replay_file:
        replay = json.load(replay_file)
    log(f"Loaded replay: {replay_path}")

    for name in COLLECTION_NAMES:
        remove_collection(name)
        create_collection(name)
    cache_collection = bpy.data.collections["Mimita_SourceCache"]
    cache_collection.hide_render = True
    cache_collection.hide_viewport = True

    GLB_CACHE.clear()
    ACTORS.clear()
    WEAPONS.clear()
    ASSETS_BY_ID.clear()
    ASSETS_BY_ID.update(
        {
            asset.get("id", f"asset:{index}"): asset
            for index, asset in enumerate(replay.get("assets", []))
        }
    )

    scene = bpy.context.scene
    fps = int(replay.get("metadata", {}).get("timelineFps", 60))
    scene.render.fps = fps
    scene.render.fps_base = 1.0

    map_path = resolve_path(replay.get("world", {}).get("mapPath", ""))
    if map_path is None:
        map_path = find_asset(
            lambda asset: asset.get("type") in ("map", "map_glb")
            or asset.get("id") == "map:mimita"
        )
    if map_path:
        map_root, map_parts = duplicate_glb(
            map_path, "Mimita_Map_Root", bpy.data.collections["Mimita_Map"]
        )
        if map_root:
            log(f"Imported map: {map_path} ({len(map_parts)} objects)")

    outfit_path = find_asset(
        lambda asset: asset.get("source") == "outfit"
        or asset.get("id") == "texture:outfit"
    )
    default_weapon_path = find_asset(
        lambda asset: asset.get("id") == "model:revolver"
        or asset.get("type") == "weapon_glb"
    )

    camera_data = bpy.data.cameras.new("MimitaCamera")
    camera = bpy.data.objects.new("MimitaCamera", camera_data)
    bpy.data.collections["Mimita_Cameras"].objects.link(camera)
    scene.camera = camera

    scene_frames = replay.get("sceneFrames", [])
    max_tick = 0
    seen_effects = set()
    effect_serial = 0
    for scene_frame in scene_frames[:MAX_FRAMES]:
        tick = int(scene_frame.get("tick", 0))

        if tick % KEYFRAME_EVERY_N_TICKS != 0:
            continue
        max_tick = max(max_tick, tick)
        frame = tick + 1
        scene.frame_set(frame)

        camera_state = scene_frame.get("camera")
        if camera_state:
            configure_camera(camera, camera_state, frame)

        for actor_state in scene_frame.get("actors", []):

            actor_type = actor_state.get("type", "")

            # skip NPCs if disabled
            if actor_type == "npc" and not IMPORT_NPCS:
                continue

            # skip player if disabled
            if actor_type == "player" and not IMPORT_PLAYER:
                continue

            actor_id = str(actor_state.get("id", "unknown"))

            actor_record = ensure_actor(actor_state, outfit_path)

            # todo 1 normalize function all glbs go thru 
            # or just idk
            # when exporting from blender its done better or idfrent or idk 
            # if actor_state.get("type") == "player":
            #     actor_record["root"].rotation_euler.rotate_axis(
            #         "X",
            #         math.radians(-90.0)
            #     )

            set_transform(actor_record["root"], actor_state)

            keyframe_transform(actor_record["root"], frame)

            apply_limb_transforms(actor_record, actor_state, frame)

            # optional weapon import
            if IMPORT_WEAPONS:

                actor_weapon_path = (
                    resolve_path(actor_state.get("weaponModelPath", ""))
                    or default_weapon_path
                )

                weapon = ensure_weapon(
                    actor_id,
                    actor_record,
                    actor_weapon_path
                )

                weapon_visible = (
                    actor_state.get("weaponName") == "revolver"
                )

                keyframe_visibility(
                    weapon["root"],
                    frame,
                    weapon_visible
                )

                for weapon_part in weapon["parts"]:
                    keyframe_visibility(
                        weapon_part,
                        frame,
                        weapon_visible
                    )

        if IMPORT_EFFECTS:

            for effect in scene_frame.get("effects", []):

                signature = (
                    effect.get("type"),
                    effect.get("spawnTick", tick),
                    tuple(effect.get("position", ())),
                    tuple(effect.get("from", ())),
                    tuple(effect.get("to", ())),
                )

                if signature in seen_effects:
                    continue

                seen_effects.add(signature)

                create_effect(effect, fps, effect_serial)

                effect_serial += 1

    if IMPORT_SOUNDS:
        import_sounds(replay.get("soundEvents", []))
    scene.frame_start = 1
    scene.frame_end = max_tick + 1
    scene.frame_set(1)
    log(
        f"Done: {len(scene_frames)} scene frames, {len(ACTORS)} actors, "
        f"{effect_serial} effects, {len(replay.get('soundEvents', []))} sounds, "
        f"{fps} FPS"
    )


main()
