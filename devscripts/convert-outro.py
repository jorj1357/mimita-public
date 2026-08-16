# 08 16 2026
# purpose
# Converts assets/video/mimitaoutrov1.webm to a runtime MP4 asset
# (assets/video/mimitaoutrov1.mp4) that the Windows Media Foundation exporter
# can decode and re-encode onto the end of every clip. Developer-only tool;
# requires a local ffmpeg. Nothing here ships to players.
# Does NOT modify the source .webm or any gameplay/config files.

import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEBM = os.path.join(ROOT, "assets", "video", "mimitaoutrov1.webm")
MP4 = os.path.join(ROOT, "assets", "video", "mimitaoutrov1.mp4")
WIDTH, HEIGHT, FPS = 1280, 720, 60


def find_ffmpeg():
    env = os.environ.get("MIMITA_FFMPEG")
    if env and os.path.isfile(env):
        return env
    config = os.path.join(ROOT, "config", "replay", "ffmpeg-path.json")
    if os.path.isfile(config):
        try:
            import json
            with open(config, "r", encoding="utf-8") as f:
                data = json.load(f)
            if data.get("path") and os.path.isfile(data["path"]):
                return data["path"]
        except Exception:
            pass
    known = os.path.join(
        ROOT, "encoders", "ffmpeg.exe")
    if os.path.isfile(known):
        return known
    exe = shutil.which("ffmpeg")
    if exe:
        return exe
    dev = r"C:\important\ffmpeg-2025-11-17-git-e94439e49b-full_build\bin\ffmpeg.exe"
    if os.path.isfile(dev):
        return dev
    return None


def main():
    if not os.path.isfile(WEBM):
        print(f"[FAIL] source outro not found: {WEBM}")
        sys.exit(1)
    ffmpeg = find_ffmpeg()
    if not ffmpeg:
        print("[FAIL] no local ffmpeg found (set MIMITA_FFMPEG or add ffmpeg to PATH)")
        sys.exit(1)
    vf = f"scale={WIDTH}:{HEIGHT}:force_original_aspect_ratio=decrease,pad={WIDTH}:{HEIGHT}:(ow-iw)/2:(oh-ih)/2"
    cmd = [
        ffmpeg, "-y",
        "-i", WEBM,
        "-c:v", "libx264", "-preset", "fast", "-pix_fmt", "yuv420p",
        "-r", str(FPS),
        "-vf", vf,
        "-c:a", "aac", "-b:a", "192k", "-ar", "48000", "-ac", "2",
        "-movflags", "+faststart",
        MP4,
    ]
    print("[OUTRO CONVERT] running:", " ".join(cmd))
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("[FAIL] ffmpeg conversion failed:")
        print(result.stderr[-4000:])
        sys.exit(1)
    size_mb = os.path.getsize(MP4) / (1024 * 1024)
    print(f"[OK] converted outro -> {MP4} ({size_mb:.2f} MB)")


if __name__ == "__main__":
    main()
