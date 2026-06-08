"""Import a Mimita cinematic replay JSON into Blender."""

import json
import math
import os
from pathlib import Path

import bpy
from mathutils import Vector

MAX_FRAMES = 1000

IMPORT_EFFECTS = True
IMPORT_SOUNDS = True
IMPORT_WEAPONS = True
IMPORT_NPCS = True

IMPORT_MAP = True
IMPORT_PLAYER = True

# Set >1 for sparse keyframes (e.g. 10 = 1 blender keyframe per 10 game ticks).
# Effects and sounds snap to the nearest lower keyframe tick.
KEYFRAME_EVERY_N_TICKS = 1

IMPORT_INDEX = 0
IMPORT_BATCH_SIZE = 8

SCENE_FRAMES_GLOBAL = []
FPS_GLOBAL = 60
CAMERA_GLOBAL = None
OUTFIT_PATH_GLOBAL = None
DEFAULT_WEAPON_PATH_GLOBAL = None
SEEN_EFFECTS = set()
EFFECT_SERIAL = 0
MAX_TICK = 0

SHARED_SPHERE_MESH = None
SHARED_CYLINDER_MESH = None
SHARED_CUBE_MESH = None


def snap_tick_to_keyframe(tick, interval):
    if interval <= 1:
        return tick
    return (tick // interval) * interval




REPLAY_JSON_PATH = (
    r"C:\important\mimita-priv-v8\replays\06-08-2026\10-14-46-replay.json"
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
        return None, None, []

    correction_root = create_empty(f"{name}_correction", collection)
    correction_root.rotation_euler = (
        math.radians(-90.0),
        0.0,
        0.0
    )

    actor_root = create_empty(name, collection)
    actor_root.parent = correction_root
    actor_root.location = (0.0, 0.0, 0.0)
    actor_root.rotation_euler = (0.0, 0.0, 0.0)
    actor_root.scale = (1.0, 1.0, 1.0)

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
            duplicate.parent = actor_root
        duplicate.matrix_local = original.matrix_local.copy()

    return correction_root, actor_root, list(duplicates.values())


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

    obj.scale = Vector(
        state.get(
            "scale",
            (1.0, 1.0, 1.0)
        )
    )


def set_local_transform(obj, state):
    obj.location = Vector(state.get("position", (0.0, 0.0, 0.0)))
    rotation = state.get("rotation", (0.0, 0.0, 0.0))
    obj.rotation_mode = "XYZ"
    obj.rotation_euler = (math.radians(rotation[0]), math.radians(rotation[1]), math.radians(rotation[2]))
    obj.scale = Vector(state.get("scale", (1.0, 1.0, 1.0)))


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


def keyframe_material_fade(material, frame, alpha, blackness=0.0):
    material.use_nodes = True
    material.diffuse_color[3] = alpha
    try:
        material.surface_render_method = "DITHERED"
    except Exception:
        material.blend_method = "BLEND"
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        base = bsdf.inputs["Base Color"].default_value
        original = material.get("mimita_original_color")
        if original is None:
            original = tuple(base[:3])
            material["mimita_original_color"] = original
        color = tuple(component * (1.0 - blackness) for component in original)
        bsdf.inputs["Base Color"].default_value = (*color, alpha)
        bsdf.inputs["Alpha"].default_value = alpha
        bsdf.inputs["Base Color"].keyframe_insert("default_value", frame=frame)
        bsdf.inputs["Alpha"].keyframe_insert("default_value", frame=frame)
    material.keyframe_insert(data_path="diffuse_color", frame=frame)


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
    correction_root = None
    if model_path:
        correction_root, root, parts = duplicate_glb(
            model_path, actor_id, bpy.data.collections["Mimita_Actors"]
        )
    else:
        correction_root, root, parts = None, None, []
    if root is None:
        correction_root, root, parts = None, *create_placeholder(
            actor_id, bpy.data.collections["Mimita_Actors"]
        )

    if actor_state.get("type") == "player":
        apply_outfit_texture(parts, outfit_path)

    materials = []
    for part in parts:
        if part.type != "MESH" or part.data is None:
            continue
        part.data = part.data.copy()
        for slot_index, source_material in enumerate(list(part.data.materials)):
            if source_material is None:
                continue
            material = source_material.copy()
            material.name = f"{actor_id}_{source_material.name}"
            part.data.materials[slot_index] = material
            materials.append(material)

    record = {
        "correction_root": correction_root,
        "root": root,
        "parts": parts,
        "part_lookup": {},
        "materials": materials,
    }
    for part in parts:
        record["part_lookup"][normalized_name(part.name)] = part
    ACTORS[actor_id] = record
    return record


def ensure_weapon(actor_id, actor_record, weapon_path):
    if actor_id in WEAPONS:
        return WEAPONS[actor_id]
    if weapon_path:
        weapon_correction, weapon_root, parts = duplicate_glb(
            weapon_path,
            f"{actor_id}_revolver",
            bpy.data.collections["Mimita_Weapons"],
        )
    else:
        weapon_correction, weapon_root, parts = None, None, []
    if weapon_root is None:
        weapon_correction, weapon_root, parts = None, *create_placeholder(
            f"{actor_id}_revolver", bpy.data.collections["Mimita_Weapons"]
        )

    right_hand = find_right_hand(actor_record["parts"])
    weapon_correction.parent = right_hand or actor_record["root"]
    weapon_correction.location = (0.0, 0.0, 0.0) if right_hand else (0.35, 0.0, 0.9)
    weapon_correction.rotation_euler = (0.0, 0.0, 0.0)
    weapon_correction.scale = (1.0, 1.0, 1.0)
    WEAPONS[actor_id] = {"root": weapon_correction, "parts": parts}
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
            set_local_transform(target, transform)
            keyframe_transform(target, frame)


def _ensure_shared_mesh(kind):
    global SHARED_SPHERE_MESH, SHARED_CYLINDER_MESH, SHARED_CUBE_MESH
    if kind == "sphere":
        if SHARED_SPHERE_MESH is None:
            bpy.ops.mesh.primitive_uv_sphere_add(segments=6, ring_count=4)
            obj = bpy.context.active_object
            SHARED_SPHERE_MESH = obj.data
            bpy.data.objects.remove(obj, do_unlink=True)
        return SHARED_SPHERE_MESH
    elif kind == "cylinder":
        if SHARED_CYLINDER_MESH is None:
            bpy.ops.mesh.primitive_cylinder_add(vertices=8)
            obj = bpy.context.active_object
            SHARED_CYLINDER_MESH = obj.data
            bpy.data.objects.remove(obj, do_unlink=True)
        return SHARED_CYLINDER_MESH
    else:
        if SHARED_CUBE_MESH is None:
            bpy.ops.mesh.primitive_cube_add()
            obj = bpy.context.active_object
            SHARED_CUBE_MESH = obj.data
            bpy.data.objects.remove(obj, do_unlink=True)
        return SHARED_CUBE_MESH


def add_primitive(kind, name, collection, location, scale, shared=True):
    mesh_template = _ensure_shared_mesh(kind) if shared else None
    if mesh_template:
        obj = bpy.data.objects.new(name, mesh_template.copy())
    else:
        if kind == "sphere":
            bpy.ops.mesh.primitive_uv_sphere_add(segments=6, ring_count=4)
        elif kind == "cylinder":
            bpy.ops.mesh.primitive_cylinder_add(vertices=8)
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


def animate_ballistic(obj, effect, spawn_frame, end_frame, fps):
    start = Vector(effect.get("position", (0.0, 0.0, 0.0)))
    velocity = Vector(effect.get("velocity", (0.0, 0.0, 0.0)))
    gravity = float(effect.get("gravity", 0.0))
    for frame in range(spawn_frame, end_frame + 1):
        elapsed = (frame - spawn_frame) / max(fps, 1)
        obj.location = start + velocity * elapsed + Vector(
            (0.0, 0.0, -0.5 * gravity * elapsed * elapsed)
        )
        obj.keyframe_insert(data_path="location", frame=frame)


def create_effect(effect, fps, serial, snapped_tick=None):
    effect_type = str(effect.get("type", "")).lower()
    start_delay = float(effect.get("startDelay", 0.0))
    raw_tick = int(effect.get("spawnTick", 0))
    if snapped_tick is not None:
        raw_tick = snapped_tick
    spawn_frame = raw_tick + 1 + round(start_delay * fps)
    lifetime = float(effect.get("lifetime", effect.get("lifetimeSeconds", 0.1)))
    end_frame = spawn_frame + max(1, round(lifetime * fps))
    position = effect.get("position", effect.get("from", (0.0, 0.0, 0.0)))
    initial_scale = effect.get("scale", (0.1, 0.1, 0.1))
    raw_color = list(effect.get("color", (1.0, 1.0, 1.0, 1.0)))
    while len(raw_color) < 4:
        raw_color.append(1.0)
    event_color = tuple(raw_color[:4])
    collection = bpy.data.collections["Mimita_Effects"]
    obj = None

    if effect_type in ("muzzle_flash", "muzzleflash"):
        obj = add_primitive(
            "sphere", f"MuzzleFlash_{serial}", collection, position, initial_scale, shared=True
        )
        obj.data.materials.append(
            material_with_color(f"Mimita_MuzzleFlash_{serial}", event_color, 12.0)
        )
    elif effect_type in ("tracer", "bullettracer", "bullet_tracer"):
        obj = add_primitive(
            "cylinder", f"Tracer_{serial}", collection, position, (0.02, 0.02, 1.0), shared=True
        )
        thickness = float(effect.get("thickness", 0.2))
        orient_beam(obj, effect.get("from", position), effect.get("to", position), thickness)
        obj.data.materials.append(
            material_with_color(f"Mimita_Tracer_{serial}", event_color, 8.0)
        )
    elif effect_type in (
        "blood", "bloodsplatter", "blood_splatter", "blood_cylinder"
    ):
        obj = add_primitive(
            "cylinder", f"Blood_{serial}", collection, position, initial_scale, shared=True
        )
        obj.scale.z = min(obj.scale.z, 0.025)
        rotation = effect.get("rotation", (0.0, 0.0, 0.0))
        obj.rotation_euler = tuple(math.radians(value) for value in rotation)
        obj.rotation_euler.rotate_axis("X", math.radians(90.0))
        obj.data.materials.append(
            material_with_color(f"Mimita_Blood_{serial}", event_color)
        )
    elif effect_type in (
        "impact", "bullet_impact", "world_impact", "impact_world",
        "impact_entity", "impact_sphere"
    ):
        obj = add_primitive(
            "sphere", f"Impact_{serial}", collection, position, initial_scale, shared=True
        )
        obj.data.materials.append(
            material_with_color(f"Mimita_Impact_{serial}", event_color)
        )
    elif effect_type in ("debris", "world_debris", "debris_block"):
        obj = add_primitive(
            "cube", f"Debris_{serial}", collection, position, initial_scale, shared=True
        )
        obj.data.materials.append(
            material_with_color(f"Mimita_Debris_{serial}", event_color)
        )
        rotation = effect.get("rotation", (0.0, 0.0, 0.0))
        obj.rotation_euler = tuple(math.radians(value) for value in rotation)
        obj.keyframe_insert(data_path="rotation_euler", frame=spawn_frame)
        animate_ballistic(obj, effect, spawn_frame, end_frame, fps)
    elif effect_type == "blood_sphere_particle":
        obj = add_primitive(
            "sphere", f"BloodParticle_{serial}", collection, position, initial_scale, shared=True
        )
        material = material_with_color(
            f"Mimita_BloodParticle_{serial}", event_color
        )
        obj.data.materials.append(material)
        animate_ballistic(obj, effect, spawn_frame, end_frame, fps)
        keyframe_material_fade(material, spawn_frame, 1.0)
        keyframe_material_fade(material, end_frame, 0.0)
    elif effect_type == "footstep":
        obj = add_primitive(
            "cylinder", f"Footstep_{serial}", collection, position, initial_scale, shared=True
        )
        obj.scale.z = 0.01
        obj.display_type = "WIRE"
        obj.hide_render = True
    else:
        return

    keyframe_visibility(obj, max(1, spawn_frame - 1), False)
    keyframe_visibility(obj, spawn_frame, effect_type != "footstep")
    for material in obj.data.materials if obj.type == "MESH" else []:
        keyframe_material_fade(
            material, spawn_frame, float(effect.get("alpha", event_color[3]))
        )
        keyframe_material_fade(material, end_frame, 0.0)
    if effect_type in ("tracer", "bullettracer", "bullet_tracer"):
        length_scale = obj.scale.z
        obj.keyframe_insert(data_path="scale", frame=spawn_frame)
        obj.scale = (
            float(effect.get("endThickness", 0.0)),
            float(effect.get("endThickness", 0.0)),
            length_scale,
        )
        obj.keyframe_insert(data_path="scale", frame=end_frame)
    elif effect.get("endScale"):
        obj.keyframe_insert(data_path="scale", frame=spawn_frame)
        obj.scale = Vector(effect["endScale"])
        obj.keyframe_insert(data_path="scale", frame=end_frame)
    keyframe_visibility(obj, end_frame, effect_type != "footstep")
    keyframe_visibility(obj, end_frame + 1, False)


def import_sounds(sound_events, snap_interval=1):
    scene = bpy.context.scene
    if scene.sequence_editor is None:
        scene.sequence_editor_create()
    sequences = scene.sequence_editor.strips
    for index, event in enumerate(sound_events):
        path = resolve_path(event.get("soundPath", ""))
        if path is None and event.get("assetId"):
            path = resolve_path(ASSETS_BY_ID.get(event["assetId"], {}).get("path", ""))
        raw_tick = int(event.get("tick", 0))
        if snap_interval > 1:
            raw_tick = snap_tick_to_keyframe(raw_tick, snap_interval)
        frame = raw_tick + 1
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
    global CAMERA_GLOBAL
    global OUTFIT_PATH_GLOBAL
    global DEFAULT_WEAPON_PATH_GLOBAL
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
        map_correction, map_root, map_parts = duplicate_glb(
            map_path, "Mimita_Map_Root", bpy.data.collections["Mimita_Map"]
        )
        if map_root:
            log(f"Imported map: {map_path} ({len(map_parts)} objects)")

    OUTFIT_PATH_GLOBAL = find_asset(
        lambda asset: asset.get("source") == "outfit"
        or asset.get("id") == "texture:outfit"
    )
    DEFAULT_WEAPON_PATH_GLOBAL = find_asset(
        lambda asset: asset.get("id") == "model:revolver"
        or asset.get("type") == "weapon_glb"
    )

    camera_data = bpy.data.cameras.new("MimitaCamera")
    CAMERA_GLOBAL = bpy.data.objects.new(
        "MimitaCamera",
        camera_data
    )    
    bpy.data.collections["Mimita_Cameras"].objects.link(CAMERA_GLOBAL)
    scene.camera = CAMERA_GLOBAL

    scene_frames = replay.get("sceneFrames", [])
    max_tick = 0
    seen_effects = set()
    effect_serial = 0
    global SCENE_FRAMES_GLOBAL
    global FPS_GLOBAL
    SCENE_FRAMES_GLOBAL = scene_frames[:MAX_FRAMES]
    FPS_GLOBAL = fps

    if IMPORT_SOUNDS:
        sound_events = replay.get("soundEvents", [])
        import_sounds(sound_events, snap_interval=KEYFRAME_EVERY_N_TICKS)
        log(f"Imported {len(sound_events)} sound events (snapped to {KEYFRAME_EVERY_N_TICKS}-tick interval)")

    bpy.app.timers.register(process_import_batch)


def process_import_batch():

    global IMPORT_INDEX
    global EFFECT_SERIAL
    global MAX_TICK

    scene = bpy.context.scene

    end_index = min(
        IMPORT_INDEX + IMPORT_BATCH_SIZE,
        len(SCENE_FRAMES_GLOBAL)
    )

    for scene_frame in SCENE_FRAMES_GLOBAL[IMPORT_INDEX:end_index]:

        tick = int(scene_frame.get("tick", 0))
        MAX_TICK = max(MAX_TICK, tick)
        snapped_tick = snap_tick_to_keyframe(tick, KEYFRAME_EVERY_N_TICKS)

        # Process actors/camera only on keyframe ticks
        if tick % KEYFRAME_EVERY_N_TICKS == 0:

            frame = tick + 1
            scene.frame_set(frame)

            camera_state = scene_frame.get("camera")

            if camera_state:
                configure_camera(CAMERA_GLOBAL, camera_state, frame)

            for actor_state in scene_frame.get("actors", []):

                actor_type = actor_state.get("type", "")

                if actor_type == "npc" and not IMPORT_NPCS:
                    continue

                if actor_type == "player" and not IMPORT_PLAYER:
                    continue

                actor_id = str(actor_state.get("id", "unknown"))

                actor_record = ensure_actor(
                    actor_state,
                    OUTFIT_PATH_GLOBAL
                )

                set_transform(actor_record["root"], actor_state)

                keyframe_transform(actor_record["root"], frame)

                apply_limb_transforms(
                    actor_record,
                    actor_state,
                    frame
                )

        # Process effects on EVERY tick, snapping to nearest keyframe
        if IMPORT_EFFECTS:

            for effect in scene_frame.get("effects", []):

                signature = (
                    effect.get("type"),
                    effect.get("spawnTick", tick),
                    tuple(effect.get("position", ())),
                    tuple(effect.get("from", ())),
                    tuple(effect.get("to", ())),
                )

                if signature in SEEN_EFFECTS:
                    continue

                SEEN_EFFECTS.add(signature)

                create_effect(
                    effect,
                    FPS_GLOBAL,
                    EFFECT_SERIAL,
                    snapped_tick=snapped_tick,
                )

                EFFECT_SERIAL += 1

    IMPORT_INDEX = end_index

    print(f"Imported {IMPORT_INDEX}/{len(SCENE_FRAMES_GLOBAL)}")

    if IMPORT_INDEX >= len(SCENE_FRAMES_GLOBAL):

        scene.frame_start = 1
        scene.frame_end = MAX_TICK + 1

        print("Replay import complete.")

        return None

    return 0.01


main()
