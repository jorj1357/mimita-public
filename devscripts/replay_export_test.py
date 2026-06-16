#!/usr/bin/env python3
"""
Replay Export Test Harness
==========================
Automated test script for replay export pipeline.

Run AFTER launching MiMITA and running replay_export_latest.
Parses game output logs and export debug logs.

Usage:
    python devscripts/replay_export_test.py

Or with explicit log file:
    python devscripts/replay_export_test.py --log <path>
"""

import os
import sys
import time
import json
import re
import glob
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXPORTS_DIR = ROOT / "replays" / "exports"

PASS = 0
FAIL = 0
WARN = 0
def ok(msg): global PASS; PASS += 1; print(f"  [PASS] {msg}")
def fail(msg): global FAIL; FAIL += 1; print(f"  [FAIL] {msg}")
def warn(msg): global WARN; WARN += 1; print(f"  [WARN] {msg}")

def find_export_logs():
    """Find all export diagnostic logs."""
    logs = []
    for f in EXPORTS_DIR.glob("export-debug*"):
        logs.append(f)
    for f in EXPORTS_DIR.glob("*.txt"):
        logs.append(f)
    for f in EXPORTS_DIR.glob("*diagnostic*"):
        logs.append(f)
    return sorted(set(logs))

def parse_log_for_stages(log_path):
    """Parse a log file for [EXPORT] stage markers."""
    if not log_path.exists():
        return []
    content = log_path.read_text(encoding="utf-8", errors="replace")
    stages = []
    for line in content.split("\n"):
        if "[EXPORT]" in line:
            stages.append(line.strip())
    return stages

def check_stage_sequence(stages):
    """Verify stages 1-8 are present and in order."""
    stage_numbers = []
    for s in stages:
        m = re.search(r"STAGE (\d)/8", s)
        if m:
            stage_numbers.append(int(m.group(1)))
    if not stage_numbers:
        fail("No stage markers found in logs")
        return
    for i in range(1, max(stage_numbers) + 1):
        if i in stage_numbers:
            ok(f"Stage {i}/8 reached")
        else:
            fail(f"Stage {i}/8 MISSING")

def check_replay_loaded(stages):
    """Verify replay was loaded."""
    for s in stages:
        if "REPLAY_PLAYER loaded" in s:
            m = re.search(r"totalTicks=(\d+)", s)
            if m and int(m.group(1)) > 0:
                ok(f"replay loaded: {m.group(1)} ticks")
            else:
                fail("replay loaded but 0 ticks")
            m2 = re.search(r"actorCount=(\d+)", s)
            if m2:
                cnt = int(m2.group(1))
                if cnt > 0:
                    ok(f"replay has {cnt} actors")
                else:
                    warn("replay has 0 actors (empty scene)")
            return
    fail("REPLAY_PLAYER loaded message not found")

def check_ffmpeg_pipe(stages):
    """Verify ffmpeg was launched."""
    for s in stages:
        if "ffmpeg launched" in s.lower():
            ok("ffmpeg launched")
            return
        if "pipe open OK" in s:
            ok("pipe opened successfully")
            return
    # Check for failure
    for s in stages:
        if "_popen returned NULL" in s:
            fail("_popen failed (returned NULL)")
            return
        if "pipe open FAILED" in s:
            fail("pipe open FAILED")
            return
    fail("ffmpeg launch status unknown")

def check_frame_capture(stages):
    """Verify frames were captured."""
    frame_lines = [s for s in stages if "frame" in s.lower() and "hash" in s.lower()]
    if not frame_lines:
        frame_lines = [s for s in stages if "Capturing" in s or "capturedTicks" in s]
    if frame_lines:
        ok(f"frame capture active ({len(frame_lines)} frame lines)")
    else:
        fail("no frame capture activity detected")
    return frame_lines

def check_static_screen(stages):
    """Detect if screen is static by checking frame hash comparison."""
    same_as_0 = [s for s in stages if "sameAsFrame0=YES" in s]
    diff_from_0 = [s for s in stages if "sameAsFrame0=NO" in s]
    if same_as_0 and not diff_from_0:
        fail(f"STATIC SCREEN: all {len(same_as_0)} frame hashes identical to frame 0")
    elif diff_from_0:
        ok(f"replay advancing: {len(diff_from_0)} frames differ from frame 0")
    else:
        warn("could not determine if screen is static (no hash comparison data)")

def check_ffmpeg_stderr():
    """Check ffmpeg stderr log for errors."""
    stderr_path = EXPORTS_DIR / "_ffmpeg_stderr.txt"
    if not stderr_path.exists():
        warn("ffmpeg stderr log not found (export may not have run)")
        return
    content = stderr_path.read_text(encoding="utf-8", errors="replace")
    if not content.strip():
        ok("ffmpeg stderr is empty (clean run)")
        return
    # Check for common error patterns
    error_patterns = [
        r"Error", r"error", r"FAIL", r"fail", r"Invalid",
        r"not found", r"Cannot", r"cannot", r"denied",
        r"Unknown", r"expected", r"unexpected"
    ]
    errors_found = []
    for pat in error_patterns:
        for match in re.finditer(pat, content):
            line_start = content.rfind("\n", 0, match.start()) + 1
            line_end = content.find("\n", match.start())
            if line_end == -1:
                line_end = len(content)
            errors_found.append(content[line_start:line_end].strip())
    if errors_found:
        fail(f"ffmpeg stderr contains {len(errors_found)} potential error(s):")
        for e in errors_found[:5]:
            print(f"         {e}")
    else:
        ok("ffmpeg stderr content (no known error pattern matched)")

def check_mp4_output():
    """Find newest MP4 in exports and verify it."""
    mp4s = sorted(EXPORTS_DIR.glob("**/*.mp4"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not mp4s:
        fail("no MP4 found in exports/")
        return None
    newest = mp4s[0]
    size = newest.stat().st_size
    print(f"  Found MP4: {newest}")
    print(f"  Size: {size} bytes ({size/1024:.1f} KB)")
    if size > 0:
        ok(f"MP4 file exists ({size} bytes)")
    else:
        fail("MP4 file is empty (0 bytes)")
        return newest
    # Try ffprobe
    try:
        result = subprocess.run(
            ["ffprobe", "-v", "quiet", "-print_format", "json",
             "-show_format", "-show_streams", str(newest)],
            capture_output=True, text=True, timeout=30
        )
        if result.returncode == 0:
            info = json.loads(result.stdout)
            fmt = info.get("format", {})
            duration = fmt.get("duration", "?")
            bitrate = fmt.get("bit_rate", "?")
            streams = info.get("streams", [])
            print(f"  Duration: {duration}s")
            print(f"  Bitrate: {bitrate}")
            for s in streams:
                codec = s.get("codec_name", "?")
                res = f"{s.get('width', '?')}x{s.get('height', '?')}"
                fps = s.get("r_frame_rate", "?")
                print(f"  Stream: {codec} {res} @ {fps} fps")
            ok("ffprobe analysis successful")
        else:
            warn(f"ffprobe failed (exit={result.returncode})")
    except FileNotFoundError:
        warn("ffprobe not found (install ffmpeg tools)")
    except subprocess.TimeoutExpired:
        warn("ffprobe timed out")
    return newest

def check_export_log():
    """Find and parse export-debug.log."""
    log_paths = sorted(ROOT.glob("replays/exports/export-debug*.log"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not log_paths:
        warn("export-debug.log not found (run export_diagnose from terminal)")
        return None
    log_path = log_paths[0]
    print(f"\n  Parsing: {log_path}")
    stages = parse_log_for_stages(log_path)
    if stages:
        ok(f"found {len(stages)} [EXPORT] log lines")
        check_stage_sequence(stages)
        check_replay_loaded(stages)
        check_ffmpeg_pipe(stages)
        frames = check_frame_capture(stages)
        check_static_screen(stages)
    else:
        fail("no [EXPORT] log lines found")
    return stages

def find_replay_clips():
    """Find all replay clips for validation."""
    clips = []
    for pattern in ["**/*.json", "**/*.mclip.json"]:
        for f in (ROOT / "replays").glob(pattern):
            name = f.name
            if "-validation" in name:
                continue
            if "exports" in f.parts:
                continue
            clips.append(f)
    clips.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return clips

def log_summary():
    print(f"\n{'='*60}")
    print(f"  RESULTS: {PASS} passed, {FAIL} failed, {WARN} warnings")
    print(f"{'='*60}")

def main():
    global PASS, FAIL, WARN
    print("=" * 60)
    print("  REPLAY EXPORT TEST HARNESS")
    print(f"  {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 60)

    # Step 1: Check replay clips exist
    print("\n--- Replay clips ---")
    clips = find_replay_clips()
    if clips:
        for c in clips[:5]:
            size = c.stat().st_size
            print(f"  {c.name} ({size/1024:.0f} KB)")
        if len(clips) > 5:
            print(f"  ... and {len(clips)-5} more")
        ok(f"{len(clips)} replay clip(s) found")
    else:
        fail("no replay clips found")
        log_summary()
        return 1

    # Step 2: Check export logs
    print("\n--- Export logs ---")
    export_logs = find_export_logs()
    if export_logs:
        for l in export_logs:
            age = time.time() - l.stat().st_mtime
            print(f"  {l.name} ({age:.0f}s old)")
        ok(f"{len(export_logs)} export log(s) found")
    else:
        warn("no export logs found")

    # Step 3: Analyze export log
    print("\n--- Pipeline analysis ---")
    stages = check_export_log()

    # Step 4: Check MP4
    print("\n--- MP4 output ---")
    mp4 = check_mp4_output()

    # Step 5: Check ffmpeg stderr
    print("\n--- ffmpeg stderr ---")
    check_ffmpeg_stderr()

    # Step 6: Combined analysis
    print("\n--- Combined diagnosis ---")
    if FAIL == 0:
        print("  ✓ All checks passed!")
    else:
        print(f"  ✗ {FAIL} check(s) failed")

    log_summary()
    return 0 if FAIL == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
