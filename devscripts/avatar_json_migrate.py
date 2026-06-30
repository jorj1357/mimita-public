#!/usr/bin/env python3
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
AVATAR_DIR = ROOT / "assets" / "avatars"
PARTS = ("head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg")
FACES = ("front", "back", "left", "right", "top", "bottom")
COLOR_PARTS = PARTS


def default_face(texture=""):
    return {
        "texture": texture,
        "scale_x": 1.0,
        "scale_y": 1.0,
        "offset_x": 0,
        "offset_y": 0,
        "color": [1.0, 1.0, 1.0],
        "transparency": 0.0,
        "rotation": 0,
        "stretch_mode": "stretch",
    }


def normalize_face(value, fallback=""):
    if isinstance(value, str):
        return default_face(value)
    if not isinstance(value, dict):
        return default_face(fallback)
    face = default_face(value.get("texture", fallback))
    for key in ("scale_x", "scale_y", "offset_x", "offset_y", "color",
                "transparency", "rotation", "stretch_mode"):
        if key in value:
            face[key] = value[key]
    return face


def simple_defaults(root, advanced):
    simple = root.get("simple") if isinstance(root.get("simple"), dict) else {}
    return {
        "face": simple.get("face") or texture_of(advanced.get("head_front")) or "",
        "shirt": simple.get("shirt") or texture_of(advanced.get("torso_front")) or "",
        "pants": simple.get("pants") or texture_of(advanced.get("leftLeg_front")) or "",
        "skin": simple.get("skin") or texture_of(advanced.get("head_back")) or texture_of(advanced.get("head_front")) or "",
    }


def texture_of(value):
    if isinstance(value, str):
        return value
    if isinstance(value, dict):
        return value.get("texture", "")
    return ""


def fallback_texture(simple, part, face):
    if part == "head" and face == "front":
        return simple["face"] or simple["skin"]
    if part == "head":
        return simple["skin"]
    if part in ("torso", "leftArm", "rightArm"):
        return simple["shirt"]
    return simple["pants"]


def normalize_advanced(root, simple):
    old = root.get("advanced") if isinstance(root.get("advanced"), dict) else {}
    advanced = {}
    for part in PARTS:
        for face in FACES:
            key = f"{part}_{face}"
            advanced[key] = normalize_face(old.get(key), fallback_texture(simple, part, face))
    return advanced


def collect_textures(root, base):
    textures = root.get("textures") if isinstance(root.get("textures"), dict) else {}
    result = {
        str(k): str(v)
        for k, v in textures.items()
        if isinstance(v, str) and v and texture_file_exists(base, v)
    }
    seen_values = set(result.values())
    for value in root["simple"].values():
        add_texture(result, seen_values, value)
    for face in root["advanced"].values():
        add_texture(result, seen_values, face.get("texture", ""))
    return dict(sorted(result.items()))


def add_texture(result, seen_values, filename):
    if not isinstance(filename, str) or not filename or filename in result or filename in seen_values:
        return
    key = Path(filename).stem or filename
    candidate = key
    index = 2
    while candidate in result and result[candidate] != filename:
        candidate = f"{key}_{index}"
        index += 1
    result[candidate] = filename
    seen_values.add(filename)


def texture_file_exists(base, filename):
    path = Path(filename)
    full = path if path.is_absolute() else base / path
    return full.exists()


def normalize_colors(root):
    colors = root.get("colors") if isinstance(root.get("colors"), dict) else {}
    return {part: colors.get(part, [1.0, 1.0, 1.0]) for part in COLOR_PARTS}


def normalize_avatar(path):
    root = json.loads(path.read_text(encoding="utf-8"))
    advanced_old = root.get("advanced") if isinstance(root.get("advanced"), dict) else {}
    normalized = {
        "name": root.get("name") or path.parent.name,
        "version": root.get("version", 1),
        "advanced_mode": bool(root.get("advanced_mode", bool(advanced_old))),
    }
    for key in ("player_model", "character_model"):
        if isinstance(root.get(key), str) and root[key]:
            normalized[key] = root[key]
    normalized["simple"] = simple_defaults(root, advanced_old)
    normalized["advanced"] = normalize_advanced(root, normalized["simple"])
    normalized["textures"] = collect_textures(normalized | {"textures": root.get("textures", {})}, path.parent)
    normalized["colors"] = normalize_colors(root)
    normalized["cosmetics"] = root.get("cosmetics") if isinstance(root.get("cosmetics"), list) else []
    if isinstance(root.get("active_preset"), str) and root["active_preset"]:
        normalized["active_preset"] = root["active_preset"]
    return normalized


def main():
    if not AVATAR_DIR.exists():
        print("assets/avatars does not exist")
        return 1
    changed = 0
    for path in sorted(AVATAR_DIR.glob("*/avatar.json")):
        before = path.read_text(encoding="utf-8")
        data = normalize_avatar(path)
        after = json.dumps(data, indent=2) + "\n"
        if after != before:
            path.write_text(after, encoding="utf-8")
            changed += 1
            print(f"[AVATAR MIGRATE] wrote {path.relative_to(ROOT).as_posix()}")
    print(f"[AVATAR MIGRATE] changed {changed} avatar file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
