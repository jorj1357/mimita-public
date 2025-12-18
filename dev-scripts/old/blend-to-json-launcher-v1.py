import subprocess
import os
import sys

# C:\important\quiet\n\mimita-public\mimita-public\dev-scripts\blend-to-json-launcher-v1.py
# dec 18 2025
# launcher for the blender to json script thing 

BLENDER_EXE = r"C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"
EXPORT_SCRIPT = r"C:\important\quiet\n\mimita-public\mimita-public\dev-scripts\turn-blender-to-json-v1.py"

def main():
    print("--------------------------------------------------")
    print(" Blender → JSON Map Exporter")
    print("--------------------------------------------------")
    print("Drag a .blend file into this window and press Enter")
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

    print()
    print("Running Blender headless...")
    print("Blend:", blend_path)
    print()

    cmd = [
        BLENDER_EXE,
        blend_path,
        "--background",
        "--python",
        EXPORT_SCRIPT
    ]

    try:
        subprocess.run(cmd, check=True)
        print()
        print("DONE.")
    except subprocess.CalledProcessError as e:
        print("ERROR running Blender:", e)

    input("Press Enter to close...")

if __name__ == "__main__":
    main()
