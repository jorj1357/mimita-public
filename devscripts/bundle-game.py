"""Bundle game files into MimitaLauncher.exe as a self-extracting archive.
Creates mimita-game.zip, appends it to MimitaLauncher.exe with magic marker.
Final output: MimitaLauncher.exe (launcher + compressed game files, ~68 MB)."""

import os
import zipfile
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAGIC = b"MIMIZIP!"

def collect_files():
    files = []
    for name in ["mimita.exe", "MimitaLauncher.exe", "version.txt",
                  "glfw3.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll",
                  "libwinpthread-1.dll"]:
        full = os.path.join(ROOT, name)
        if os.path.isfile(full):
            files.append((name, full, os.path.getsize(full)))

    for subdir in ["assets", "config", "shaders", "Characters"]:
        d = os.path.join(ROOT, subdir)
        if os.path.isdir(d):
            for dirpath, dirnames, filenames in os.walk(d):
                for fn in sorted(filenames):
                    full = os.path.join(dirpath, fn)
                    rel = os.path.relpath(full, ROOT).replace("\\", "/")
                    files.append((rel, full, os.path.getsize(full)))
    return files

def build_bundle():
    launcher_path = os.path.join(ROOT, "MimitaLauncher.exe")
    zip_path = os.path.join(ROOT, "mimita-game.zip")
    out_path = os.path.join(ROOT, "MimitaLauncher.exe")

    if not os.path.isfile(launcher_path):
        print(f"[FAIL] Launcher not found: {launcher_path}")
        sys.exit(1)

    files = collect_files()
    raw = sum(sz for _, _, sz in files)
    print(f"Collecting {len(files)} files ({raw / 1e6:.1f} MB raw)...")

    # Create ZIP
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for i, (rel, full, _) in enumerate(files):
            zf.write(full, rel)
            if (i + 1) % 100 == 0:
                print(f"  {i + 1}/{len(files)}")

    zip_size = os.path.getsize(zip_path)
    print(f"  ZIP: {zip_size / 1e6:.1f} MB ({zip_size / raw * 100:.0f}%)")

    # Append ZIP to launcher with magic marker at the END for easy detection
    # Layout: [EXE data][ZIP data][ZIP size:4][MAGIC:8]
    with open(launcher_path, "rb") as exe:
        exe_data = exe.read()

    with open(zip_path, "rb") as zf:
        zip_data = zf.read()

    with open(out_path, "wb") as out:
        out.write(exe_data)
        out.write(zip_data)
        out.write(struct.pack("<I", zip_size))
        out.write(MAGIC)

    final_size = os.path.getsize(out_path)
    print(f"\n[OK] Self-contained launcher: {out_path}")
    print(f"     Launcher: {len(exe_data) / 1e6:.1f} MB")
    print(f"     ZIP:      {zip_size / 1e6:.1f} MB")
    print(f"     Total:    {final_size / 1e6:.1f} MB")

    # Clean up standalone ZIP (already embedded)
    os.remove(zip_path)

if __name__ == "__main__":
    build_bundle()
