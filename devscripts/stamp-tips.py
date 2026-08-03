#!/usr/bin/env python3
"""Stamp tips in config/tips.json with the UTC write time.

Tips are plain strings. Any tip that does NOT already end with a UTC stamp in
the format "D M YYYY HHMM utc" (e.g. "8 3 2026 1450 utc") gets the current UTC
time appended, so going forward new tips are dated automatically.

Usage:
    python devscripts/stamp-tips.py [--date "D M YYYY HHMM"] [--dry-run]

--date overrides the stamp for tips being stamped (useful for back-dating).
--dry-run prints what would change without writing the file.
"""
import argparse
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

TIPS_PATH = Path(__file__).resolve().parent.parent / "config" / "tips.json"
STAMP_RE = re.compile(r"\s*\d{1,2} \d{1,2} \d{4} \d{4} utc\s*$")


def main() -> int:
    parser = argparse.ArgumentParser(description="Stamp tips with UTC write time")
    parser.add_argument("--date", help='Override stamp, format "D M YYYY HHMM" (UTC)')
    parser.add_argument("--dry-run", action="store_true", help="Print changes only")
    args = parser.parse_args()

    if args.date:
        stamp = args.date.strip()
    else:
        now = datetime.now(timezone.utc)
        stamp = f"{now.day} {now.month} {now.year} {now.hour:02d}{now.minute:02d} utc"

    try:
        tips = json.loads(TIPS_PATH.read_text(encoding="utf-8"))
    except FileNotFoundError:
        print(f"[TIPS] missing {TIPS_PATH}")
        return 1
    except json.JSONDecodeError as e:
        print(f"[TIPS] invalid json: {e}")
        return 1

    if not isinstance(tips, list):
        print("[TIPS] config/tips.json is not an array")
        return 1

    changed = 0
    out = []
    for tip in tips:
        if not isinstance(tip, str):
            out.append(tip)
            continue
        if STAMP_RE.search(tip):
            out.append(tip)
            continue
        stamped = tip.rstrip() + " " + stamp
        out.append(stamped)
        changed += 1
        print(f"[TIPS] stamping: {stamped}")

    if changed == 0:
        print("[TIPS] all tips already stamped")
        return 0

    if args.dry_run:
        print(f"[TIPS] dry run: {changed} tip(s) would be stamped")
        return 0

    TIPS_PATH.write_text(json.dumps(out, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"[TIPS] stamped {changed} tip(s) in config/tips.json")
    return 0


if __name__ == "__main__":
    sys.exit(main())
