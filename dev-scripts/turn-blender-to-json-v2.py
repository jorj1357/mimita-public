import os
import sys
import math
import json
import subprocess

# ============================================================
# CONFIG
# ============================================================

BLENDER_EXE = r"C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"

OUT_DIR = r"C:\important\quiet\n\mimita-public\mimita-public\assets\maps\json-converts"

# these are defined in config.h as well
POS_STEP = 0.1
ROT_STEP = 15.0

# ============================================================
# HELPERS (shared logic)
# ============================================================

# 1 decimal place thats ALL U GET BRUH
# also dont do 12.80000000005 
def snap(v, step, decimals):
    return round(round(v / step) * step, decimals)

# minimum is -180, max is 180
def normalize_deg(a):
    a = a % 360.0
    if a >= 180.0:
        a -= 360.0
    return a

# ============================================================
# BLENDER MODE (runs INSIDE Blender)
# ============================================================

def run_inside_blender():
    import bpy # type: ignore

    def snap_position(loc):
        return [
            snap(loc.x, POS_STEP, 1),
            snap(loc.y, POS_STEP, 1),
            snap(loc.z, POS_STEP, 1),
        ]

    def snap_rotation(rot):
        deg = [math.degrees(r) for r in rot]
        norm = [normalize_deg(r) for r in deg]
        return [int(snap(r, ROT_STEP, 0)) for r in norm]
    
    os.makedirs(OUT_DIR, exist_ok=True)

    export = {
        "blocks": [],
        "spheres": []
    }

    for obj in bpy.context.scene.objects:
        if obj.type != "MESH":
            continue

        name = obj.name

        # -------------------------
        # BLOCKS (Cube.*)
        # -------------------------
        if name.startswith("Cube"):
            export["blocks"].append({
                "name": name,
                "position": snap_position(obj.location),
                "rotation": snap_rotation(obj.rotation_euler),
                "size": [
                    snap(obj.dimensions.x, POS_STEP, 1),
                    snap(obj.dimensions.y, POS_STEP, 1),
                    snap(obj.dimensions.z, POS_STEP, 1),
                ]
            })

        # -------------------------
        # SPHERES (Sphere.*)
        # -------------------------
        elif name.startswith("Sphere"):
            radius = max(
                obj.dimensions.x,
                obj.dimensions.y,
                obj.dimensions.z
            ) * 0.5

            export["spheres"].append({
                "name": name,
                "position": snap_position(obj.location),
                "radius": snap(radius, POS_STEP, 1)
            })

    blend_name = os.path.splitext(os.path.basename(bpy.data.filepath))[0]
    out_path = os.path.join(OUT_DIR, blend_name + "-converted.json")

    with open(out_path, "w") as f:
        json.dump(export, f, indent=2)

    print("Exported JSON to:", out_path)

# ============================================================
# LAUNCHER MODE (double-clicked)
# ============================================================

def run_as_launcher():
    print("--------------------------------------------------")
    print(" Blender → JSON Map Converter")
    print("--------------------------------------------------")
    print("Drag a .blend file here and press Enter")
    print()

    blend_path = input("> ").strip().strip('"')

    if not blend_path.lower().endswith(".blend"):
        print("ERROR: Not a .blend file")
        input("Press Enter to exit...")
        return

    if not os.path.exists(blend_path):
        print("ERROR: File does not exist")
        input("Press Enter to exit...")
        return

    cmd = [
        BLENDER_EXE,
        blend_path,
        "--background",
        "--python",
        os.path.abspath(__file__),
    ]

    print()
    print("Running Blender headless...")
    print()

    subprocess.run(cmd)

    print()
    print("DONE.")
    input("Press Enter to close...")

# ============================================================
# ENTRY POINT
# ============================================================

try:
    import bpy # type: ignore
    run_inside_blender()
except ImportError:
    run_as_launcher()
