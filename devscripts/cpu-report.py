"""Report per-thread CPU attribution for a dev-loop run.

Reads the Phase 0 collector outputs and prints the accounting table from the
2026-11-28 CPU spec:

    MiMITA total CPU: 25.00% of host capacity
    function/thread   core%   host%

Threads come from ``devscripts/perf-watch.py --threads``. Journal phase numbers
come from ``devscripts/measure-events.py`` so the report never re-implements a
journal parser.

Examples:

    python devscripts/cpu-report.py 20261128_171225
    python devscripts/cpu-report.py 20261128_171225 \\
        --threads-csv .dev/perf-captures/idle.threads.csv
"""

from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import json
from pathlib import Path
import subprocess
import sys
from statistics import mean


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def find_run_dir(run_id: str) -> Path | None:
    matches = sorted(Path("logs").glob(f"*/{run_id}"))
    return matches[-1] if matches else None


def load_thread_rows(path: Path) -> list[dict[str, str]]:
    if not path.is_file():
        return []
    with path.open("r", newline="", encoding="utf-8") as stream:
        return [row for row in csv.DictReader(stream) if row.get("core_pct")]


def aggregate_threads(rows: list[dict[str, str]]) -> list[dict[str, object]]:
    buckets: dict[tuple[str, str], dict[str, object]] = {}
    for row in rows:
        try:
            core = float(row.get("core_pct") or 0.0)
        except ValueError:
            continue
        key = (row.get("pid", ""), row.get("tid", ""))
        bucket = buckets.setdefault(key, {
            "pid": row.get("pid", ""),
            "tid": row.get("tid", ""),
            "name": row.get("thread_name") or "",
            "started": row.get("thread_start_utc") or "",
            "core": [],
            "user": 0.0,
            "kernel": 0.0,
            "host_cpus": row.get("host_logical_cpus") or "",
        })
        if not bucket["name"] and row.get("thread_name"):
            bucket["name"] = row["thread_name"]
        bucket["core"].append(core)
        try:
            bucket["user"] = float(row.get("cpu_user_s") or 0.0)
            bucket["kernel"] = float(row.get("cpu_kernel_s") or 0.0)
        except ValueError:
            pass
    aggregated: list[dict[str, object]] = []
    for bucket in buckets.values():
        cores: list[float] = bucket.pop("core")  # type: ignore[assignment]
        bucket["core_mean"] = mean(cores)
        try:
            cpus = int(bucket.get("host_cpus") or 0)
        except ValueError:
            cpus = 0
        bucket["host_mean"] = bucket["core_mean"] / cpus if cpus else 0.0
        bucket["samples"] = len(cores)
        aggregated.append(bucket)
    aggregated.sort(key=lambda item: float(item["core_mean"]), reverse=True)  # type: ignore[arg-type]
    return aggregated


def run_measure_events(run_id: str, preset: str) -> str:
    command = [sys.executable, "devscripts/measure-events.py", run_id,
               "--preset", preset, "--json"]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.SubprocessError) as error:
        return f"(measure-events {preset} failed: {error})"
    if result.returncode != 0:
        message = (result.stderr or result.stdout).strip().splitlines()
        return f"(measure-events {preset} unavailable: {message[0] if message else 'no output'})"
    try:
        payload = json.loads(result.stdout)
    except json.JSONDecodeError:
        return result.stdout.strip()
    lines = [f"preset {preset}:"]
    counts = payload.get("counts") or {}
    for event, count in sorted(counts.items()):
        lines.append(f"  {event}: count={count}")
    for key, summary in sorted((payload.get("values") or {}).items()):
        if not isinstance(summary, dict):
            continue
        lines.append(
            "  {key}: n={n} mean={mean} p50={p50} p95={p95} max={max}".format(
                key=key,
                n=summary.get("n", "?"),
                mean=summary.get("mean", "?"),
                p50=summary.get("p50", "?"),
                p95=summary.get("p95", "?"),
                max=summary.get("max", "?"),
            ))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("run", help="run id (for example 20261128_171225) or a run directory")
    parser.add_argument("--threads-csv", type=Path,
                        help="explicit per-thread CSV; default discovers <run>/*/cpu-threads.csv")
    parser.add_argument("--top", type=int, default=25)
    parser.add_argument("--out", type=Path, help="write the report here as markdown")
    parser.add_argument("--no-journal", action="store_true",
                        help="skip the measure-events phase section")
    args = parser.parse_args()

    run_path = Path(args.run)
    if run_path.is_dir():
        run_id = run_path.name
        run_dir = run_path
    else:
        run_id = args.run
        run_dir = find_run_dir(run_id)
        if run_dir is None:
            parser.error(f"run {run_id} not found under logs/")

    thread_csvs: list[Path] = []
    if args.threads_csv:
        thread_csvs = [args.threads_csv]
    else:
        thread_csvs = sorted(run_dir.glob("*/cpu-threads.csv"))
    rows: list[dict[str, str]] = []
    for path in thread_csvs:
        rows.extend(load_thread_rows(path))

    lines: list[str] = []
    lines.append(f"# CPU attribution — run {run_id}")
    lines.append("")
    lines.append(f"generated_utc: {utc_now()}")
    lines.append(f"run_dir: {run_dir}")
    lines.append(f"thread_csvs: {', '.join(str(p) for p in thread_csvs) or 'none'}")
    lines.append("")

    if not rows:
        lines.append("No per-thread samples found. Collect one with:")
        lines.append("")
        lines.append(f"  python devscripts/perf-watch.py --run {run_id} --threads --duration 60")
        lines.append("")
    else:
        threads = aggregate_threads(rows)
        cpus = 0
        for row in rows:
            try:
                cpus = max(cpus, int(row.get("host_logical_cpus") or 0))
            except ValueError:
                pass
        cpus = cpus or 1
        total_core = sum(float(t["core_mean"]) for t in threads)
        lines.append("```text")
        lines.append(f"MiMITA sampled total CPU: {total_core:.1f}% of one logical CPU "
                     f"({total_core / cpus:.2f}% of host capacity, {cpus} logical CPUs)")
        lines.append("")
        lines.append(f"{'pid:tid':<16}{'thread':<26}{'core%':>8}{'host%':>8}"
                     f"{'user_s':>9}{'kernel_s':>10}  thread_start_utc")
        lines.append("-" * 96)
        for thread in threads[:args.top]:
            lines.append(
                f"{str(thread['pid']) + ':' + str(thread['tid']):<16}"
                f"{(thread['name'] or '(unnamed)'):<26}"
                f"{float(thread['core_mean']):>8.2f}"
                f"{float(thread['host_mean']):>8.2f}"
                f"{float(thread['user']):>9.1f}"
                f"{float(thread['kernel']):>10.1f}"
                f"  {thread['started']}")
        if len(threads) > args.top:
            remainder = sum(float(t["core_mean"]) for t in threads[args.top:])
            lines.append(f"{'... remaining':<42}{remainder:>8.2f}{remainder / cpus:>8.2f}")
        lines.append("-" * 96)
        lines.append(f"{'total':<42}{total_core:>8.2f}{total_core / cpus:>8.2f}")
        lines.append("")
        lines.append("Thread rows are per-thread CPU deltas from GetThreadTimes; "
                     "core% is share of one logical CPU, host% is share of all "
                     "logical CPUs. User and kernel seconds are lifetime totals.")
        lines.append("```")
        lines.append("")
        by_pid: dict[str, float] = {}
        for thread in threads:
            pid = str(thread["pid"])
            by_pid[pid] = by_pid.get(pid, 0.0) + float(thread["core_mean"])
        lines.append("Per process (sum of sampled threads):")
        lines.append("")
        lines.append("```text")
        for pid, core in sorted(by_pid.items(), key=lambda item: -item[1]):
            lines.append(f"  pid {pid:<8} {core:7.1f}% of one logical CPU  {core / cpus:6.2f}% host")
        lines.append("```")
        lines.append("")

    if not args.no_journal:
        lines.append("## Journal phase numbers (measure-events.py)")
        lines.append("")
        lines.append("```text")
        lines.append(run_measure_events(run_id, "server-tick"))
        lines.append(run_measure_events(run_id, "frame"))
        lines.append("```")
        lines.append("")

    report = "\n".join(lines)
    print(report)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(report + "\n", encoding="utf-8")
        print(f"\nwrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
