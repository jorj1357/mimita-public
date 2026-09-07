# 2026-09-07T14:46:50Z  jorj - tweak this so that it works for mimita replays
# so u can export something, its like 20mb, run that file thru this, it compresses to 8mb
# now u can upload to discord and stuff easily

import os
import subprocess
from pathlib import Path

TARGET_MB = 8
AUDIO_KBPS = 96

def run(cmd):
    subprocess.run(cmd, check=True)

def get_duration(path):
    cmd = [
        "ffprobe", "-v", "error",
        "-show_entries", "format=duration",
        "-of", "default=noprint_wrappers=1:nokey=1",
        str(path)
    ]
    return float(subprocess.check_output(cmd).decode().strip())

def compress_video(input_path):
    input_path = Path(input_path.strip().strip('"'))
    if not input_path.exists():
        print("File not found.")
        input("Press Enter to exit...")
        return

    duration = get_duration(input_path)

    target_kbits = TARGET_MB * 8192
    video_kbps = int((target_kbits / duration) - AUDIO_KBPS)

    if video_kbps < 100:
        video_kbps = 100

    output_path = input_path.with_name(input_path.stem + "_8mb.mp4")

    print(f"\nDuration: {duration:.2f}s")
    print(f"Video bitrate: {video_kbps} kbps")
    print(f"Output: {output_path}\n")

    cmd = [
        "ffmpeg", "-y",
        "-hwaccel", "cuda",
        "-i", str(input_path),

        "-c:v", "h264_nvenc",
        "-preset", "p4",
        "-b:v", f"{video_kbps}k",
        "-maxrate", f"{video_kbps}k",
        "-bufsize", f"{video_kbps * 2}k",

        "-c:a", "aac",
        "-b:a", f"{AUDIO_KBPS}k",

        "-movflags", "+faststart",
        str(output_path)
    ]

    run(cmd)

    size_mb = output_path.stat().st_size / (1024 * 1024)
    print(f"\nDone. Final size: {size_mb:.2f} MB")

    if size_mb > TARGET_MB:
        print("Slightly over 8MB. Run again on the output file if needed.")

    input("\nPress Enter to exit...")

print("Drop video file here, then press Enter:")
video = input("> ")
compress_video(video)