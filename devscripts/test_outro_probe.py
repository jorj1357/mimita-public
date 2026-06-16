import subprocess
import sys
import os

FFPROBE = r"C:\important\ffmpeg-2025-11-17-git-e94439e49b-full_build\bin\ffprobe.exe"
BASE = r"C:\important\mimita-priv-v8"

REPLAY = os.path.join(BASE, r"replays\exports\06-16-2026\11-14-25-clip-duel.mp4")
OUTRO = os.path.join(BASE, r"assets\video\mimitaoutrov1.webm")

def probe(path):
    args = [
        FFPROBE,
        "-v", "error",
        "-show_entries", "format=duration",
        "-of", "default=noprint_wrappers=1:nokey=1",
        path
    ]
    print(f"command args: {args}")
    result = subprocess.run(args, capture_output=True, text=True, shell=False)
    print(f"return code: {result.returncode}")
    print(f"stdout: [{result.stdout.strip()}]")
    print(f"stderr: [{result.stderr.strip()}]")
    dur_str = result.stdout.strip()
    dur = 0.0
    if dur_str:
        try:
            dur = float(dur_str)
        except ValueError:
            print(f"parse error: cannot convert '{dur_str}' to float")
    print(f"parsed duration: {dur}")
    print()
    return dur

replay_dur = probe(REPLAY)
outro_dur = probe(OUTRO)

print(f"replay duration: {replay_dur}")
print(f"outro duration: {outro_dur}")

if replay_dur > 0 and outro_dur > 0:
    print("PASS")
    sys.exit(0)
else:
    print("FAIL")
    sys.exit(1)
