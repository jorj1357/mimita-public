"""Create mimita-game.zip of all runtime files.
The launcher downloads this from GitHub on first run."""

import os
import zipfile
import hashlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

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

def build_zip():
    files = collect_files()
    raw = sum(sz for _, _, sz in files)
    print(f"Collecting {len(files)} files ({raw / 1e6:.1f} MB raw)...")

    zip_path = os.path.join(ROOT, "mimita-game.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for i, (rel, full, _) in enumerate(files):
            zf.write(full, rel)
            if (i + 1) % 100 == 0:
                print(f"  {i + 1}/{len(files)}")

    compressed = os.path.getsize(zip_path)
    ratio = compressed / raw * 100 if raw > 0 else 0
    print(f"\n[OK] {zip_path}")
    print(f"     Raw: {raw / 1e6:.1f} MB")
    print(f"     ZIP: {compressed / 1e6:.1f} MB ({ratio:.0f}%)")

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
