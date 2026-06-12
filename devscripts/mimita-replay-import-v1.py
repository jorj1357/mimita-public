"""Import a Mimita cinematic replay JSON into Blender."""

import argparse
import json
import math
import os
import sys
from pathlib import Path

import bpy
from mathutils import Vector, Euler, Matrix

# MAX_FRAMES = 100000
# 6 11 2026 note keep at like 250 bc higher frames makes longer import times and 
# wanting to do testing fast 
MAX_FRAMES = 250

IMPORT_EFFECTS = True
IMPORT_SOUNDS = True
IMPORT_WEAPONS = True
IMPORT_NPCS = True

# todo fix bc tiss doesnt work 6 11 2026
IMPORT_MAP = True
# IMPORT_MAP = False
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
REPLAY_PATH_GLOBAL = None
VALIDATION_DATA_GLOBAL = None
VALIDATION_REPORT_PATH_GLOBAL = None

SHARED_SPHERE_MESH = None
SHARED_CYLINDER_MESH = None
SHARED_CUBE_MESH = None


def snap_tick_to_keyframe(tick, interval):
    if interval <= 1:
        return tick
    return (tick // interval) * interval




REPLAY_JSON_PATH = (
    r"C:\important\mimita-priv-v8\replays\06-11-2026\20-22-20-replay.json"
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

IMPORT_RUNNING = False
GLB_CACHE = {}
ACTORS = {}
WEAPONS = {}
ACTOR_WEAPONS = {}
ASSETS_BY_ID = {}


def parse_runtime_args():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--replay")
    parser.add_argument("--validation")
    parser.add_argument("--validation-report")
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return parser.parse_args(args)


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


def game_rotation_to_glb(game_euler):
    """Convert a game Z-up Euler rotation to GLB Y-up using similarity transform.
    
    The correction_root applies R_x(-90°) to convert GLB Y-up → Blender Z-up.
    Body parts are children of actor_root (child of correction_root), so their
    local space is GLB Y-up. To correctly express a game Z-up rotation in
    GLB Y-up, we need the similarity transform: R_glb = C * R_game * C⁻¹
    where C = R_x(+90°).
    """
    game_mat = game_euler.to_matrix()
    C = Euler((math.radians(90.0), 0.0, 0.0)).to_matrix()
    C_inv = Euler((math.radians(-90.0), 0.0, 0.0)).to_matrix()
    glb_mat = C @ game_mat @ C_inv
    return glb_mat.to_euler('XYZ')


def glb_position_to_game(position):
    converted = Vector(position)
    converted.rotate(Euler((math.radians(-90.0), 0.0, 0.0)))
    return converted


def glb_rotation_to_game(rotation_matrix):
    conversion = Euler((math.radians(90.0), 0.0, 0.0)).to_matrix()
    inverse = Euler((math.radians(-90.0), 0.0, 0.0)).to_matrix()
    return inverse @ rotation_matrix @ conversion


def rotation_error_degrees(expected_degrees, actual_matrix):
    expected = Euler(
        tuple(math.radians(value) for value in expected_degrees),
        "XYZ",
    ).to_quaternion()
    actual = actual_matrix.to_quaternion()
    difference = expected.rotation_difference(actual)
    angle = math.degrees(difference.angle) % 360.0
    return min(angle, 360.0 - angle)


def validation_part_object(actor_record, part_name):
    normalized = normalized_name(part_name)
    target = actor_record["part_lookup"].get(normalized)
    if target is not None:
        return target
    return next(
        (
            part
            for candidate, part in actor_record["part_lookup"].items()
            if normalized in candidate or candidate in normalized
        ),
        None,
    )


def validation_output_paths(replay_path, requested_report_path):
    if requested_report_path:
        json_path = Path(requested_report_path)
    else:
        json_path = replay_path.with_name(
            replay_path.stem + "-validation-report.json"
        )
    text_path = json_path.with_suffix(".txt")
    return json_path, text_path


def validate_replay(validation, replay_path, requested_report_path=None):
    thresholds = validation.get("thresholds", {})
    position_threshold = float(thresholds.get("positionMeters", 0.05))
    rotation_threshold = float(thresholds.get("rotationDegrees", 5.0))
    game_fps = int(validation.get("tickRate", 60))
    scene = bpy.context.scene
    blender_fps = scene.render.fps / max(scene.render.fps_base, 1.0)

    samples = []
    aggregates = {}

    def record_sample(
        actor_id,
        part_name,
        tick,
        position_error=None,
        rotation_error=None,
        missing=None,
    ):
        passed = (
            missing is None
            and position_error is not None
            and rotation_error is not None
            and position_error < position_threshold
            and rotation_error < rotation_threshold
        )
        sample = {
            "actor": actor_id,
            "part": part_name,
            "tick": tick,
            "positionErrorMeters": position_error,
            "rotationErrorDegrees": rotation_error,
            "missing": missing,
            "pass": passed,
        }
        samples.append(sample)

        key = (actor_id, part_name)
        aggregate = aggregates.setdefault(
            key,
            {
                "actor": actor_id,
                "part": part_name,
                "sampleCount": 0,
                "failureCount": 0,
                "maxPositionErrorMeters": 0.0,
                "maxRotationErrorDegrees": 0.0,
                "worstPositionTick": None,
                "worstRotationTick": None,
                "missing": set(),
            },
        )
        aggregate["sampleCount"] += 1
        if not passed:
            aggregate["failureCount"] += 1
        if missing:
            aggregate["missing"].add(missing)
        if position_error is not None and (
            aggregate["worstPositionTick"] is None
            or position_error > aggregate["maxPositionErrorMeters"]
        ):
            aggregate["maxPositionErrorMeters"] = position_error
            aggregate["worstPositionTick"] = tick
        if rotation_error is not None and (
            aggregate["worstRotationTick"] is None
            or rotation_error > aggregate["maxRotationErrorDegrees"]
        ):
            aggregate["maxRotationErrorDegrees"] = rotation_error
            aggregate["worstRotationTick"] = tick

    for validation_frame in validation.get("frames", []):
        tick = int(validation_frame.get("tick", 0))
        if tick % KEYFRAME_EVERY_N_TICKS != 0:
            continue
        blender_frame = max(
            1, int(tick * (blender_fps / float(max(game_fps, 1)))) + 1
        )
        scene.frame_set(blender_frame)
        bpy.context.view_layer.update()

        for expected_actor in validation_frame.get("actors", []):
            actor_id = str(expected_actor.get("id", "unknown"))
            actor_record = ACTORS.get(actor_id)
            expected_parts = [("Root", expected_actor.get("root", {}))]
            expected_parts.extend(
                expected_actor.get("bodyParts", {}).items()
            )

            if actor_record is None:
                for part_name, _ in expected_parts:
                    record_sample(
                        actor_id, part_name, tick, missing="actor"
                    )
                continue

            for part_name, expected in expected_parts:
                if part_name == "Root":
                    target = actor_record.get("root")
                else:
                    target = validation_part_object(actor_record, part_name)
                if target is None:
                    record_sample(
                        actor_id, part_name, tick, missing="part"
                    )
                    continue

                actual_position = glb_position_to_game(target.location)
                actual_rotation = glb_rotation_to_game(
                    target.rotation_euler.to_matrix()
                )
                expected_position = Vector(
                    expected.get("position", (0.0, 0.0, 0.0))
                )
                expected_rotation = expected.get(
                    "rotation", (0.0, 0.0, 0.0)
                )
                record_sample(
                    actor_id,
                    part_name,
                    tick,
                    (expected_position - actual_position).length,
                    rotation_error_degrees(
                        expected_rotation, actual_rotation
                    ),
                )

    aggregate_rows = []
    for aggregate in aggregates.values():
        row = dict(aggregate)
        row["missing"] = sorted(row["missing"])
        row["pass"] = row["failureCount"] == 0
        aggregate_rows.append(row)
    aggregate_rows.sort(key=lambda row: (row["actor"], row["part"]))

    passed_parts = [
        f'{row["actor"]}.{row["part"]}'
        for row in aggregate_rows if row["pass"]
    ]
    failed_parts = [
        f'{row["actor"]}.{row["part"]}'
        for row in aggregate_rows if not row["pass"]
    ]
    largest_position = max(
        (row["maxPositionErrorMeters"] for row in aggregate_rows),
        default=0.0,
    )
    largest_rotation = max(
        (row["maxRotationErrorDegrees"] for row in aggregate_rows),
        default=0.0,
    )
    overall_pass = bool(aggregate_rows) and not failed_parts

    report = {
        "schemaVersion": 1,
        "sourceReplay": str(replay_path),
        "sourceValidation": validation.get("sourceReplay", ""),
        "thresholds": {
            "positionMeters": position_threshold,
            "rotationDegrees": rotation_threshold,
        },
        "pass": overall_pass,
        "summary": {
            "sampleCount": len(samples),
            "partCount": len(aggregate_rows),
            "passedParts": passed_parts,
            "failedParts": failed_parts,
            "largestPositionErrorMeters": largest_position,
            "largestRotationErrorDegrees": largest_rotation,
        },
        "parts": aggregate_rows,
        "samples": samples,
    }

    lines = ["[VALIDATION]"]
    current_actor = None
    for row in aggregate_rows:
        if row["actor"] != current_actor:
            current_actor = row["actor"]
            lines.extend(["", f"Actor: {current_actor}"])
        status = "PASS" if row["pass"] else "FAIL"
        lines.append(
            f'{row["part"]}: Position Error = '
            f'{row["maxPositionErrorMeters"]:.6f}m '
            f'(tick {row["worstPositionTick"]}), Rotation Error = '
            f'{row["maxRotationErrorDegrees"]:.6f} deg '
            f'(tick {row["worstRotationTick"]}) [{status}]'
        )
        if row["missing"]:
            lines.append(f'  Missing: {", ".join(row["missing"])}')
    lines.extend(
        [
            "",
            "====================",
            "REPLAY VALIDATION",
            "====================",
            f'Overall: {"PASS" if overall_pass else "FAIL"}',
            f'PASS: {", ".join(passed_parts) if passed_parts else "none"}',
            f'FAIL: {", ".join(failed_parts) if failed_parts else "none"}',
            f"Largest Position Error: {largest_position:.6f}m",
            f"Largest Rotation Error: {largest_rotation:.6f} deg",
        ]
    )

    json_path, text_path = validation_output_paths(
        replay_path, requested_report_path
    )
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    text_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    log(f"Validation JSON: {json_path}")
    log(f"Validation text: {text_path}")
    return report


def keyframe_transform(obj, frame):
    obj.keyframe_insert(data_path="location", frame=frame)
    obj.keyframe_insert(data_path="rotation_euler", frame=frame)
    obj.keyframe_insert(data_path="scale", frame=frame)
    if frame <= 5:
        print(f"[KEYFRAME] obj={obj.name} frame={frame}")


def keyframe_visibility(obj, frame, visible):
    obj.hide_viewport = not visible
    obj.hide_render = not visible
    obj.keyframe_insert(data_path="hide_viewport", frame=frame)
    obj.keyframe_insert(data_path="hide_render", frame=frame)


def animation_summary(obj):
    action = (
        obj.animation_data.action
        if obj and obj.animation_data and obj.animation_data.action
        else None
    )
    if action is None:
        return "NONE", 0, 0
    fcurves = getattr(action, "fcurves", None)
    if fcurves is None:
        return action.name, -1, -1
    return (
        action.name,
        len(fcurves),
        sum(len(curve.keyframe_points) for curve in fcurves),
    )


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
        key = normalized_name(part.name)
        record["part_lookup"][key] = part

    # Reparent body parts directly to actor_root so that root-relative
    # transforms from the replay data are applied in the correct space.
    # The GLB hierarchy is nested (e.g. torso → head), but the exporter
    # bakes all hierarchy transforms into a single root-relative transform
    # per part. Without reparenting, transforms are applied in the
    # intermediate parent's space instead of the root's space.
    body_part_keys = {"head", "torso", "leftarm", "rightarm", "leftleg", "rightleg"}
    for part in parts:
        if normalized_name(part.name) in body_part_keys and part.parent != root:
            part.parent = root

    print(f"[ACTOR CREATED] {actor_id} root={root.name if root else 'None'} parts={len(parts)}")
    ACTORS[actor_id] = record
    return record


def ensure_weapon(actor_id, actor_record, weapon_path):
    if not weapon_path:
        return None
    weapon_key = f"{actor_id}:{weapon_path}"
    if weapon_key in WEAPONS:
        return WEAPONS[weapon_key]

    resolved_path = resolve_path(weapon_path)
    if resolved_path is None:
        print(f"[WEAPON MISSING] actor={actor_id} path={weapon_path}")
        # Create a correction empty as parent so code below doesn't crash
        weapon_correction = create_empty(f"{actor_id}_weapon_missing", bpy.data.collections["Mimita_Weapons"])
        wpn_placeholder, _ = create_placeholder(f"{actor_id}_missing_weapon", bpy.data.collections["Mimita_Weapons"])
        weapon_root = wpn_placeholder
        parts = []
    else:
        weapon_name = os.path.basename(weapon_path).replace('.glb', '')
        weapon_correction, weapon_root, parts = duplicate_glb(
            str(resolved_path),
            f"{actor_id}_{weapon_name}",
            bpy.data.collections["Mimita_Weapons"],
        )
        if weapon_root is None:
            weapon_correction = create_empty(f"{actor_id}_{weapon_name}_correction", bpy.data.collections["Mimita_Weapons"])
            wpn_ph, _ = create_placeholder(f"{actor_id}_{weapon_name}", bpy.data.collections["Mimita_Weapons"])
            weapon_root = wpn_ph
            parts = []

    right_hand = find_right_hand(actor_record["parts"])
    weapon_correction.parent = right_hand or actor_record["root"]
    weapon_correction.location = (0.0, 0.0, 0.0) if right_hand else (0.35, 0.0, 0.9)
    weapon_correction.rotation_euler = (0.0, 0.0, 0.0)
    weapon_correction.scale = (1.0, 1.0, 1.0)
    record = {"root": weapon_correction, "parts": parts}
    WEAPONS[weapon_key] = record
    print(f"[WEAPON CREATED] actor={actor_id} path={weapon_path}")
    return record


def apply_limb_transforms(actor_record, actor_state, frame):
    actor_id = actor_state.get("id", actor_state.get("name", "unknown"))
    limbs = actor_state.get("limbs") or actor_state.get("bodyParts")
    if not limbs:
        return
    if isinstance(limbs, dict):
        entries = limbs.items()
        part_count = len(limbs)
    elif isinstance(limbs, list):
        entries = [(entry.get("name", ""), entry) for entry in limbs]
        part_count = len(limbs)
    else:
        log(f"apply_limb_transforms: unexpected limbs type={type(limbs).__name__} for {actor_id}")
        return
    log(f"apply_limb_transforms: {actor_id} has {part_count} body parts at frame {frame}")

    # The correction_root rotates GLB Y-up → Blender Z-up via -90X.
    # Body parts are children of actor_root (child of correction_root),
    # so their local space is GLB Y-up. But replay stores transforms
    # in game coordinates (Z-up relative to root). Convert game Z-up
    # to GLB Y-up by applying +90X before setting local transforms.
    correction = Euler((math.radians(90.0), 0.0, 0.0))

    keyframed_count = 0
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
            pos = Vector(transform.get("position", (0.0, 0.0, 0.0)))
            pos.rotate(correction)
            rot = transform.get("rotation", (0.0, 0.0, 0.0))
            game_euler = Euler((math.radians(rot[0]), math.radians(rot[1]), math.radians(rot[2])))
            glb_euler = game_rotation_to_glb(game_euler)
            # Debug: print limb transforms at frame 0
            if frame <= 1:
                print(f"[LIMB] name={name}")
                print(f"[LIMB]   bind_location={tuple(round(v, 4) for v in target.location)}")
                print(f"[LIMB]   bind_rotation={tuple(round(math.degrees(a), 1) for a in target.rotation_euler)}")
                print(f"[LIMB]   replay_location={tuple(transform.get('position', (0,0,0)))}")
                print(f"[LIMB]   replay_rotation={tuple(transform.get('rotation', (0,0,0)))}")
                print(f"[LIMB]   final_location={tuple(round(v, 4) for v in pos)}")
                print(f"[LIMB]   final_rotation={tuple(round(math.degrees(a), 1) for a in glb_euler)}")

            target.location = pos
            target.rotation_mode = "XYZ"
            target.rotation_euler = glb_euler
            target.scale = Vector(transform.get("scale", (1.0, 1.0, 1.0)))
            keyframe_transform(target, frame)
            keyframed_count += 1
        elif not target:
            log(f"apply_limb_transforms: part '{name}' not found in actor_record part_lookup for {actor_id}")
            log(f"  available parts: {list(actor_record['part_lookup'].keys())}")
    if keyframed_count > 0 and frame <= 5:
        log(f"apply_limb_transforms: keyframed {keyframed_count} parts for {actor_id} at frame {frame}")


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
        # Orient cylinder along surface normal: game exports the contact normal,
        # and the cylinder should lie flat on that surface. Track Z to normal,
        # then add random spin around normal from rotation field.
        normal = Vector(effect.get("normal", (0.0, 0.0, 1.0)))
        if normal.length > 0.001:
            obj.rotation_euler = normal.to_track_quat('Z').to_euler()
        rot_spin = effect.get("rotation", (0.0, 0.0, 0.0))
        if isinstance(rot_spin, (list, tuple)) and len(rot_spin) >= 3:
            obj.rotation_euler.rotate_axis("Z", math.radians(rot_spin[2]))
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


def safe_load_sound(sound_path):
    if not sound_path:
        return None
    if not os.path.exists(sound_path):
        replay_dir = os.path.dirname(str(REPLAY_JSON_PATH))
        project_root = str(REPO_ROOT)
        candidates = [
            sound_path,
            os.path.join(replay_dir, sound_path),
            os.path.join(project_root, sound_path),
            os.path.join(project_root, "assets", "sounds", os.path.basename(sound_path)),
        ]
        for candidate in candidates:
            if os.path.exists(candidate):
                sound_path = candidate
                break
        else:
            return None
    try:
        return bpy.data.sounds.load(sound_path, check_existing=True)
    except Exception as e:
        log(f"failed to load sound {sound_path}: {e}")
        return None


def import_sounds(sound_events, snap_interval=1, game_fps=60):
    missing_sounds = []
    loaded_count = 0
    scene = bpy.context.scene
    if scene.sequence_editor is None:
        scene.sequence_editor_create()
    sequences = scene.sequence_editor.strips
    blender_fps = scene.render.fps / max(scene.render.fps_base, 1.0)
    for index, event in enumerate(sound_events):
        raw_tick = int(event.get("tick", 0))
        raw_path = event.get("soundPath", "")
        if not raw_path and event.get("assetId"):
            raw_path = ASSETS_BY_ID.get(event["assetId"], {}).get("path", "")
        sound = safe_load_sound(raw_path) if raw_path else None
        if sound is None:
            missing_sounds.append(raw_path or event.get("assetId", "unknown"))
            log(f"Sound not found at tick {raw_tick}: {raw_path}")
            continue
        if snap_interval > 1:
            raw_tick = snap_tick_to_keyframe(raw_tick, snap_interval)
        # Convert game tick to Blender frame: game 60fps → blender fps
        frame = int(raw_tick * (blender_fps / float(game_fps))) + 1
        label = os.path.basename(raw_path) if raw_path else "sound"
        try:
            strip = sequences.new_sound(
                name=f"SOUND_{index}_{label}",
                filepath=sound.filepath,
                channel=1 + (index % 8),
                frame_start=frame,
            )
            strip.volume = float(event.get("volume", 1.0))
            if hasattr(strip, "pitch"):
                strip.pitch = float(event.get("pitch", 1.0))
            loaded_count += 1
        except Exception as exc:
            log(f"Failed to create sound strip at frame {frame}: {label}: {exc}")
            continue
    log(f"Sounds loaded: {loaded_count}")
    if missing_sounds:
        log(f"Sounds missing: {len(missing_sounds)}")
        for path in missing_sounds:
            log(f"  missing sound: {path}")


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
    global REPLAY_PATH_GLOBAL
    global VALIDATION_DATA_GLOBAL
    global VALIDATION_REPORT_PATH_GLOBAL

    runtime_args = parse_runtime_args()
    replay_path = resolve_path(runtime_args.replay or REPLAY_JSON_PATH)
    if replay_path is None:
        raise FileNotFoundError(runtime_args.replay or REPLAY_JSON_PATH)
    REPLAY_PATH_GLOBAL = replay_path
    VALIDATION_REPORT_PATH_GLOBAL = runtime_args.validation_report
    with replay_path.open("r", encoding="utf-8") as replay_file:
        replay = json.load(replay_file)
    log(f"Loaded replay: {replay_path}")

    validation_path_value = runtime_args.validation
    if not validation_path_value:
        validation_path_value = replay.get("validation", {}).get("path")
    if validation_path_value:
        validation_path = Path(validation_path_value)
        if not validation_path.is_absolute():
            validation_path = replay_path.parent / validation_path
    else:
        validation_path = replay_path.with_name(
            replay_path.stem + "-validation.json"
        )
    if validation_path.exists():
        with validation_path.open("r", encoding="utf-8") as validation_file:
            VALIDATION_DATA_GLOBAL = json.load(validation_file)
        log(f"Loaded validation data: {validation_path}")
    else:
        VALIDATION_DATA_GLOBAL = None
        log(f"Validation data not found: {validation_path}")

    # Step 1: Print replay structure
    scene_frames_sizes = replay.get("sceneFrames", [])
    print("[REPLAY]")
    print("sceneFrames =", len(scene_frames_sizes))
    if scene_frames_sizes:
        first_actor = scene_frames_sizes[0].get("actors", [{}])[0]
        print("firstActor keys =", list(first_actor.keys()))

    # Step 2: Check if actor position changes over time
    sample_indices = [idx for idx in [0, 10, 50, 100] if idx < len(scene_frames_sizes)]
    if sample_indices:
        print("[REPLAY POSITIONS]")
        for frame_index in sample_indices:
            actors = scene_frames_sizes[frame_index].get("actors", [])
            if actors:
                actor = actors[0]
                print(f"  frame{frame_index}: pos={actor.get('position')} rot={actor.get('rotation')}")

    for name in COLLECTION_NAMES:
        remove_collection(name)
        create_collection(name)
    cache_collection = bpy.data.collections["Mimita_SourceCache"]
    cache_collection.hide_render = True
    cache_collection.hide_viewport = True

    GLB_CACHE.clear()
    ACTORS.clear()
    WEAPONS.clear()
    ACTOR_WEAPONS.clear()
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
        import_sounds(sound_events, snap_interval=KEYFRAME_EVERY_N_TICKS, game_fps=60)
        log(f"Processed {len(sound_events)} sound events")

    global IMPORT_RUNNING
    if IMPORT_RUNNING:
        log("Import already running — skipping duplicate")
        return
    IMPORT_RUNNING = True
    if bpy.app.background:
        while process_import_batch() is not None:
            pass
    else:
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

            frame = max(1, int(tick * (scene.render.fps / 60.0)) + 1)
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

                # Convert game Z-up coordinates to GLB Y-up (correction_root space)
                game_correction = Euler((math.radians(90.0), 0.0, 0.0))
                converted_state = dict(actor_state)
                pos = Vector(actor_state.get("position", (0.0, 0.0, 0.0)))
                pos.rotate(game_correction)
                converted_state["position"] = (pos.x, pos.y, pos.z)
                rot = actor_state.get("rotation", (0.0, 0.0, 0.0))
                rot_euler = Euler((math.radians(rot[0]), math.radians(rot[1]), math.radians(rot[2])))
                glb_euler = game_rotation_to_glb(rot_euler)
                converted_state["rotation"] = (math.degrees(glb_euler.x), math.degrees(glb_euler.y), math.degrees(glb_euler.z))
                print(f"[ACTOR FRAME] tick={tick} frame={frame} id={actor_id} pos={actor_state.get('position')}")
                set_transform(actor_record["root"], converted_state)

                keyframe_transform(actor_record["root"], frame)

                apply_limb_transforms(
                    actor_record,
                    actor_state,
                    frame
                )

                # Import and track weapon models per actor with visibility keyframes
                weapon_path = actor_state.get("weaponModelPath", "")
                weapon_name = actor_state.get("weaponName", "none")
                actor_weapons = ACTOR_WEAPONS
                last_path = actor_weapons.get(actor_id, "")
                current_wpn = None
                if weapon_path:
                    if weapon_path != last_path:
                        if last_path:
                            old_key = f"{actor_id}:{last_path}"
                            old_wpn = WEAPONS.get(old_key)
                            if old_wpn and old_wpn.get("root"):
                                keyframe_visibility(old_wpn["root"], frame, False)
                        current_wpn = ensure_weapon(actor_id, actor_record, weapon_path)
                        if current_wpn and current_wpn.get("root"):
                            keyframe_visibility(current_wpn["root"], frame, True)
                        actor_weapons[actor_id] = weapon_path
                        print(f"[WEAPON IMPORT] actor={actor_id} weapon={weapon_name} path={weapon_path}")
                    else:
                        # Same weapon as last frame — keep visible
                        wpn_key = f"{actor_id}:{weapon_path}"
                        existing = WEAPONS.get(wpn_key)
                        if existing and existing.get("root"):
                            keyframe_visibility(existing["root"], frame, True)
                elif last_path:
                    # No weapon now — hide previous weapon
                    old_key = f"{actor_id}:{last_path}"
                    old_wpn = WEAPONS.get(old_key)
                    if old_wpn and old_wpn.get("root"):
                        keyframe_visibility(old_wpn["root"], frame, False)

                # Actor tracking light
                light_name = f"{actor_id}_light"
                light_obj = bpy.data.objects.get(light_name)
                if light_obj is None:
                    light_data = bpy.data.lights.new(light_name, 'POINT')
                    light_obj = bpy.data.objects.new(light_name, light_data)
                    light_data.energy = 50.0
                    light_data.color = (1.0, 1.0, 1.0)
                    bpy.data.collections["Mimita_Actors"].objects.link(light_obj)
                    print(f"[REPLAY LIGHT] actor={actor_id} light={light_name}")
                light_pos = converted_state.get("position", (0.0, 0.0, 0.0))
                light_obj.location = (light_pos[0], light_pos[1], light_pos[2] + 2.0)
                light_obj.keyframe_insert(data_path="location", frame=frame)

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
        scene.frame_end = max(1, int(MAX_TICK * (scene.render.fps / 60.0)) + 1)

        print("[REPLAY IMPORT] complete")
        print(f"[REPLAY IMPORT] frames: {scene.frame_start}-{scene.frame_end}")

        # Verify animation data, actions, and weapons
        body_part_names = {"head", "torso", "leftarm", "rightarm", "leftleg", "rightleg"}
        for actor_id, actor_record in ACTORS.items():
            root = actor_record.get("root")
            if root:
                action_name, curve_count, keyframe_count = animation_summary(
                    root
                )
                print(
                    f"[REPLAY SUMMARY] actor={actor_id} "
                    f"root action={action_name} fcurves={curve_count} "
                    f"keyframes={keyframe_count}"
                )

            # Check body parts
            for part in actor_record.get("parts", []):
                norm = normalized_name(part.name)
                if norm not in body_part_names:
                    continue
                action_name, curve_count, keyframe_count = animation_summary(
                    part
                )
                print(
                    f"[REPLAY SUMMARY]   part={part.name} "
                    f"action={action_name} fcurves={curve_count} "
                    f"keyframes={keyframe_count}"
                )

            # Check weapon visibility
            for key, wpn in WEAPONS.items():
                if actor_id in key:
                    wpn_root = wpn.get("root")
                    if wpn_root:
                        print(f"[WEAPON STATUS] key={key} visible={not wpn_root.hide_viewport} loc={tuple(round(v,2) for v in wpn_root.location)} parent={wpn_root.parent.name if wpn_root.parent else 'NONE'}")

        print(f"[REPLAY SUMMARY] actors={len(ACTORS)} weapons={len(WEAPONS)}")

        if VALIDATION_DATA_GLOBAL is not None:
            validate_replay(
                VALIDATION_DATA_GLOBAL,
                REPLAY_PATH_GLOBAL,
                VALIDATION_REPORT_PATH_GLOBAL,
            )
        else:
            log("Validation skipped because no validation data was loaded")

        return None

    return 0.01


main()
