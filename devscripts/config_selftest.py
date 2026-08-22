# 08 22 2026, 13 12
# purpose
# Validates repository-owned JSON configuration and avatar folder contracts.
# Verifies the NPC avatar selection file references a valid relative avatar.json.
# Does NOT launch the game, render OpenGL, or modify any configuration.
# Does NOT validate live hot reload behavior or exported MP4 pixels.
# Does NOT change user-selected account or game settings.
#!/usr/bin/env python3
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]


def rel(path):
    return path.relative_to(ROOT).as_posix()


def load_json(path, errors):
    try:
        with path.open("r", encoding="utf-8") as handle:
            return json.load(handle)
    except Exception as exc:
        errors.append(f"{rel(path)}: JSON parse failed: {exc}")
        return None


def expect_file(path_text, source, errors, base=None):
    if not path_text:
        return
    path = Path(path_text)
    full = (base / path) if base and not path.is_absolute() else path
    full = full if full.is_absolute() else ROOT / full
    if not full.exists():
        errors.append(f"{source}: missing file {path_text}")


def expect_vec3(value, source, errors):
    if not isinstance(value, list) or len(value) < 3:
        errors.append(f"{source}: expected 3-number array")


def validate_procedural(errors):
    root = load_json(ROOT / "config/player-procedural.json", errors)
    if not isinstance(root, dict):
        errors.append("config/player-procedural.json: root must be an object")
        return
    legacy = {
        "leftArmRaise", "leftArmForward", "leftArmTwist",
        "rightArmRaise", "rightArmForward", "rightArmTwist",
        "weaponSwayAmount", "weaponSwaySpeed",
        "idleSwayAmount", "idleSwaySpeed",
        "revolverOffsetX", "revolverOffsetY", "revolverOffsetZ",
        "revolverRotX", "revolverRotY", "revolverRotZ",
        "shotgunOffsetX", "shotgunOffsetY", "shotgunOffsetZ",
        "shotgunRotX", "shotgunRotY", "shotgunRotZ",
    }
    for key in sorted(legacy):
        if key in root:
            errors.append(f"config/player-procedural.json: legacy field {key} must move to owner JSON")


def validate_weapons(errors):
    root = load_json(ROOT / "config/weapons.json", errors)
    if not isinstance(root, dict):
        errors.append("config/weapons.json: root must be an object")
        return 0
    count = 0
    for weapon_id, data in root.items():
        if not isinstance(data, dict):
            errors.append(f"config/weapons.json:{weapon_id}: weapon must be an object")
            continue
        count += 1
        model = data.get("model", {})
        if isinstance(model, dict):
            expect_file(model.get("path", ""), f"weapon {weapon_id}", errors)
        viewmodel = data.get("viewmodel", {})
        if isinstance(viewmodel, dict) and isinstance(viewmodel.get("attachment"), dict):
            attach = viewmodel["attachment"]
            expect_vec3(attach.get("position"), f"weapon {weapon_id} attachment.position", errors)
            expect_vec3(attach.get("rotation_degrees"), f"weapon {weapon_id} attachment.rotation_degrees", errors)
    return count


def validate_animation_parts(parts, source, errors):
    if not isinstance(parts, dict):
        errors.append(f"{source}: parts must be an object")
        return
    for part_name, part in parts.items():
        if not isinstance(part, dict):
            errors.append(f"{source}.{part_name}: part must be an object")
            continue
        for key in ("translation", "rotation"):
            value = part.get(key)
            if value is not None and (not isinstance(value, list) or len(value) < 3):
                errors.append(f"{source}.{part_name}.{key}: expected 3-number array")


def validate_animations(errors):
    root = load_json(ROOT / "config/animations.json", errors)
    if not isinstance(root, dict):
        errors.append("config/animations.json: root must be an object")
        return (0, 0)
    sway = root.get("sway", {})
    if not isinstance(sway, dict):
        errors.append("config/animations.json: sway must be an object")
    else:
        for key in ("weapon_amount", "weapon_speed", "idle_amount", "idle_speed"):
            if not isinstance(sway.get(key), (int, float)):
                errors.append(f"config/animations.json: sway.{key} must be a number")
    clips = root.get("layers", {}).get("animations", {})
    if not isinstance(clips, dict):
        errors.append("config/animations.json: layers.animations must be an object")
        clips = {}
    for name, clip in clips.items():
        keyframes = clip.get("keyframes", []) if isinstance(clip, dict) else []
        if not isinstance(keyframes, list):
            errors.append(f"animation clip {name}: keyframes must be an array")
            continue
        for index, keyframe in enumerate(keyframes):
            validate_animation_parts(keyframe.get("parts", {}), f"animation {name}[{index}]", errors)
    weapons = root.get("weapons", {})
    if not isinstance(weapons, dict):
        errors.append("config/animations.json: weapons must be an object")
        weapons = {}
    for weapon_id, weapon in weapons.items():
        poses = weapon.get("poses", {}) if isinstance(weapon, dict) else {}
        if not isinstance(poses, dict):
            errors.append(f"animation weapon {weapon_id}: poses must be an object")
    return (len(clips), len(weapons))


def validate_avatar(path, errors):
    root = load_json(path, errors)
    if not isinstance(root, dict):
        errors.append(f"{rel(path)}: root must be an object")
        return False
    base = path.parent
    source = rel(path)
    for key in ("name", "version", "advanced_mode", "simple", "advanced", "textures", "colors", "cosmetics"):
        if key not in root:
            errors.append(f"{source}: missing normalized key {key}")
    expect_file(root.get("player_model", ""), source, errors)
    expect_file(root.get("character_model", ""), source, errors)
    textures = root.get("textures", {})
    aliases = textures if isinstance(textures, dict) else {}
    if not isinstance(textures, dict):
        errors.append(f"{source}: textures must be an object")
    for alias, filename in aliases.items():
        expect_file(filename, f"{source} texture {alias}", errors, base)
    simple = root.get("simple", {})
    if not isinstance(simple, dict):
        errors.append(f"{source}: simple must be an object")
    else:
        for key in ("face", "shirt", "pants", "skin"):
            if key not in simple:
                errors.append(f"{source}: simple.{key} missing")
            elif simple[key] and simple[key] not in aliases:
                expect_file(simple[key], f"{source} simple {key}", errors, base)
    advanced = root.get("advanced", {})
    if isinstance(advanced, dict):
        for part in ("head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"):
            for face_name in ("front", "back", "left", "right", "top", "bottom"):
                slot = f"{part}_{face_name}"
                if slot not in advanced:
                    errors.append(f"{source}: advanced.{slot} missing")
        for face, settings in advanced.items():
            if not isinstance(settings, dict):
                errors.append(f"{source} advanced {face}: must be an object")
                continue
            texture = settings.get("texture") if isinstance(settings, dict) else settings
            if isinstance(texture, str) and texture and texture not in aliases:
                expect_file(texture, f"{source} advanced {face}", errors, base)
            for key in ("scale_x", "scale_y", "offset_x", "offset_y", "color", "transparency", "rotation", "stretch_mode"):
                if key not in settings:
                    errors.append(f"{source} advanced {face}: missing {key}")
            if "color" in settings:
                expect_vec3(settings["color"], f"{source} advanced {face}.color", errors)
    else:
        errors.append(f"{source}: advanced must be an object")
    colors = root.get("colors", {})
    if isinstance(colors, dict):
        for part in ("head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"):
            expect_vec3(colors.get(part), f"{source} colors.{part}", errors)
    else:
        errors.append(f"{source}: colors must be an object")
    if not isinstance(root.get("cosmetics"), list):
        errors.append(f"{source}: cosmetics must be an array")
    return True


def validate_avatars(errors):
    avatar_dir = ROOT / "assets/avatars"
    if not avatar_dir.exists():
        errors.append("assets/avatars: directory missing")
        return 0
    count = 0
    for path in sorted(avatar_dir.glob("*/avatar.json")):
        if validate_avatar(path, errors):
            count += 1
    return count


def validate_npc_avatar(errors):
    path = ROOT / "config/npc-avatar.json"
    root = load_json(path, errors)
    if not isinstance(root, dict):
        errors.append("config/npc-avatar.json: root must be an object")
        return
    if not isinstance(root.get("forceAvatar"), bool):
        errors.append("config/npc-avatar.json: forceAvatar must be a boolean")
    forced_path = root.get("forceAvatarPath")
    if not isinstance(forced_path, str):
        errors.append("config/npc-avatar.json: forceAvatarPath must be a string")
        return
    normalized = forced_path.replace("\\\\", "/")
    if not normalized.startswith("assets/avatars/") or not normalized.endswith("/avatar.json"):
        errors.append("config/npc-avatar.json: forceAvatarPath must be a relative assets/avatars/*/avatar.json path")
        return
    expect_file(normalized, "config/npc-avatar.json forceAvatarPath", errors)


def main():
    errors = []
    validate_procedural(errors)
    weapon_count = validate_weapons(errors)
    clip_count, weapon_anim_count = validate_animations(errors)
    avatar_count = validate_avatars(errors)
    validate_npc_avatar(errors)

    print("[CONFIG SELFTEST] weapons:", weapon_count)
    print("[CONFIG SELFTEST] animation clips:", clip_count)
    print("[CONFIG SELFTEST] animated weapons:", weapon_anim_count)
    print("[CONFIG SELFTEST] avatars:", avatar_count)

    if errors:
        print("[CONFIG SELFTEST] FAIL")
        for error in errors:
            print("  " + error)
        return 1
    print("[CONFIG SELFTEST] PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
