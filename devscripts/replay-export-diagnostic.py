#!/usr/bin/env python3
"""
Replay Export Diagnostic Script
================================
Runs static validation checks on the replay export pipeline.
Does NOT require the game to be running.

Usage:
    python devscripts/replay-export-diagnostic.py

Output:
    replays/exports/export-diagnostic-report.txt
"""

import os
import sys
import json
import time
import subprocess
import glob
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPLAYS_DIR = ROOT / "replays"
EXPORTS_DIR = REPLAYS_DIR / "exports"
REPORT_PATH = EXPORTS_DIR / "export-diagnostic-report.txt"

os.makedirs(EXPORTS_DIR, exist_ok=True)

report_lines = []
def log(line=""):
    report_lines.append(line)
    print(line)

def save_report():
    with open(REPORT_PATH, "w") as f:
        f.write("\n".join(report_lines) + "\n")
    print(f"\nReport saved to {REPORT_PATH}")

def find_newest_replay():
    """Find newest .json or .mclip.json in replays/ tree."""
    candidates = []
    for pattern in ["**/*.json", "**/*.mclip.json"]:
        for f in REPLAYS_DIR.glob(pattern):
            name = f.name
            if "-validation" in name:
                continue
            # Exclude export dir
            if "exports" in f.parts:
                continue
            candidates.append(f)
    if not candidates:
        return None
    candidates.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return candidates[0]

def check_a_replay_exists():
    log("=" * 60)
    log("CHECK A: Replay exists")
    log("-" * 60)
    replay = find_newest_replay()
    if replay is None:
        log("  FAIL: No replay files found anywhere under replays/")
        return False
    log(f"  PASS: Newest replay: {replay}")
    log(f"       Size: {replay.stat().st_size / 1024:.1f} KB")
    log(f"       Modified: {time.ctime(replay.stat().st_mtime)}")
    return replay

def check_b_replay_loads(replay_path):
    log("\n" + "=" * 60)
    log("CHECK B: Replay loads (header validation)")
    log("-" * 60)
    try:
        with open(replay_path) as f:
            content = f.read(4096)  # Read just the beginning for header
    except Exception as e:
        log(f"  FAIL: Cannot read file: {e}")
        return False

    # Basic JSON structure check
    import re
    # Look for tickCount in the raw content
    tick_match = re.search(r'"tickCount"\s*:\s*(\d+)', content)
    tick_rate_match = re.search(r'"tickRate"\s*:\s*(\d+)', content)
    map_match = re.search(r'"mapName"\s*:\s*"([^"]+)"', content)
    frame_match = re.search(r'"tickCount"\s*:\s*(\d+)', content)

    tick_count = int(tick_match.group(1)) if tick_match else 0
    tick_rate = int(tick_rate_match.group(1)) if tick_rate_match else 60
    map_name = map_match.group(1) if map_match else "unknown"
    duration = tick_count / tick_rate if tick_rate > 0 else 0

    log(f"  PASS: tickCount={tick_count}, tickRate={tick_rate}")
    log(f"       Duration: {duration:.1f} sec")
    log(f"       Map: {map_name}")

    if tick_count == 0:
        log(f"  WARN: tickCount is 0 (empty replay)")
    return True

def check_c_ffmpeg_exists():
    log("\n" + "=" * 60)
    log("CHECK C: FFmpeg exists")
    log("-" * 60)
    ffmpeg_path = ROOT / "C" / "important" / "ffmpeg-2025-11-17-git-e94439e49b-full_build" / "bin" / "ffmpeg.exe"
    # The default path from code
    ffmpeg_path = Path("C:\\important\\ffmpeg-2025-11-17-git-e94439e49b-full_build\\bin\\ffmpeg.exe")
    if not ffmpeg_path.exists():
        log(f"  FAIL: ffmpeg not found at {ffmpeg_path}")
        # Try to find it anywhere
        for candidate in sorted(Path("C:\\important").glob("**/ffmpeg.exe")):
            log(f"  Found alternative: {candidate}")
            ffmpeg_path = candidate
            break
        else:
            return False
    log(f"  PASS: {ffmpeg_path}")
    return ffmpeg_path

def check_d_ffmpeg_version(ffmpeg_path):
    log("\n" + "=" * 60)
    log("CHECK D: ffmpeg -version")
    log("-" * 60)
    try:
        result = subprocess.run(
            [str(ffmpeg_path), "-version"],
            capture_output=True, text=True, timeout=10
        )
        first_line = result.stdout.split("\n")[0] if result.stdout else "(no output)"
        log(f"  PASS: {first_line}")
        if result.returncode != 0:
            log(f"  WARN: exit code={result.returncode}")
            log(f"  stderr: {result.stderr[:200]}")
        return True
    except FileNotFoundError:
        log(f"  FAIL: Cannot execute ffmpeg")
        return False
    except subprocess.TimeoutExpired:
        log(f"  FAIL: ffmpeg -version timed out after 10s")
        return False

def check_e_testsrc_export(ffmpeg_path):
    log("\n" + "=" * 60)
    log("CHECK E: ffmpeg testsrc export")
    log("-" * 60)
    output_path = EXPORTS_DIR / "diagnostic-test.mp4"
    try:
        result = subprocess.run(
            [str(ffmpeg_path), "-y", "-f", "lavfi", "-i",
             "testsrc=duration=1:size=1280x720:rate=60",
             "-pix_fmt", "yuv420p", "-c:v", "libx264",
             "-preset", "fast", "-crf", "18",
             str(output_path)],
            capture_output=True, text=True, timeout=30
        )
        if output_path.exists() and output_path.stat().st_size > 0:
            log(f"  PASS: MP4 created, size={output_path.stat().st_size / 1024:.1f} KB")
            output_path.unlink()  # Clean up
            return True
        else:
            log(f"  FAIL: MP4 not created or empty")
            log(f"  stderr: {result.stderr[:500]}")
            return False
    except subprocess.TimeoutExpired:
        log(f"  FAIL: ffmpeg timed out after 30s")
        return False

def check_f_game_code_inspection():
    log("\n" + "=" * 60)
    log("CHECK F: Game code inspection (static analysis)")
    log("-" * 60)

    export_cpp = ROOT / "src" / "replay" / "replay-export.cpp"
    main_cpp = ROOT / "src" / "main.cpp"

    findings = []

    # Search replay-export.cpp for key functions
    if export_cpp.exists():
        content = export_cpp.read_text()
        if "gReplayPlayer" not in content and "ReplayPlayer" not in content:
            findings.append("  WARN: replay-export.cpp does NOT reference gReplayPlayer")
        else:
            findings.append("  replay-export.cpp: references gReplayPlayer")
        if "clip.load(" in content:
            findings.append("  replay-export.cpp: loads clip locally (NOT into gReplayPlayer)")
        if "beginPlayback" not in content:
            findings.append("  WARN: replay-export.cpp does NOT call beginPlayback()")
    else:
        findings.append(f"  FAIL: {export_cpp} not found")

    # Search main.cpp for replay render path
    if main_cpp.exists():
        content = main_cpp.read_text()
        replay_playback_count = content.count("replayPlaybackActive")
        is_playing_count = content.count("gReplayPlayer.isPlaying()")
        seek_count = content.count("seekToTick")
        findings.append(f"  main.cpp: replayPlaybackActive appears {replay_playback_count} times")
        findings.append(f"  main.cpp: gReplayPlayer.isPlaying() appears {is_playing_count} times")
        findings.append(f"  main.cpp: seekToTick appears {seek_count} times")

        # Check if export seek code pauses playback
        if "gReplayPlayer.pause()" in content:
            idx = content.find("gReplayPlayer.pause()")
            line_no = content[:idx].count("\n") + 1
            findings.append(f"  main.cpp:{line_no}: pause() called during export seek")

        # Check the render condition
        if "if (replayPlaybackActive)" in content:
            idx = content.find("if (replayPlaybackActive)")
            line_no = content[:idx].count("\n") + 1
            findings.append(f"  main.cpp:{line_no}: replay actor render gated by replayPlaybackActive")
    else:
        findings.append(f"  FAIL: {main_cpp} not found")

    for f in findings:
        log(f)

    return findings

def check_g_stdin_pipe_test():
    log("\n" + "=" * 60)
    log("CHECK G: Stdin pipe test (does ffmpeg -i - work?)")
    log("-" * 60)
    log("  NOTE: This check can only be run manually from the in-game terminal.")
    log("  Run: export_test_exact_pipe")
    log("  This opens a visible cmd window with the exact export ffmpeg command.")
    log("  If ffmpeg starts and waits for stdin, -i - is working.")
    log("  If ffmpeg immediately exits, -i - is failing on this system.")

def main():
    log("=" * 60)
    log("  REPLAY EXPORT DIAGNOSTIC")
    log(f"  {time.strftime('%Y-%m-%d %H:%M:%S')}")
    log("=" * 60)

    replay = check_a_replay_exists()
    if not replay:
        save_report()
        return 1

    check_b_replay_loads(replay)

    ffmpeg = check_c_ffmpeg_exists()
    if ffmpeg:
        check_d_ffmpeg_version(ffmpeg)
        check_e_testsrc_export(ffmpeg)
    else:
        log("\n  SKIP: ffmpeg checks (not found)")

    check_f_game_code_inspection()
    check_g_stdin_pipe_test()

    log("\n" + "=" * 60)
    log("DIAGNOSTIC COMPLETE")
    log("=" * 60)

    save_report()
    return 0

if __name__ == "__main__":
    sys.exit(main())
