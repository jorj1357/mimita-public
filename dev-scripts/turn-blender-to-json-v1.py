import bpy
import json
import math
import os

# C:\important\quiet\n\mimita-public\mimita-public\dev-scripts\turn-blender-to-json-v1.py
# dec 18 2025 
# for collisions to stop breaking 

# -----------------------------
# CONFIG
# -----------------------------
# also in config.h dec 18 2025 
POS_STEP = 0.1          # position snap
ROT_STEP = 15.0         # degrees snap

# -----------------------------
# HELPERS
# -----------------------------
def snap(value, step):
    return round(value / step) * step

def snap_vec(vec, step):
    return [snap(v, step) for v in vec]

def normalize_deg(angle):
    # normalize to [-180, 180)
    a = angle % 360.0
    if a >= 180.0:
        a -= 360.0
    return a

def snap_rotation_deg(rot_rad):
    rot_deg = [math.degrees(r) for r in rot_rad]
    rot_norm = [normalize_deg(r) for r in rot_deg]
    rot_snap = [snap(r, ROT_STEP) for r in rot_norm]
    return rot_snap

def snap_position(pos):
    return snap_vec([pos.x, pos.y, pos.z], POS_STEP)

# -----------------------------
# EXPORT
# -----------------------------
export = {}

for obj in bpy.context.scene.objects:
    if obj.type not in {"MESH", "EMPTY"}:
        continue

    export[obj.name] = {
        "position": snap_position(obj.location),
        "rotation": snap_rotation_deg(obj.rotation_euler)
    }

# -----------------------------
# WRITE FILE
# -----------------------------
path = os.path.join(bpy.path.abspath("//"), "map.json")
with open(path, "w") as f:
    json.dump(export, f, indent=2)

print("Exported map to:", path)
