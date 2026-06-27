"""
Migrate old avatar.json files to new outfit.json format.

Usage:
    python devscripts/migrate_avatar_to_outfit.py [--all]
    python devscripts/migrate_avatar_to_outfit.py assets/avatars/<name>/avatar.json

Without arguments, shows which outfits need migration.
With --all, migrates all outfits that lack an outfit.json.
"""

import json
import os
import sys

AVATARS_DIR = "assets/avatars"
PART_KEYS = ["head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"]
FACE_KEYS = ["front", "back", "left", "right", "top", "bottom"]

def convert_avatar_to_outfit(avatar_path):
    """Convert an old avatar.json to the new outfit.json format."""
    with open(avatar_path) as f:
        old = json.load(f)

    outfit = {
        "format": "mimita-outfit",
        "version": 1,
        "name": old.get("name", ""),
        "textures": {},
        "faces": {},
        "colors": {},
        "cosmetics": []
    }

    # Build texture set and face assignments
    simple = old.get("simple", {})

    # Collect all referenced PNG filenames to build texture aliases
    texture_to_alias = {}
    textures_used = set()
    faces = {}

    def add_face(part, face, filename):
        if not filename:
            return
        textures_used.add(filename)
        if filename not in texture_to_alias:
            # Create alias from filename stem
            stem = os.path.splitext(filename)[0]
            alias = stem.replace(" ", "_").replace("-", "_").lower()
            # Deduplicate alias
            base = alias
            counter = 1
            while alias in texture_to_alias.values():
                alias = f"{base}_{counter}"
                counter += 1
            texture_to_alias[filename] = alias
        alias = texture_to_alias[filename]
        if part not in faces:
            faces[part] = {}
        faces[part][face] = alias

    if old.get("advanced_mode") and "advanced" in old:
        adv = old["advanced"]
        for part in PART_KEYS:
            for face in FACE_KEYS:
                key = f"{part}_{face}"
                val = adv.get(key, "")
                add_face(part, face, val)
    else:
        # Simple mode
        skin = simple.get("skin", "")
        face_tex = simple.get("face", "") or skin
        shirt = simple.get("shirt", "")
        pants = simple.get("pants", "")

        for part in PART_KEYS:
            for face in FACE_KEYS:
                if part == "head" and face == "front":
                    add_face(part, face, face_tex)
                elif part == "head":
                    add_face(part, face, skin)
                elif part in ("leftLeg", "rightLeg"):
                    add_face(part, face, pants)
                else:
                    add_face(part, face, shirt)

    # Build texture map
    textures = {}
    for filename, alias in texture_to_alias.items():
        textures[alias] = filename

    outfit["textures"] = textures
    outfit["faces"] = faces

    # Colors
    colors = old.get("colors", {})
    for part in PART_KEYS:
        if part in colors:
            outfit["colors"][part] = colors[part]

    # Cosmetics
    for c in old.get("cosmetics", []):
        slot = c.get("slot", "")
        choice = c.get("choice", "")
        if choice and choice != "none":
            outfit["cosmetics"].append({
                "id": choice,
                "model": f"cosmetics/{choice}.glb",
                "parent": slot if slot in PART_KEYS else "root",
                "position": [0, 0, 0],
                "rotation": [0, 0, 0],
                "scale": [1, 1, 1]
            })

    return outfit


def main():
    do_all = "--all" in sys.argv
    target = None
    for arg in sys.argv[1:]:
        if arg != "--all" and os.path.exists(arg):
            target = arg

    if target:
        paths = [target]
    else:
        # Scan all avatar directories
        paths = []
        for name in sorted(os.listdir(AVATARS_DIR)):
            d = os.path.join(AVATARS_DIR, name)
            if os.path.isdir(d):
                av_path = os.path.join(d, "avatar.json")
                out_path = os.path.join(d, "outfit.json")
                if os.path.exists(av_path) and not os.path.exists(out_path):
                    paths.append(av_path)

    if not paths:
        print("All outfits already have outfit.json. Nothing to migrate.")
        return

    migrated = 0
    for av_path in paths:
        out_path = os.path.join(os.path.dirname(av_path), "outfit.json")
        outfit = convert_avatar_to_outfit(av_path)
        with open(out_path, "w") as f:
            json.dump(outfit, f, indent=2)
        print(f"  Migrated: {out_path}")
        migrated += 1

    print(f"\nDone. {migrated} outfit(s) migrated.")


if __name__ == "__main__":
    main()
