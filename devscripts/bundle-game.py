"""Create mimita-game.zip of all runtime files for the launcher to download.
Output: mimita-game.zip — uploaded alongside MimitaLauncher.exe to GitHub Releases."""

import os
import zipfile
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def collect_files():
    files = []

    # Root files
    for name in ["mimita.exe", "MimitaLauncher.exe", "version.txt",
                  "glfw3.dll", "libgcc_s_seh-1.dll", "libstdc++-6.dll",
                  "libwinpthread-1.dll"]:
        full = os.path.join(ROOT, name)
        if os.path.isfile(full):
            files.append((name, full, os.path.getsize(full)))

    # assets/ directory (all files)
    if os.path.isdir(os.path.join(ROOT, "assets")):
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, "assets")):
            for fn in sorted(filenames):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, ROOT).replace("\\", "/")
                files.append((rel, full, os.path.getsize(full)))

    # config/ directory (all files)
    if os.path.isdir(os.path.join(ROOT, "config")):
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, "config")):
            for fn in sorted(filenames):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, ROOT).replace("\\", "/")
                files.append((rel, full, os.path.getsize(full)))

    # shaders/ directory (all files)
    if os.path.isdir(os.path.join(ROOT, "shaders")):
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, "shaders")):
            for fn in sorted(filenames):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, ROOT).replace("\\", "/")
                files.append((rel, full, os.path.getsize(full)))

    # Characters/ directory (all files)
    if os.path.isdir(os.path.join(ROOT, "Characters")):
        for dirpath, dirnames, filenames in os.walk(os.path.join(ROOT, "Characters")):
            for fn in sorted(filenames):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, ROOT).replace("\\", "/")
                files.append((rel, full, os.path.getsize(full)))

    return files

def build_zip():
    files = collect_files()
    total_uncompressed = sum(sz for _, _, sz in files)
    print(f"Collecting {len(files)} files ({total_uncompressed / 1e6:.1f} MB raw)...")

    zip_path = os.path.join(ROOT, "mimita-game.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for i, (rel_path, full_path, size) in enumerate(files):
            zf.write(full_path, rel_path)
            if (i + 1) % 100 == 0:
                print(f"  {i + 1}/{len(files)}")

    compressed = os.path.getsize(zip_path)
    ratio = compressed / total_uncompressed * 100 if total_uncompressed > 0 else 0
    print(f"\n[OK] Created {zip_path}")
    print(f"     Files: {len(files)}")
    print(f"     Uncompressed: {total_uncompressed / 1e6:.1f} MB")
    print(f"     Compressed:   {compressed / 1e6:.1f} MB ({ratio:.0f}%)")

    # Compute SHA-256
    import hashlib
    sha = hashlib.sha256()
    with open(zip_path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            sha.update(chunk)
    print(f"     SHA-256: {sha.hexdigest()}")

if __name__ == "__main__":
    build_zip()
