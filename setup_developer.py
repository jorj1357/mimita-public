# 08 31 2026, 00 00
# purpose
# Downloads and prepares the local Windows build dependencies for MiMITA.
# Stores all downloaded tools under the git-ignored developer directory.
# Exports build environment variables and can build the game immediately.
# DOES NOT modify system PATH, install software globally, or change production files.
# DOES NOT download assets, music, or runtime game data.

import hashlib
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEVELOPER = ROOT / "developer"
TOOLCHAIN = DEVELOPER / "toolchain"
GLFW = DEVELOPER / "glfw"

MINGW_URL = "https://github.com/brechtsanders/winlibs_mingw/releases/download/15.2.0posix-13.0.0-ucrt-r4/winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4.zip"
MINGW_SHA_URL = MINGW_URL + ".sha256"
GLFW_URL = "https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.bin.WIN64.zip"
LIBJUICE_URL = "https://github.com/paullouisageneau/libjuice.git"
LIBJUICE_ZIP_URL = "https://codeload.github.com/paullouisageneau/libjuice/zip/refs/heads/master"


def download(url, destination):
    print(f"[DOWNLOAD] {url}", flush=True)
    print(f"[DOWNLOAD] saving to {destination.relative_to(ROOT)}", flush=True)
    with urllib.request.urlopen(url) as response, open(destination, "wb") as output:
        total = int(response.headers.get("Content-Length", 0))
        copied = 0
        while block := response.read(1024 * 1024):
            output.write(block)
            copied += len(block)
            if total:
                print(f"[DOWNLOAD] {copied / total:.0%}", flush=True)
    print("[DOWNLOAD] complete", flush=True)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def extract(url, archive, destination):
    if not archive.exists():
        download(url, archive)
    else:
        print(f"[CACHE] using {archive.relative_to(ROOT)}", flush=True)
    print(f"[EXTRACT] {archive.relative_to(ROOT)} -> {destination.relative_to(ROOT)}", flush=True)
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as package:
        package.extractall(destination)
    print("[EXTRACT] complete", flush=True)


def locate_glfw_root():
    candidates = list(GLFW.glob("glfw-3.4.bin.WIN64")) + [GLFW]
    for candidate in candidates:
        if (candidate / "include" / "GLFW" / "glfw3.h").is_file():
            return candidate
    return None


def ensure_libjuice():
    header = ROOT / "external" / "libjuice" / "include" / "juice" / "juice.h"
    if header.is_file():
        print("[CACHE] external\\libjuice is ready", flush=True)
        return True
    print(f"[SUBMODULE] fetching {LIBJUICE_URL}", flush=True)
    try:
        result = subprocess.run(
            ["git", "submodule", "update", "--init", "--recursive", "external/libjuice"],
            cwd=ROOT,
            text=True,
        )
    except OSError:
        result = None
    if result is not None and result.returncode == 0 and header.is_file():
        print("[SUBMODULE] external\\libjuice ready", flush=True)
        return True
    print("[SUBMODULE] Git unavailable or fetch failed; using archive download", flush=True)
    archive = DEVELOPER / "libjuice-master.zip"
    try:
        extract(LIBJUICE_ZIP_URL, archive, DEVELOPER / "libjuice-download")
        extracted = DEVELOPER / "libjuice-download" / "libjuice-master"
        target = ROOT / "external" / "libjuice"
        target.mkdir(parents=True, exist_ok=True)
        for item in extracted.iterdir():
            destination = target / item.name
            if item.is_dir():
                shutil.copytree(item, destination, dirs_exist_ok=True)
            else:
                shutil.copy2(item, destination)
        if header.is_file():
            print("[ARCHIVE] external\\libjuice ready", flush=True)
            return True
    except Exception as error:
        print(f"[ARCHIVE] libjuice download failed: {error}", flush=True)
    print(f"Download manually: {LIBJUICE_ZIP_URL}", flush=True)
    print("Extract its contents into: external\\libjuice", flush=True)
    return False


def main():
    print("=== MiMITA developer setup ===", flush=True)
    print(f"[ROOT] {ROOT.name}", flush=True)
    if not ensure_libjuice():
        return 2
    DEVELOPER.mkdir(exist_ok=True)
    mingw_zip = DEVELOPER / "winlibs-mingw.zip"
    glfw_zip = DEVELOPER / "glfw-3.4-win64.zip"

    try:
        extract(MINGW_URL, mingw_zip, TOOLCHAIN)
        expected = urllib.request.urlopen(MINGW_SHA_URL).read().decode().split()[0].lower()
        actual = sha256(mingw_zip)
        print(f"[CHECKSUM] expected={expected}", flush=True)
        print(f"[CHECKSUM] actual  ={actual}", flush=True)
        if actual != expected:
            raise RuntimeError(f"MinGW checksum mismatch: expected {expected}, got {actual}")
        extract(GLFW_URL, glfw_zip, GLFW)
    except Exception as error:
        print(f"\nAutomatic download failed: {error}")
        print("Manual downloads:")
        print(f"  MinGW: {MINGW_URL}")
        print(f"  MinGW checksum: {MINGW_SHA_URL}")
        print(f"  GLFW: {GLFW_URL}")
        print(f"Place MinGW under: {TOOLCHAIN}")
        print(f"Place GLFW under: {GLFW}\\glfw-3.4.bin.WIN64")
        return 2

    mingw_bin = TOOLCHAIN / "mingw64" / "bin"
    glfw_root = locate_glfw_root()
    compiler = mingw_bin / "g++.exe"
    windres = mingw_bin / "windres.exe"
    glfw_lib = glfw_root / "lib-mingw-w64" if glfw_root else None
    if not compiler.is_file() or not windres.is_file() or not glfw_root or not glfw_lib.is_dir():
        print("Downloaded files have an unexpected layout.")
        print(f"MinGW expected at: {compiler}")
        print(f"GLFW expected at: {GLFW}\\glfw-3.4.bin.WIN64")
        return 2

    environment = os.environ.copy()
    environment.update({
        "MIMITA_COMPILER": str(compiler),
        "MIMITA_WINDRES": str(windres),
        "MIMITA_GLFW_INCLUDE": str(glfw_root / "include"),
        "MIMITA_GLFW_LIB": str(glfw_lib),
    })
    print(f"[READY] dependencies: {DEVELOPER.relative_to(ROOT)}", flush=True)
    print(f"[READY] compiler: {compiler.relative_to(ROOT)}", flush=True)
    print(f"[READY] GLFW: {glfw_root.relative_to(ROOT)}", flush=True)
    print("[BUILD] launching buildv2.py (safe 99-hour runtime check)", flush=True)
    result = subprocess.run(
        [sys.executable, "-u", "buildv2.py", "timeout", "356400"],
        cwd=ROOT,
        env=environment,
    )
    print(f"[BUILD] finished with exit code {result.returncode}", flush=True)
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
