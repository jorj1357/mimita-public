import subprocess
import sys
import os
import shutil

FFMPEG = r"C:\important\ffmpeg-2025-11-17-git-e94439e49b-full_build\bin\ffmpeg.exe"
FFPROBE = r"C:\important\ffmpeg-2025-11-17-git-e94439e49b-full_build\bin\ffprobe.exe"
BASE = r"C:\important\mimita-priv-v8"

# Find the latest exported MP4
export_dir = os.path.join(BASE, "replays", "exports")
candidates = []
for root, dirs, files in os.walk(export_dir):
    for f in files:
        if f.endswith(".mp4") and "-with-outro" not in f:
            candidates.append(os.path.join(root, f))
if not candidates:
    print("FAIL: no exported MP4s found")
    sys.exit(1)
candidates.sort(key=lambda p: os.path.getmtime(p), reverse=True)
REPLAY = candidates[0]
OUTRO = os.path.join(BASE, r"assets\video\mimitaoutrov1.webm")
TMP = os.path.join(BASE, r"replays\exports\_tmp")
os.makedirs(TMP, exist_ok=True)

def probe(path):
    result = subprocess.run([FFPROBE, "-v", "error", "-show_entries", "format=duration",
                             "-of", "default=noprint_wrappers=1:nokey=1", path],
                            capture_output=True, text=True)
    try:
        return float(result.stdout.strip())
    except:
        return 0.0

def probe_resolution(path):
    result = subprocess.run([FFPROBE, "-v", "error", "-select_streams", "v:0",
                             "-show_entries", "stream=width,height",
                             "-of", "csv=s=x:p=0", path],
                            capture_output=True, text=True)
    s = result.stdout.strip()
    if 'x' in s:
        parts = s.split('x')
        return int(parts[0]), int(parts[1])
    return 0, 0

def is_valid(path):
    """Check if MP4 has valid moov atom (i.e., is playable)"""
    if not os.path.exists(path):
        return False
    result = subprocess.run([FFPROBE, "-v", "error", "-show_entries", "format=duration",
                             path], capture_output=True, text=True)
    return result.returncode == 0 and "moov" not in result.stderr and "Invalid" not in result.stderr

def run_test(name, cmd_args, expected_duration):
    print(f"\n{'='*60}")
    print(f"TEST: {name}")
    print(f"{'='*60}")
    print(f"command: {cmd_args}")
    result = subprocess.run(cmd_args, capture_output=True, text=True)
    print(f"return code: {result.returncode}")
    if result.stderr.strip():
        # Print last 5 lines of stderr
        lines = result.stderr.strip().split('\n')
        for line in lines[-5:]:
            print(f"stderr: {line.strip()}")
    output = cmd_args[cmd_args.index('-y') + 1] if '-y' in cmd_args else None
    if output:
        if output.startswith('"'):
            output = output.strip('"')
        valid = is_valid(output)
        dur = probe(output)
        size = os.path.getsize(output) if os.path.exists(output) else 0
        print(f"output: {output}")
        print(f"output exists: {os.path.exists(output)}")
        print(f"output size: {size}")
        print(f"output duration: {dur:.1f}s")
        print(f"output valid: {valid}")
        if expected_duration:
            print(f"expected: ~{expected_duration:.1f}s")
            if abs(dur - expected_duration) < 0.5 and valid:
                print(">>> PASS")
                return True
            else:
                print(">>> FAIL")
                return False
        if valid:
            print(">>> PASS (valid)")
            return True
        print(">>> FAIL")
        return False
    return result.returncode == 0

# Probe inputs
replay_dur = probe(REPLAY)
outro_dur = probe(OUTRO)
replay_w, replay_h = probe_resolution(REPLAY)
outro_w, outro_h = probe_resolution(OUTRO)

print(f"REPLAY: {REPLAY}")
print(f"  duration={replay_dur:.1f}s  resolution={replay_w}x{replay_h}  size={os.path.getsize(REPLAY)}")
print(f"OUTRO: {OUTRO}")
print(f"  duration={outro_dur:.1f}s  resolution={outro_w}x{outro_h}  size={os.path.getsize(OUTRO)}")
print(f"EXPECTED FINAL: ~{replay_dur + outro_dur:.1f}s")

OUTPUT_FILTER = os.path.join(TMP, "test_filter_complex.mp4")
OUTPUT_DEMUXER = os.path.join(TMP, "test_demuxer.mp4")
OUTPUT_TWOSTAGE = os.path.join(TMP, "test_twostage.mp4")
OUTRO_NORMALIZED = os.path.join(TMP, "outro_normalized.mp4")
CONCAT_LIST = os.path.join(TMP, "concat_list.txt")

# Test A: Current filter_complex approach
success = run_test("Filter complex concat (current approach)", [
    FFMPEG, "-y",
    "-i", REPLAY,
    "-i", OUTRO,
    "-filter_complex", "[0:v][0:a][1:v][1:a]concat=n=2:v=1:a=1[outv][outa]",
    "-map", "[outv]", "-map", "[outa]",
    "-c:v", "libx264", "-preset", "fast", "-pix_fmt", "yuv420p", "-crf", "18",
    "-c:a", "aac", "-b:a", "192k",
    OUTPUT_FILTER
], replay_dur + outro_dur)

# Test B: Two-stage approach — stage 1 normalize outro, stage 2 concat demuxer
print(f"\n{'='*60}")
print("TEST: Two-stage (normalize outro + concat demuxer)")
print(f"{'='*60}")

# Stage 1: Normalize outro to match replay
stage1_args = [
    FFMPEG, "-y",
    "-i", OUTRO,
    "-c:v", "libx264", "-preset", "fast", "-pix_fmt", "yuv420p", "-crf", "18",
    "-c:a", "aac", "-b:a", "192k",
]
if replay_w > 0 and replay_h > 0:
    stage1_args += ["-vf", f"scale={replay_w}:{replay_h}:force_original_aspect_ratio=decrease,pad={replay_w}:{replay_h}:(ow-iw)/2:(oh-ih)/2"]
stage1_args.append(OUTRO_NORMALIZED)

print(f"Stage 1 command: {stage1_args}")
r1 = subprocess.run(stage1_args, capture_output=True, text=True)
print(f"Stage 1 return code: {r1.returncode}")
if r1.stderr.strip():
    for line in r1.stderr.strip().split('\n')[-3:]:
        print(f"stderr: {line.strip()}")

norm_dur = probe(OUTRO_NORMALIZED)
norm_valid = is_valid(OUTRO_NORMALIZED)
print(f"Normalized outro valid: {norm_valid}")
print(f"Normalized outro duration: {norm_dur:.1f}s")

if norm_valid:
    # Stage 2: Concat via demuxer with -c copy
    with open(CONCAT_LIST, 'w') as f:
        f.write(f"file '{REPLAY}'\n")
        f.write(f"file '{OUTRO_NORMALIZED}'\n")

    stage2_args = [
        FFMPEG, "-y", "-f", "concat", "-safe", "0",
        "-i", CONCAT_LIST,
        "-c", "copy",
        OUTPUT_TWOSTAGE
    ]
    print(f"Stage 2 command: {stage2_args}")
    r2 = subprocess.run(stage2_args, capture_output=True, text=True)
    print(f"Stage 2 return code: {r2.returncode}")
    if r2.stderr.strip():
        for line in r2.stderr.strip().split('\n')[-5:]:
            print(f"stderr: {line.strip()}")
    
    twostage_valid = is_valid(OUTPUT_TWOSTAGE)
    twostage_dur = probe(OUTPUT_TWOSTAGE)
    twostage_size = os.path.getsize(OUTPUT_TWOSTAGE) if os.path.exists(OUTPUT_TWOSTAGE) else 0
    print(f"two-stage exists: {os.path.exists(OUTPUT_TWOSTAGE)}")
    print(f"two-stage size: {twostage_size}")
    print(f"two-stage duration: {twostage_dur:.1f}s")
    print(f"two-stage valid: {twostage_valid}")
    expected = replay_dur + norm_dur
    if twostage_valid and abs(twostage_dur - expected) < 0.5:
        print(f">>> TWO-STAGE PASS ({twostage_dur:.1f}s ≈ {expected:.1f}s)")
        success = True
    else:
        print(">>> TWO-STAGE FAIL")
else:
    print(">>> TWO-STAGE FAIL (normalization failed)")

# Report final verdict
print(f"\n{'='*60}")
print("SUMMARY")
print(f"{'='*60}")
print(f"Replay: {os.path.basename(REPLAY)} ({replay_dur:.1f}s {replay_w}x{replay_h})")
print(f"Outro:  {os.path.basename(OUTRO)} ({outro_dur:.1f}s {outro_w}x{outro_h})")

if success:
    print("VERDICT: PASS - at least one concat method works")
    sys.exit(0)
else:
    print("VERDICT: FAIL - no concat method produced valid output")
    sys.exit(1)
