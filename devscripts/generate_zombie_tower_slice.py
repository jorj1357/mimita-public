import argparse
import json
import math
import random
import random
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GLB = ROOT / "assets/maps/zombietower6.glb"
OUTPUT = ROOT / "config/maps/zombietower6.json"
FIRST_FLOOR = 20
FLOOR_COUNT = 10


def read_gltf_json(path):
    data = path.read_bytes()
    if data[:4] != b"glTF" or struct.unpack_from("<I", data, 4)[0] != 2:
        raise ValueError(f"unsupported GLB header: {path}")
    total = struct.unpack_from("<I", data, 8)[0]
    length, chunk_type = struct.unpack_from("<II", data, 12)
    if chunk_type != 0x4E4F534A or 20 + length > total:
        raise ValueError(f"invalid GLB JSON chunk: {path}")
    return json.loads(data[20:20 + length])


def floor_anchors(document):
    anchors = {}
    for node in document.get("nodes", []):
        name = node.get("name", "")
        for floor in range(FIRST_FLOOR, FIRST_FLOOR + FLOOR_COUNT):
            if name == f"ZombieTower_Floor_{floor:03d}_7684":
                pos = node.get("translation")
                if not isinstance(pos, list) or len(pos) != 3:
                    raise ValueError(f"floor anchor has no translation: {name}")
                anchors[floor] = [float(v) for v in pos]
                break
    expected = set(range(FIRST_FLOOR, FIRST_FLOOR + FLOOR_COUNT))
    missing = sorted(expected - anchors.keys())
    if missing:
        raise ValueError(f"GLB is missing floor anchors: {missing}")
    return anchors


def make_entity(entity_id, kind, position, **fields):
    entity = {
        "id": entity_id,
        "type": kind,
        "position": [round(value, 3) for value in position],
        "size": [1.0, 1.0, 1.0],
        "radius": 1.0,
        "enabled": True,
        "visible": True,
        "oneShot": False,
        "tag": "",
        "monsterPool": "default",
        "pickupId": "",
        "bossId": "",
        "damageType": "generic",
        "spawnCount": 1,
        "maxAlive": 1,
        "damage": 0,
        "damageIntervalTicks": 60,
        "spawnCooldownTicks": 60,
        "checkpointRequirement": 0,
        "order": 0,
        "required": False,
        "activationRadius": 0.0,
        "healthMultiplier": 1.0,
    }
    entity.update(fields)
    return entity


def generate(seed):
    anchors = floor_anchors(read_gltf_json(GLB))
    rng = random.Random(seed)
    entities = []
    start = anchors[FIRST_FLOOR]
    entities.append(make_entity("slice_lobby", "spawnpoint", start, tag="tower_lobby"))
    entities.append(make_entity("slice_intro", "spawnpoint", start, tag="tower_intro"))
    for floor, anchor in sorted(anchors.items()):
        order = floor - FIRST_FLOOR
        entities.append(make_entity(
            f"slice_checkpoint_{floor:03d}", "checkpoint", anchor,
            radius=5.0, order=order, required=True))
        if floor == 20:
            boss_position = [anchor[0] + 30.0, anchor[1], anchor[2]]
            entities.append(make_entity(
                "slice_boss_020", "boss_trigger", boss_position,
                radius=18.0, bossId="zombie_tower_chainsaw_boss_placeholder",
                checkpointRequirement=1, order=order, required=True))
        else:
            angle = rng.random() * 6.283185307179586
            offset = [round(18.0 * math.cos(angle), 3),
                      round(18.0 * math.sin(angle), 3), 0.0]
            position = [anchor[i] + offset[i] for i in range(3)]
            entities.append(make_entity(
                f"slice_encounter_{floor:03d}", "monster_zone", position,
                radius=14.0, activationRadius=24.0, required=True,
                oneShot=True, spawnCount=2, maxAlive=2,
                monsterPool="default", order=order))
    final_anchor = anchors[FIRST_FLOOR + FLOOR_COUNT - 1]
    entities.append(make_entity("slice_end_029", "tower_end", final_anchor,
                                radius=8.0, order=FLOOR_COUNT - 1, required=True))
    return {"blocks": [], "entities": entities}, anchors


def main():
    parser = argparse.ArgumentParser(description="Generate the authored entity data for the Zombie Tower ten-floor content slice.")
    parser.add_argument("--seed", type=int, default=20261101)
    parser.add_argument("--check", action="store_true", help="validate deterministic output without writing")
    args = parser.parse_args()
    config, anchors = generate(args.seed)
    encoded = json.dumps(config, indent=2) + "\n"
    if args.check:
        repeated, repeated_anchors = generate(args.seed)
        ids = [entity["id"] for entity in config["entities"]]
        heights = [anchors[floor][2] for floor in sorted(anchors)]
        if repeated != config or repeated_anchors != anchors:
            raise SystemExit("same seed produced different output")
        if len(ids) != len(set(ids)):
            raise SystemExit("generated entity IDs are not unique")
        if any(abs((b - a) - 100.0) > 0.01 for a, b in zip(heights, heights[1:])):
            raise SystemExit("slice floor anchors are not spaced 100 m apart")
        if json.loads(encoded) != config:
            raise SystemExit("generated map JSON did not round-trip")
        print(f"validated {len(anchors)} GLB floor anchors, {len(ids)} unique entities, and deterministic 100 m spacing (seed={args.seed})")
        return
    OUTPUT.write_text(encoded, encoding="utf-8")
    print(f"wrote {OUTPUT.relative_to(ROOT)}: {len(anchors)} floors, {len(config['entities'])} entities (seed={args.seed})")


if __name__ == "__main__":
    main()
