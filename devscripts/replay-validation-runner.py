"""Run the Mimita Blender replay importer and transform validator headlessly."""

import argparse
import json
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BLENDER = Path(
    r"C:\Program Files\Blender Foundation\Blender 5.0\blender.exe"
)
IMPORTER = REPO_ROOT / "devscripts" / "mimita-replay-import-v1.py"


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--replay", required=True)
    parser.add_argument("--validation")
    parser.add_argument("--report")
    parser.add_argument("--blender", default=str(DEFAULT_BLENDER))
    return parser.parse_args()


def main():
    args = parse_args()
    replay_path = Path(args.replay)
    if not replay_path.is_absolute():
        replay_path = (REPO_ROOT / replay_path).resolve()
    if not replay_path.exists():
        print(f"[REPLAY VALIDATION] Replay not found: {replay_path}")
        return 2

    blender_path = Path(args.blender)
    if not blender_path.exists():
        print(f"[REPLAY VALIDATION] Blender not found: {blender_path}")
        return 2

    report_path = (
        Path(args.report)
        if args.report
        else replay_path.with_name(
            replay_path.stem + "-validation-report.json"
        )
    )
    if not report_path.is_absolute():
        report_path = (REPO_ROOT / report_path).resolve()

    command = [
        str(blender_path),
        "--background",
        "--factory-startup",
        "--python",
        str(IMPORTER),
        "--",
        "--replay",
        str(replay_path),
        "--validation-report",
        str(report_path),
    ]
    if args.validation:
        validation_path = Path(args.validation)
        if not validation_path.is_absolute():
            validation_path = (REPO_ROOT / validation_path).resolve()
        command.extend(["--validation", str(validation_path)])

    print("[REPLAY VALIDATION] Launching Blender")
    result = subprocess.run(command, cwd=REPO_ROOT, check=False)
    if result.returncode != 0:
        print(
            "[REPLAY VALIDATION] Blender import failed with exit code "
            f"{result.returncode}"
        )
        return result.returncode
    if not report_path.exists():
        print(f"[REPLAY VALIDATION] Report was not created: {report_path}")
        return 2

    report = json.loads(report_path.read_text(encoding="utf-8"))
    status = "PASS" if report.get("pass") else "FAIL"
    summary = report.get("summary", {})
    print(f"[REPLAY VALIDATION] {status}")
    print(
        "[REPLAY VALIDATION] largestPositionError="
        f'{summary.get("largestPositionErrorMeters", 0.0):.6f}m '
        "largestRotationError="
        f'{summary.get("largestRotationErrorDegrees", 0.0):.6f}deg'
    )
    print(f"[REPLAY VALIDATION] report={report_path}")
    return 0 if report.get("pass") else 1


if __name__ == "__main__":
    sys.exit(main())
