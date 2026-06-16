import subprocess, sys, os

FFMPEG = r"C:\important\ffmpeg-2025-11-17-git-e94439e49b-full_build\bin\ffmpeg.exe"
FFPROBE = r"C:\important\ffmpeg-2025-11-17-git-e94439e49b-full_build\bin\ffprobe.exe"
BASE = r"C:\important\mimita-priv-v8"
REPLAY = BASE + r"\replays\exports\06-16-2026\11-14-25-clip-duel.mp4"
OUTRO = BASE + r"\assets\video\mimitaoutrov1.webm"
TMP = BASE + r"\replays\exports\_tmp"
OUTRO_NORM = TMP + r"\outro_norm2.mp4"
OUTPUT = TMP + r"\twostage_v3.mp4"
CONCAT = TMP + r"\concat_v3.txt"

# Stage 1
r1 = subprocess.run([FFMPEG, "-y", "-i", OUTRO,
    "-c:v", "libx264", "-preset", "fast", "-pix_fmt", "yuv420p", "-crf", "18",
    "-c:a", "aac", "-b:a", "192k",
    "-vf", "scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2",
    OUTRO_NORM], capture_output=True, text=True)
print(f"Stage1 exit={r1.returncode}")

# Write concat list
with open(CONCAT, "w") as f:
    f.write("file '" + REPLAY + "'\n")
    f.write("file '" + OUTRO_NORM + "'\n")

# Stage 2 with bsf
r2 = subprocess.run([FFMPEG, "-y", "-f", "concat", "-safe", "0", "-i", CONCAT,
    "-c", "copy", "-bsf:v", "h264_mp4toannexb", OUTPUT], capture_output=True, text=True)
print(f"Stage2 exit={r2.returncode}")
for line in r2.stderr.strip().split("\n")[-3:]:
    print(f"  stderr: {line.strip()}")

# Validate
r3 = subprocess.run([FFPROBE, "-v", "error", "-show_entries", "format=duration",
    "-of", "default=noprint_wrappers=1:nokey=1", OUTPUT], capture_output=True, text=True)
dur = r3.stdout.strip()
sz = os.path.getsize(OUTPUT) if os.path.exists(OUTPUT) else 0
valid = r3.returncode == 0 and "moov" not in r3.stderr
print(f"Output: size={sz} duration={dur}s valid={valid}")
if valid and float(dur) > 25:
    print("PASS")
    sys.exit(0)
else:
    print("FAIL")
    sys.exit(1)
