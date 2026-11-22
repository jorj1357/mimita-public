"""Run manifest and per-executable journal layout for the dev loop."""

from __future__ import annotations

import datetime
import json
from dataclasses import dataclass
from pathlib import Path

from .state import write_json

ROOT = Path(__file__).resolve().parents[2]


def _utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(
        timespec="milliseconds").replace("+00:00", "Z")


@dataclass
class RunLayout:
    run_dir: Path
    run_id: str
    manifest_path: Path
    dev_loop_journal: Path
    server_journal: Path
    client_journal: Path

    def instance_dir(self, instance_id: str) -> Path:
        return self.run_dir / instance_id

    def journal_for(self, instance_id: str) -> Path:
        return self.instance_dir(instance_id) / "events.jsonl"


def create_run_layout() -> RunLayout:
    """Create one run folder with a dev-loop and one server/client journal."""
    now = datetime.datetime.now(datetime.timezone.utc)
    base = ROOT / "logs" / now.strftime("%Y-%m-%d") / now.strftime("%Y%m%d_%H%M%S")
    run_dir = base
    suffix = 1
    while run_dir.exists():
        run_dir = Path(f"{base}_{suffix:02d}")
        suffix += 1
    for instance_id in ("dev-loop", "server-01", "client-01"):
        (run_dir / instance_id).mkdir(parents=True, exist_ok=True)
    return RunLayout(
        run_dir=run_dir,
        run_id=run_dir.name,
        manifest_path=run_dir / "run.json",
        dev_loop_journal=run_dir / "dev-loop" / "events.jsonl",
        server_journal=run_dir / "server-01" / "events.jsonl",
        client_journal=run_dir / "client-01" / "events.jsonl",
    )


def write_run_manifest(layout: RunLayout, manifest: dict) -> None:
    """Write a run manifest with the shared retrying atomic JSON writer."""
    payload = dict(manifest)
    payload.setdefault("schema_version", 1)
    payload["run_id"] = layout.run_id
    payload["journal_root"] = str(layout.run_dir)
    payload["updated_utc"] = _utc_now()
    write_json(layout.manifest_path, payload)


def read_run_manifest(layout: RunLayout) -> dict:
    try:
        value = json.loads(layout.manifest_path.read_text(encoding="utf-8"))
        return value if isinstance(value, dict) else {}
    except (OSError, json.JSONDecodeError):
        return {}


def manifest_path_for_events(events_path: str | None) -> Path | None:
    """Resolve `.../<run>/instance/events.jsonl` back to `<run>/run.json`."""
    if not events_path:
        return None
    path = Path(events_path)
    for parent in path.parents:
        candidate = parent / "run.json"
        if candidate.exists():
            return candidate
    return None
