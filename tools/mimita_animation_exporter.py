"""Interactive standalone MiMITA animation exporter for Blender 5.x.

Run from ordinary Python for interactive setup:
  py tools/mimita_animation_exporter.py
Or run directly through Blender with a .blend file:
  blender --background "my_actor.blend" --python tools/mimita_animation_exporter.py

Blender supplies bpy and evaluates the open scene; this script prompts for the
rig, per-part Actions, clip settings, and output paths, then writes game JSON.
"""

import json
import math
import os
import re
import subprocess
import sys
import tempfile
import traceback

DEFAULT_BLENDER_EXE = r"C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"

try:
    import bpy
    from mathutils import Euler, Matrix, Quaternion, Vector
except ModuleNotFoundError:
    print("MiMITA Animation Exporter needs Blender to evaluate the .blend animation.")
    blender = DEFAULT_BLENDER_EXE if os.path.isfile(DEFAULT_BLENDER_EXE) else ""
    if not blender:
        print(f"Blender was not found at the default path: {DEFAULT_BLENDER_EXE}")
        blender = input("Example: C:\\Program Files\\Blender Foundation\\Blender 5.0\\blender.exe\nBlender executable path: ").strip().strip('"')
    blend_file = input("Example: C:\\important\\quiet\\blender-projects\\my animation.blend\nPath to the .blend file to export from: ").strip().strip('"')
    if not os.path.isfile(blend_file):
        raise SystemExit("The .blend file was not found.")
    if not blender or not os.path.isfile(blender):
        raise SystemExit("Blender executable was not found; enter the full path to blender.exe.")
    subprocess.run([blender, "--background", blend_file, "--python", os.path.abspath(__file__)], check=True)
    raise SystemExit(0)

PARTS = ("head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg")
DEFAULT_REFERENCE = "assets/entity/player/default/mimita-char-no-animations-v4.glb"


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def resolve_repo_path(value):
    # `//` is Blender's blend-file-relative prefix. Plain paths are repo-relative.
    if value.startswith("//"):
        return bpy.path.abspath(value)
    if os.path.isabs(value):
        return value
    return os.path.join(repo_root(), value)


def part_map(root):
    found = {}
    for obj in root.children:
        match = re.fullmatch(r"(head|torso|leftArm|rightArm|leftLeg|rightLeg)(?:\.\d{3})?", obj.name)
        if match:
            part = match.group(1)
            if part in found:
                raise ValueError("Duplicate canonical part object: " + part)
            found[part] = obj
    missing = [name for name in PARTS if name not in found]
    if missing:
        raise ValueError("Missing direct child part objects: " + ", ".join(missing))
    return found


def marker_tick(frame, first_frame):
    return int(round(frame - first_frame))


def bind_actions(objects, actions):
    """Bind the chosen object Action to each canonical part."""
    saved = {}
    try:
        for part, obj in objects.items():
            action = actions.get(part)
            had_animation_data = obj.animation_data is not None
            anim = obj.animation_data_create()
            saved[obj] = (had_animation_data, anim.action, getattr(anim, "action_slot", None))
            if action is None:
                anim.action = None
                if hasattr(anim, "action_slot"):
                    anim.action_slot = None
                continue
            slots = list(getattr(action, "slots", ()))
            if not slots:
                anim.action = action
                continue
            slot = next((s for s in slots if s.name_display in (obj.name, part)), None)
            if slot is None:
                # Blender may have recorded the object's original name in the slot.
                slot = next((s for s in slots if obj in list(s.users())), None)
            if slot is None:
                raise ValueError(f"Action {action.name} has no slot for object {obj.name}")
            anim.action = action
            anim.action_slot = slot
    except Exception:
        restore_actions(saved)
        raise
    return saved


def restore_actions(saved):
    for obj, (had_animation_data, action, slot) in saved.items():
        if not had_animation_data:
            obj.animation_data_clear()
            continue
        anim = obj.animation_data
        if anim:
            anim.action = action
            if hasattr(anim, "action_slot"):
                anim.action_slot = slot


def load_reference(path):
    if not os.path.isfile(path):
        raise ValueError("Rest reference GLB not found: " + path)
    before = set(bpy.data.objects)
    try:
        bpy.ops.import_scene.gltf(filepath=path)
    except Exception as exc:
        raise ValueError("Could not import rest reference GLB: " + str(exc)) from exc
    imported = set(bpy.data.objects) - before
    root = next((o for o in imported if re.fullmatch(r"plrOrigin(?:\.\d{3})?", o.name)), None)
    if root is None:
        cleanup_import(imported)
        raise ValueError("Reference GLB did not import a plrOrigin root")
    parts = {}
    for child in root.children:
        name = child.name.rsplit("/", 1)[-1].split(".", 1)[0]
        if name in PARTS:
            parts[name] = child
    missing = [p for p in PARTS if p not in parts]
    if missing:
        cleanup_import(imported)
        raise ValueError("Reference GLB is missing parts: " + ", ".join(missing))
    return imported, root, parts


def cleanup_import(objects):
    meshes = {o.data for o in objects if o.type == "MESH" and o.data}
    for obj in objects:
        if obj.name in bpy.data.objects:
            bpy.data.objects.remove(obj, do_unlink=True)
    for mesh in meshes:
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)


def finite_vec(vec):
    values = list(vec)
    if not all(math.isfinite(float(v)) for v in values):
        raise ValueError("A sampled transform contains NaN or infinity")
    # Keep exported files readable and prevent float residue (for example,
    # 3e-08) from obscuring authored values. Runtime normalizes quaternions.
    rounded = [round(float(v), 2) for v in values]
    return [0.0 if value == 0.0 else value for value in rounded]


def collect_markers(scene, start, end, duration, melee):
    markers = []
    for marker in scene.timeline_markers:
        if start <= marker.frame <= end + 1:
            # Animators commonly place action.end on the last keyed frame.
            # Treat that endpoint as exclusive, matching a marker one frame
            # after the final sample (end + 1) and the clip duration.
            if marker.name == "action.end" and marker.frame == end:
                tick = duration
            else:
                tick = marker_tick(marker.frame, start)
            if 0 <= tick <= duration:
                markers.append({"name": marker.name, "tick": tick})
    if melee:
        by_name = {m["name"]: m["tick"] for m in markers}
        needed = ("phase.windup", "phase.active", "phase.recovery", "action.end")
        absent = [name for name in needed if name not in by_name]
        if absent:
            raise ValueError("Melee export requires markers: " + ", ".join(absent))
        ticks = [by_name[name] for name in needed]
        if ticks[0] != 0 or ticks[-1] != duration or not all(a < b for a, b in zip(ticks, ticks[1:])):
            raise ValueError("Melee markers must be ordered, nonempty, begin at tick 0, and end at durationTicks")
    return sorted(markers, key=lambda m: (m["tick"], m["name"]))


def action_keyframes(actions, start, end):
    """Return the union of integral authored transform key frames."""
    frames = {start, end}
    for action in actions.values():
        for layer in getattr(action, "layers", ()):
            for strip in getattr(layer, "strips", ()):
                for slot in getattr(action, "slots", ()):
                    bag = strip.channelbag(slot) if hasattr(strip, "channelbag") else None
                    if not bag:
                        continue
                    for curve in bag.fcurves:
                        if curve.data_path not in ("location", "rotation_euler", "rotation_quaternion"):
                            continue
                        for point in curve.keyframe_points:
                            frame = float(point.co[0])
                            if start <= frame <= end and abs(frame - round(frame)) < 1e-5:
                                frames.add(int(round(frame)))
    return sorted(frames)


def action_curves(action, obj):
    """Return the curves for this object's assigned Action slot only."""
    if action is None:
        return []
    anim = obj.animation_data
    slot = getattr(anim, "action_slot", None) if anim else None
    if hasattr(action, "layers"):
        curves = []
        for layer in action.layers:
            for strip in layer.strips:
                bag = strip.channelbag(slot) if slot and hasattr(strip, "channelbag") else None
                if bag:
                    curves.extend(bag.fcurves)
        return curves
    return list(getattr(action, "fcurves", ()))


def action_transform(obj, action, frame):
    """Evaluate object transform channels directly; Blender 5 background depsgraph
    can leave evaluated matrix_world stale for these imported Action slots."""
    location = list(obj.location)
    scale = list(obj.scale)
    rotation_mode = obj.rotation_mode
    if rotation_mode == "QUATERNION":
        rotation = list(obj.rotation_quaternion)
    else:
        rotation = list(obj.rotation_euler)
    channels = {"location": location, "scale": scale,
                "rotation_quaternion": rotation if rotation_mode == "QUATERNION" else None,
                "rotation_euler": rotation if rotation_mode != "QUATERNION" else None}
    for curve in action_curves(action, obj):
        values = channels.get(curve.data_path)
        if values is not None and 0 <= curve.array_index < len(values):
            values[curve.array_index] = curve.evaluate(frame)
    if rotation_mode == "QUATERNION":
        orient = Quaternion(rotation)
    else:
        orient = Euler(rotation, rotation_mode).to_quaternion()
    return Matrix.LocRotScale(Vector(location), orient, Vector(scale))


def sample_clip(scene, root, objects, rest_root, rest_parts, start, end, frames):
    if any(obj.parent != root for obj in objects.values()):
        raise ValueError("Supported rig requires all six parts to be direct children of plrOrigin")
    if any(obj.constraints for obj in objects.values()):
        raise ValueError("Constraints are not supported by the Action-channel exporter")
    root_action = root.animation_data.action if root.animation_data else None
    root_transform_curves = [
        curve for curve in action_curves(root_action, root)
        if curve.data_path in ("location", "rotation_euler", "rotation_quaternion", "scale")
    ]
    # Static keyed root placement is common in Blender-authored scenes and is
    # not root motion. Ignore it, but reject any channel that changes during
    # the sampled clip because runtime root motion is not supported.
    for curve in root_transform_curves:
        values = [float(curve.evaluate(frame)) for frame in range(start, end + 1)]
        if values and max(values) - min(values) > 1e-5:
            raise ValueError(
                f"Animated plrOrigin/root motion is not supported: "
                f"{curve.data_path}[{curve.array_index}] changes over the clip")
    duration = end - start + 1
    rest_root_inv = rest_root.matrix_world.inverted_safe()
    rest_local = {p: rest_root_inv @ rest_parts[p].matrix_world for p in PARTS}
    # The default Blender rig stores its import-basis correction in the root
    # quaternion field even when rotation_mode is Euler (whose active transform
    # remains identity). Calibrate and read that field explicitly.
    basis_rotation = root.rotation_quaternion.copy()
    expected_basis = Quaternion((math.sqrt(0.5), -math.sqrt(0.5), 0.0, 0.0))
    if min((basis_rotation - expected_basis).magnitude,
           (basis_rotation + expected_basis).magnitude) > 1e-3:
        raise ValueError("Rig root quaternion is not the calibrated default-player Blender-to-game basis (X -90 degrees)")
    basis = basis_rotation.to_matrix().to_4x4()
    basis_inv = basis.inverted_safe()
    print("  Blender-to-game basis quaternion WXYZ: " +
          ", ".join(f"{float(v):.2f}" for v in
                    (basis_rotation.w, basis_rotation.x, basis_rotation.y, basis_rotation.z)))
    selected_actions = {part: (obj.animation_data.action if obj.animation_data else None)
                        for part, obj in objects.items()}
    frames_out = []
    for frame in frames:
        scene.frame_set(frame)
        part_data = {}
        for part in PARTS:
            obj = objects[part]
            action = selected_actions[part]
            if action is None:
                relative = rest_local[part]
            else:
                relative = obj.matrix_parent_inverse @ action_transform(obj, action, frame)
            # Runtime applies normalized rest * pose delta.
            delta = rest_local[part].inverted_safe() @ relative
            # Convert the complete local delta once, so both pivot translation
            # and orientation use the same Blender-to-game basis.
            delta = basis @ delta @ basis_inv
            loc, quat, scale = delta.decompose()
            if any(abs(float(v) - 1.0) > 1e-4 for v in scale):
                part_data[part] = {"translation": finite_vec(loc),
                                   "rotationQuaternionWxyz": finite_vec((quat.w, quat.x, quat.y, quat.z)),
                                   "scale": finite_vec(scale)}
            else:
                part_data[part] = {"translation": finite_vec(loc),
                                   "rotationQuaternionWxyz": finite_vec((quat.w, quat.x, quat.y, quat.z))}
        frames_out.append({"tick": frame - start, "interp": "smooth", "parts": part_data})
    return duration, frames_out


def atomic_json(path, document):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fd, temp_path = tempfile.mkstemp(prefix=".mimita-animation-", suffix=".json", dir=os.path.dirname(path))
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as stream:
            json.dump(document, stream, indent=2, ensure_ascii=False, allow_nan=False)
            stream.write("\n")
        with open(temp_path, "r", encoding="utf-8") as stream:
            json.load(stream)
        os.replace(temp_path, path)
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)


def add_profile_include(path, entry):
    """Add a shared clip path without reformatting the avatar's authored JSON."""
    with open(path, "r", encoding="utf-8") as stream:
        source = stream.read()
    document = json.loads(source)
    includes = document.get("include", [])
    if not isinstance(includes, list):
        raise ValueError("Avatar animation profile 'include' must be an array")
    if entry in includes:
        return False

    encoded = json.dumps(entry, ensure_ascii=False)
    match = re.search(r'("include"\s*:\s*\[)(.*?)(\])', source, re.DOTALL)
    if match:
        body = match.group(2).rstrip()
        separator = "," if body.strip() else ""
        replacement = match.group(1) + body + separator + "\n    " + encoded + "\n  " + match.group(3)
        updated = source[:match.start()] + replacement + source[match.end():]
    else:
        close = source.rfind("}")
        if close < 0:
            raise ValueError("Avatar animation profile is not a JSON object")
        prefix = source[:close].rstrip()
        separator = "," if prefix.rstrip().endswith("}") or prefix.rstrip().endswith("]") or prefix.rstrip().endswith('"') else ""
        updated = prefix + separator + "\n  \"include\": [\n    " + encoded + "\n  ]\n" + source[close:]
    fd, temp_path = tempfile.mkstemp(prefix=".mimita-profile-", suffix=".json", dir=os.path.dirname(path))
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="") as stream:
            stream.write(updated)
        with open(temp_path, "r", encoding="utf-8") as stream:
            json.load(stream)
        os.replace(temp_path, path)
    finally:
        if os.path.exists(temp_path):
            os.remove(temp_path)
    return True


def prompt_choice(label, choices, current=None, explanation="", example=""):
    print(f"\n{label}:")
    if explanation:
        print("  " + explanation)
    if example:
        print("  Example: " + example)
    for index, choice in enumerate(choices, 1):
        suffix = " [current]" if current == choice else ""
        print(f"  {index}. {choice}{suffix}")
    while True:
        answer = input("Choose number" + (f" [default {choices.index(current) + 1}]" if current in choices else "") + ": ").strip()
        if not answer and current in choices:
            return current
        if answer.isdigit() and 1 <= int(answer) <= len(choices):
            return choices[int(answer) - 1]
        print("Enter one of the listed numbers.")


def prompt_text(label, default, explanation="", example=""):
    if explanation:
        print("  " + explanation)
    if example:
        print("  Example: " + example)
    answer = input(f"{label} [{default}]: ").strip()
    return answer or default


def main():
    if not bpy.data.filepath:
        raise RuntimeError("Open a .blend file when launching this script, for example: blender --background actor.blend --python tools/mimita_animation_exporter.py")
    scene = bpy.context.scene
    roots = [obj for obj in bpy.data.objects if obj.name == "plrOrigin" or re.fullmatch(r"plrOrigin\.\d{3}", obj.name)]
    if not roots:
        raise RuntimeError("No plrOrigin root found in the opened Blender file")
    root_names = [obj.name for obj in roots]
    active = bpy.context.view_layer.objects.active
    active_root = active.parent if active and active.parent in roots else active if active in roots else None
    default_root = active_root.name if active_root else ("plrOrigin" if "plrOrigin" in root_names else root_names[0])
    root_name = prompt_choice("Rig root", root_names, default_root,
                              "Choose the character's plrOrigin object. The active limb's parent is preselected when possible.",
                              "For downpunch20261018 choose plrOrigin.036; for downkick20261018 choose plrOrigin.037.")
    root = bpy.data.objects[root_name]
    print(f"Selected rig root: {root.name}")
    objects = part_map(root)
    all_actions = sorted(bpy.data.actions, key=lambda action: action.name)
    if not all_actions:
        raise RuntimeError("No Blender Actions found in the opened file")

    print("\nChoose an Action for each body part. These separate Actions are combined into one game clip.")
    print("Choose 'Rest pose' when that part does not move; it will still be included in every exported tick.")
    actions = {}
    for part, obj in objects.items():
        choices = ["Rest pose"] + [action.name for action in all_actions]
        current = obj.animation_data.action.name if obj.animation_data and obj.animation_data.action else "Rest pose"
        example_action = next((name for name in choices[1:] if name.lower().startswith(part.lower())), choices[1])
        chosen = prompt_choice(part + " Action", choices, current,
                               f"This controls the {part} object. Select Rest pose if it stays still.",
                               f"For {part}, choose {example_action} if that is the action you animated.")
        if chosen != "Rest pose":
            actions[part] = bpy.data.actions[chosen]
    if not actions:
        raise RuntimeError("No Actions selected; nothing to export")

    clip_id = prompt_text("Clip ID", "new_animation",
                          "This is the name the game uses to find the move in the JSON.",
                          "A punch move could be named punch_a; this does not need to match a Blender Action name.")
    clip_type = prompt_choice("Clip type", ["Generic", "Melee"], "Generic",
                              "Choose Melee only for an attack with windup, active, and recovery phases.",
                              "A punch is Melee; an idle or walk cycle is Generic.")
    if clip_type == "Melee":
        print("  Add these Blender timeline markers with these exact names: phase.windup, phase.active, phase.recovery, action.end.")
        print("  Example for a 60-tick punch: windup=0, active=10, recovery=25, action.end=60.")
    sampling = prompt_choice("Pose export mode", ["Authored keyframes", "Dense every tick"], "Authored keyframes",
                             "Authored keyframes writes only keyed poses; MiMITA smoothly interpolates between them. Dense every tick writes Blender's evaluated pose at every tick.",
                             "Keys at frames 1, 15, 30, and 60 become ticks 0, 14, 29, and 59; the default writes four poses.")
    reference_path = prompt_text("Rest reference GLB", DEFAULT_REFERENCE,
                                 "This supplies the neutral character pose that animation offsets are measured from.",
                                 r"C:\mimita-v9\assets\entity\player\default\mimita-char-no-animations-v4.glb")
    output_default = (f"config/animations/library/melee/captainfalcon/{clip_id}.json"
                      if clip_type == "Melee" else f"config/animations/library/{clip_id}.json")
    output_path = prompt_text("Output JSON path", output_default,
                              "MiMITA reads shared clips from config/animations/library; the default is ready for the shared library.",
                              rf"C:\mimita-v9\config\animations\library\{clip_id}.json")
    profile_path = prompt_text("Optional avatar animations.json (blank to skip)", "",
                               "Choose an avatar profile only if you want this avatar to load the shared clip automatically.",
                               r"C:\mimita-v9\assets\avatars\captainfalcon\animations.json")

    saved_frame = scene.frame_current
    saved_actions = {}
    imported = set()
    try:
        fps = scene.render.fps / scene.render.fps_base
        if abs(fps - 60.0) > 1e-5:
            raise ValueError(f"Scene FPS must be 60 (currently {fps:g})")
        start = math.ceil(min(action.frame_range[0] for action in actions.values()))
        end = math.floor(max(action.frame_range[1] for action in actions.values()))
        if end < start:
            raise ValueError("Selected Actions have no integral sample frames")
        print(f"\nSampling at {fps:g} FPS: Blender frames {start}–{end} become ticks 0–{end - start} ({end - start + 1} ticks).")
        saved_actions = bind_actions(objects, actions)
        imported, ref_root, ref_parts = load_reference(resolve_repo_path(reference_path))
        sample_frames = list(range(start, end + 1)) if sampling == "Dense every tick" else action_keyframes(actions, start, end)
        duration, keyframes = sample_clip(scene, root, objects, ref_root, ref_parts, start, end, sample_frames)
        markers = collect_markers(scene, start, end, duration, clip_type == "Melee")
        if not re.fullmatch(r"[A-Za-z0-9_-]+", clip_id):
            raise ValueError("Clip ID may contain only letters, numbers, underscore, and hyphen")
        clip = {"durationTicks": duration, "loop": False, "keyframes": keyframes}
        if markers:
            clip["markers"] = markers
        output = resolve_repo_path(output_path)

        profile_changed = False
        if profile_path:
            resolved_profile = resolve_repo_path(profile_path)
            if not os.path.isfile(resolved_profile):
                raise ValueError("Avatar animation profile not found: " + resolved_profile)
            relative_clip_path = os.path.relpath(output, repo_root()).replace(os.sep, "/")
        atomic_json(output, {"version": 2, "clips": {clip_id: clip}})
        if profile_path:
            profile_changed = add_profile_include(resolved_profile, relative_clip_path)
        print("\nExport complete")
        print(f"  Clip: {clip_id}")
        print(f"  Duration: {duration} ticks")
        print(f"  Poses: {len(keyframes)} ({sampling})")
        print(f"  Exported file: {output}")
        print("  Numeric precision: 2 decimal places; near-zero values written as 0.0")
        print("  Rotation check: evaluated Action channels; root-basis-converted rest-relative WXYZ deltas")
        print("  Review: compare the exported pose with the selected avatar in-game before visual acceptance")
        if profile_changed:
            print(f"  Updated avatar profile: {resolved_profile}")
    finally:
        scene.frame_set(saved_frame)
        restore_actions(saved_actions)
        if imported:
            cleanup_import(imported)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"\nEXPORT FAILED: {exc}", file=sys.stderr)
        traceback.print_exc()
        raise SystemExit(1)
