"""Inventory and export six-part Blender Actions without interactive prompts.

Run from the repository root:
  py tools/mimita_animation_batch.py --blend "C:\\...\\mimita sizing 6.blend" --inventory
  py tools/mimita_animation_batch.py --blend "C:\\...\\mimita sizing 6.blend" --export

The inventory is an editable import manifest. Only entries marked ``ready``
are exported; ambiguous and unsupported rigs remain visible in the manifest.
"""

import argparse
import json
import os
import re
import subprocess
import sys

DEFAULT_BLENDER = r"C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"
PARTS = ("head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg")
SOURCE_ID = "mimita_sizing_6"


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def manifest_path():
    return os.path.join(repo_root(), "config", "animations", "imports", SOURCE_ID + ".json")


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blend", required=True, help="Absolute path to the source .blend")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--inventory", action="store_true")
    group.add_argument("--export", action="store_true")
    parser.add_argument("--blender", default=DEFAULT_BLENDER)
    parser.add_argument("--only", action="append", default=[], help="Export only this rig root")
    parser.add_argument("--replace", action="store_true", help="Replace this source's existing clip outputs")
    return parser.parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:])


def slug(value):
    return re.sub(r"_+", "_", re.sub(r"[^a-z0-9]+", "_", value.lower())).strip("_")


def collection_paths(scene):
    paths = {}

    def walk(collection, prefix):
        path = prefix + [collection.name]
        for obj in collection.objects:
            paths.setdefault(obj.name, []).append("/".join(path))
        for child in collection.children:
            walk(child, path)

    walk(scene.collection, [])
    return paths


def classify(paths):
    joined = " ".join(paths).lower()
    if any(token in joined for token in ("archive", "backup", "old", "map", "spawn")):
        return "skip", "archive or non-animation collection"
    if "/anims/" in joined or any(token in joined for token in (
            "walk", "run", "sprint", "idle", "crouch", "jump", "fall", "land",
            "hurt", "attack", "punch", "kick", "dash", "freeze", "equip", "death")):
        return "ready", "named animation collection"
    return "review", "collection purpose is ambiguous"


def inventory(bpy, blend):
    paths = collection_paths(bpy.context.scene)
    entries = []
    used_ids = set()
    for root in sorted(bpy.data.objects, key=lambda obj: obj.name):
        if not re.fullmatch(r"plrOrigin(?:\.\d{3})?", root.name):
            continue
        source_paths = sorted(paths.get(root.name, []))
        status, reason = classify(source_paths)
        parts = {}
        for obj in root.children:
            match = re.fullmatch(r"(head|torso|leftArm|rightArm|leftLeg|rightLeg)(?:\.\d{3})?", obj.name)
            if not match:
                continue
            name = match.group(1)
            if name in parts:
                status, reason = "review", "duplicate canonical body part: " + name
            action = obj.animation_data.action if obj.animation_data else None
            slot = obj.animation_data.action_slot if obj.animation_data and action else None
            parts[name] = {"object": obj.name, "action": action.name if action else None,
                           "slot": slot.identifier if slot else None}
        if any(part not in parts for part in PARTS):
            status, reason = "unsupported", "six canonical parts are not present"
        elif not any(part["action"] for part in parts.values()):
            status, reason = "skip", "no assigned part Actions"
        actions = [bpy.data.actions[p["action"]] for p in parts.values() if p["action"]]
        frame_range = [int(min(a.frame_range[0] for a in actions)),
                       int(max(a.frame_range[1] for a in actions))] if actions else None
        source_name = source_paths[0].split("/")[-1] if source_paths else root.name
        clip_id = SOURCE_ID + "_" + slug(source_name)
        if clip_id in used_ids:
            clip_id += "_" + slug(root.name)
        used_ids.add(clip_id)
        if len(source_paths) != 1 and status == "ready":
            status, reason = "review", "rig belongs to multiple collections"
        entries.append({
            "status": status, "reason": reason, "collection_paths": source_paths,
            "rig_root": root.name, "parts": parts, "frame_range": frame_range,
            "clip_type": "generic", "clip_id": clip_id,
            "loop": any(token in source_name.lower() for token in
                        ("idle", "walk", "run", "sprint", "crouched", "defend")),
            "output": "config/animations/library/imported/" + SOURCE_ID + "/" + clip_id + ".json",
        })
    return {"version": 1, "source_blend": os.path.abspath(blend),
            "scene": bpy.context.scene.name,
            "fps": bpy.context.scene.render.fps / bpy.context.scene.render.fps_base,
            "entries": entries}


def write_manifest(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    temp = path + ".tmp"
    with open(temp, "w", encoding="utf-8") as stream:
        json.dump(data, stream, indent=2, ensure_ascii=False)
        stream.write("\n")
    os.replace(temp, path)


def preserve_reviewed_entries(data, path):
    """Keep human clip decisions when the same source rigs are inventoried again."""
    if not os.path.isfile(path):
        return data
    with open(path, "r", encoding="utf-8") as stream:
        previous = json.load(stream)
    if os.path.normcase(previous.get("source_blend", "")) != os.path.normcase(data["source_blend"]):
        return data
    old_by_root = {entry["rig_root"]: entry for entry in previous.get("entries", [])}
    for entry in data["entries"]:
        old = old_by_root.get(entry["rig_root"])
        if not old or old.get("parts") != entry["parts"] or old.get("collection_paths") != entry["collection_paths"]:
            continue
        for key in ("clip_type", "loop", "clip_id", "output"):
            if key in old:
                entry[key] = old[key]
        # An explicit review decision remains in force for unchanged source data.
        if old.get("status") == "review":
            entry["status"] = "review"
            entry["reason"] = old.get("reason", entry["reason"])
    return data


def export_entries(bpy, manifest, only=(), replace=False):
    # Blender may have an installed add-on with this module name. Load the
    # repository's exporter by absolute path so its rig/pose contract is used.
    import importlib.util
    exporter_path = os.path.join(os.path.dirname(__file__), "mimita_animation_exporter.py")
    spec = importlib.util.spec_from_file_location("mimita_repo_animation_exporter", exporter_path)
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)

    scene = bpy.context.scene
    if abs(manifest["fps"] - 60.0) > 1e-5:
        raise ValueError("Source scene must be 60 FPS")
    if os.path.normcase(os.path.abspath(bpy.data.filepath)) != os.path.normcase(os.path.abspath(manifest["source_blend"])):
        raise ValueError("Opened Blender file does not match the import manifest")
    exported, skipped, failed = [], [], []
    for entry in manifest["entries"]:
        if only and entry["rig_root"] not in only:
            continue
        if entry["status"] != "ready":
            skipped.append({"rig_root": entry["rig_root"], "reason": entry["reason"]})
            continue
        saved_frame, saved_actions, imported = scene.frame_current, {}, set()
        try:
            root = bpy.data.objects[entry["rig_root"]]
            objects = exporter.part_map(root)
            actions = {}
            for name, part in entry["parts"].items():
                if objects[name].name != part["object"]:
                    raise ValueError("part object changed: " + name)
                if part["action"]:
                    action = bpy.data.actions.get(part["action"])
                    if action is None:
                        raise ValueError("Action missing: " + part["action"])
                    actions[name] = action
            if not actions:
                raise ValueError("no Actions selected")
            start = int(min(a.frame_range[0] for a in actions.values()))
            end = int(max(a.frame_range[1] for a in actions.values()))
            if [start, end] != entry["frame_range"]:
                raise ValueError("Action frame range changed; regenerate the inventory")
            saved_actions = exporter.bind_actions(objects, actions)
            imported, ref_root, ref_parts = exporter.load_reference(
                exporter.resolve_repo_path(exporter.DEFAULT_REFERENCE))
            frames = exporter.action_keyframes(actions, start, end)
            duration, keys = exporter.sample_clip(
                scene, root, objects, ref_root, ref_parts, start, end, frames)
            melee = entry["clip_type"] == "melee"
            markers = exporter.collect_markers(scene, start, end, duration, melee)
            clip = {"durationTicks": duration, "loop": entry.get("loop", not melee),
                    "keyframes": keys}
            if markers:
                clip["markers"] = markers
            if melee:
                clip["durationTicks"] = next(marker["tick"] for marker in markers
                                             if marker["name"] == "action.end")
            target = exporter.resolve_repo_path(entry["output"])
            library = os.path.join(repo_root(), "config", "animations", "library")
            if os.path.commonpath((library, target)) != library:
                raise ValueError("output must remain inside the shared clip library")
            if os.path.exists(target):
                if not replace:
                    raise ValueError("output already exists; review before replacing: " + target)
                with open(target, "r", encoding="utf-8") as stream:
                    previous = json.load(stream)
                if list(previous.get("clips", {})) != [entry["clip_id"]]:
                    raise ValueError("existing output has a different clip ID: " + target)
                if melee:
                    exporter.preserve_playback_markers(markers, target, entry["clip_id"], True)
                    clip["durationTicks"] = next(marker["tick"] for marker in markers
                                                 if marker["name"] == "action.end")
                    clip["markers"] = markers
            exporter.atomic_json(target, {"version": 2, "clips": {entry["clip_id"]: clip}})
            exported.append({"clip_id": entry["clip_id"], "output": entry["output"],
                             "poses": len(keys), "duration_ticks": clip["durationTicks"]})
        except Exception as exc:
            failed.append({"rig_root": entry["rig_root"], "reason": str(exc)})
        finally:
            scene.frame_set(saved_frame)
            exporter.restore_actions(saved_actions)
            if imported:
                exporter.cleanup_import(imported)
    return {"exported": exported, "skipped": skipped, "failed": failed}


def main():
    args = parse_args()
    blend = os.path.abspath(args.blend)
    if not os.path.isfile(blend):
        raise SystemExit("Blender source file not found: " + blend)
    try:
        import bpy
    except ModuleNotFoundError:
        if not os.path.isfile(args.blender):
            raise SystemExit("Blender executable not found: " + args.blender)
        mode = "--inventory" if args.inventory else "--export"
        command = [args.blender, "--background", blend, "--python", os.path.abspath(__file__),
                   "--", "--blend", blend, mode, "--blender", args.blender]
        for root in args.only:
            command.extend(("--only", root))
        if args.replace:
            command.append("--replace")
        result = subprocess.run(command, check=False)
        raise SystemExit(result.returncode)

    if args.inventory:
        data = preserve_reviewed_entries(inventory(bpy, blend), manifest_path())
        write_manifest(manifest_path(), data)
        counts = {status: sum(e["status"] == status for e in data["entries"])
                  for status in ("ready", "review", "skip", "unsupported")}
        print(json.dumps({"manifest": manifest_path(), "counts": counts}, indent=2))
    else:
        with open(manifest_path(), "r", encoding="utf-8") as stream:
            manifest = json.load(stream)
        result = export_entries(bpy, manifest, args.only, args.replace)
        print(json.dumps(result, indent=2))
        if result["failed"]:
            raise SystemExit(1)


if __name__ == "__main__":
    main()
