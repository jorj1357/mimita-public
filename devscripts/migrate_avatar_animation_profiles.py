"""Move active avatar animation profiles into avatar.json and library clips.

This one-time migration preserves existing state maps and shared includes.
The runtime reads ``avatar.json.animation_profile`` after migration.
"""

import json
import argparse
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AVATARS = ROOT / "assets" / "avatars"
LIBRARY = ROOT / "config" / "animations" / "library" / "avatar"


def read_jsonc(path):
    source = path.read_text(encoding="utf-8")
    output = []
    quoted = escaped = False
    index = 0
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if quoted:
            output.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            index += 1
            continue
        if char == '"':
            quoted = True
            output.append(char)
            index += 1
            continue
        if char == "/" and next_char == "/":
            while index < len(source) and source[index] not in "\r\n":
                index += 1
            continue
        if char == "/" and next_char == "*":
            index += 2
            while index + 1 < len(source) and source[index:index + 2] != "*/":
                index += 1
            index += 2
            continue
        output.append(char)
        index += 1
    return json.loads("".join(output))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main():
    migrated = []
    for source in sorted(AVATARS.glob("*/animations.json")):
        avatar_path = source.with_name("avatar.json")
        if not avatar_path.exists():
            raise ValueError("Missing avatar.json for " + str(source))
        avatar = read_jsonc(avatar_path)
        if "animation_profile" in avatar:
            raise ValueError("Avatar already has an inline profile: " + str(avatar_path))
        old = read_jsonc(source)
        profile = {key: value for key, value in old.items()
                   if key not in ("clips", "include")}
        include = list(old.get("include", []))
        for clip_id, clip in old.get("clips", {}).items():
            output = LIBRARY / source.parent.name / (clip_id + ".json")
            if output.exists():
                raise ValueError("Clip already exists: " + str(output))
            write_json(output, {"version": 2, "clips": {clip_id: clip}})
            include.append(output.relative_to(ROOT).as_posix())
        profile["include"] = include
        avatar.pop("animations", None)
        avatar["animation_profile"] = profile
        write_json(avatar_path, avatar)
        source.unlink()
        migrated.append((source.parent.name, len(old.get("clips", {})), len(include)))
    print(f"Migrated {len(migrated)} avatar profiles")
    for name, clips, includes in migrated:
        print(f"  {name}: {clips} local clips, {includes} library references")


def deduplicate():
    groups = defaultdict(list)
    for path in sorted(LIBRARY.glob("*/*.json")):
        document = json.loads(path.read_text(encoding="utf-8"))
        clip_id, clip = next(iter(document["clips"].items()))
        groups[(clip_id, json.dumps(clip, sort_keys=True))].append(path)
    replacements = {}
    for (clip_id, _), paths in groups.items():
        if len(paths) < 2:
            continue
        target = LIBRARY / "shared" / (clip_id + ".json")
        if target.exists():
            raise ValueError("Shared clip already exists: " + str(target))
        write_json(target, json.loads(paths[0].read_text(encoding="utf-8")))
        for path in paths:
            replacements[path.relative_to(ROOT).as_posix()] = target.relative_to(ROOT).as_posix()
    for avatar_path in sorted(AVATARS.glob("*/avatar.json")):
        avatar = read_jsonc(avatar_path)
        profile = avatar.get("animation_profile")
        if not isinstance(profile, dict):
            continue
        old_include = profile.get("include", [])
        new_include = [replacements.get(path, path) for path in old_include]
        if new_include != old_include:
            profile["include"] = list(dict.fromkeys(new_include))
            write_json(avatar_path, avatar)
    for path in replacements:
        (ROOT / path).unlink()
    print(f"Deduplicated {len(replacements)} avatar clip files into {len(groups) - sum(len(paths) == 1 for paths in groups.values())} shared clips")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dedupe", action="store_true")
    args = parser.parse_args()
    if args.dedupe:
        deduplicate()
    else:
        main()
