import subprocess, sys, os

FFMPEG = r"C:\important\ffmpeg-2025-11-17-git-e94439e49b-full_build\bin\ffmpeg.exe"
FFPROBE = r"C:\important\ffmpeg-2025-11-17-git-e94439e49b-full_build\bin\ffprobe.exe"
BASE = r"C:\important\mimita-priv-v8"
REPLAY = BASE + r"\replays\exports\06-16-2026\11-14-25-clip-duel.mp4"
OUTRO = BASE + r"\assets\video\mimitaoutrov1.webm"
TMP = BASE + r"\replays\exports\_tmp"
os.makedirs(TMP, exist_ok=True)

def probe(path):
    r = subprocess.run([FFPROBE, "-v", "error", "-show_entries", "format=duration",
        "-of", "default=noprint_wrappers=1:nokey=1", path], capture_output=True, text=True)
    return float(r.stdout.strip())

# Step 1
replay_dur = probe(REPLAY)
print("[AUTO OUTRO] replay duration=%.1f" % replay_dur)

# Step 2: normalize
outro_norm = TMP + "\\auto_norm.mp4"
r2 = subprocess.run([FFMPEG, "-y", "-i", OUTRO,
    "-c:v", "libx264", "-preset", "fast", "-pix_fmt", "yuv420p", "-crf", "18",
    "-c:a", "aac", "-b:a", "192k",
    "-vf", "scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2",
    outro_norm], capture_output=True, text=True)
print("[AUTO OUTRO] stage1 exit=%d" % r2.returncode)

outro_dur = probe(outro_norm)
print("[AUTO OUTRO] outro duration=%.1f" % outro_dur)

expected = replay_dur + outro_dur
print("[AUTO OUTRO] expected duration=%.1f" % expected)

# Step 3: concat
concat_list = TMP + "\\auto_concat.txt"
with open(concat_list, "w") as f:
    f.write("file '" + REPLAY + "'\n")
    f.write("file '" + outro_norm + "'\n")

output = TMP + "\\auto_final.mp4"
r3 = subprocess.run([FFMPEG, "-y", "-f", "concat", "-safe", "0", "-i", concat_list,
    "-c", "copy", "-bsf:v", "h264_mp4toannexb", output], capture_output=True, text=True)
print("[AUTO OUTRO] stage2 exit=%d" % r3.returncode)

final_dur = probe(output)
print("[AUTO OUTRO] final duration=%.1f" % final_dur)

if abs(final_dur - expected) < 0.5:
    print("[AUTO OUTRO] PASS")
    sys.exit(0)
else:
    print("[AUTO OUTRO] FAILED duration mismatch")
    sys.exit(1)
