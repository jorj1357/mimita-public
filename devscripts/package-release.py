"""Package a verified MiMITA release ZIP from an explicit release executable."""

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUNTIME_DIRS = ("assets", "config", "shaders", "Characters")
RUNTIME_FILES = (
    "glfw3.dll",
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "tools/ffmpeg.exe",
)
EXCLUDED_PREFIXES = (
    "assets/sound/music/ingame/donttrack/",
    "characters/_template/",
    "config/accounts/",
)
EXCLUDED_PATHS = {
    "config/profiles.json",
    "config/current-profile.json",
    "assets/font/mingliu.ttf",
    "assets/font/notoserifcjktc-regular.otf",
}
EXCLUDED_SUFFIXES = (".kra", ".png~")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def excluded(relative: str) -> bool:
    lower = relative.lower()
    if lower == "config/accounts/default.json":
        return False
    if lower in EXCLUDED_PATHS or lower.endswith(EXCLUDED_SUFFIXES):
        return True
    if "/donttrack/" in lower or lower.rsplit("/", 1)[-1].startswith("readme"):
        return True
    return lower.startswith(EXCLUDED_PREFIXES)


def runtime_files(executable: Path) -> list[tuple[str, Path]]:
    files = [("mimita.exe", executable), ("version.txt", ROOT / "version.txt")]
    for relative in RUNTIME_FILES:
        path = ROOT.joinpath(*relative.split("/"))
        if not path.is_file():
            raise FileNotFoundError(f"required runtime file is missing: {relative}")
        files.append((relative, path))
    for directory in RUNTIME_DIRS:
        base = ROOT / directory
        if not base.is_dir():
            raise FileNotFoundError(f"required runtime directory is missing: {directory}")
        for path in sorted(item for item in base.rglob("*") if item.is_file()):
            relative = path.relative_to(ROOT).as_posix()
            if not excluded(relative):
                files.append((relative, path))
    return files


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    executable = args.executable.resolve()
    expected_version = (ROOT / "version.txt").read_text(encoding="utf-8").strip()
    if args.version != expected_version:
        raise ValueError(f"--version {args.version} does not match version.txt {expected_version}")
    if not executable.is_file():
        raise FileNotFoundError(f"release executable not found: {executable}")
    if executable.stat().st_size > 60 * 1024 * 1024:
        raise ValueError("refusing to package a debug-sized executable; build release mode first")

    config_version = json.loads((ROOT / "config/version.json").read_text(encoding="utf-8"))
    config_version_string = "{major}.{minor}.{patch}".format(**config_version)
    if config_version_string != args.version or config_version.get("prerelease"):
        raise ValueError("config/version.json must identify this final public release")

    output = args.output or ROOT / "dist" / f"mimita-game-v{args.version}.zip"
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    files = runtime_files(executable)
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for relative, path in files:
            archive.write(path, relative)

    print(json.dumps({
        "output": str(output),
        "files": len(files),
        "size_bytes": output.stat().st_size,
        "sha256": sha256(output),
        "executable_sha256": sha256(executable),
    }, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
