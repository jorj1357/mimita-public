"""Focused tests for the EXE no-op source-identity fast path."""

from __future__ import annotations

import json
import queue
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
if str(ROOT / "devscripts") not in sys.path:
    sys.path.insert(0, str(ROOT / "devscripts"))

from dev_loop import build as build_worker  # noqa: E402


class _Process:
    def __init__(self, lines: list[str], on_wait=None):
        self.stdout = lines
        self._on_wait = on_wait

    def wait(self) -> int:
        if self._on_wait is not None:
            self._on_wait()
        return 0


class _Loop:
    def __init__(self, generation: int = 14):
        self.lock = threading.RLock()
        self.generation = generation
        self.latest_stale = True
        self.latest_build_folder = "artifact-a"
        self.latest_build = "artifact-a"
        self.build_completions = queue.Queue()
        self.events: list[tuple] = []

    def _record_build_progress(self, _progress: dict) -> None:
        pass

    def _record_build_output(self, _line: str) -> None:
        pass

    def record_runtime_event(self, *event) -> None:
        self.events.append(event)


class ExeNoOpIdentityTests(unittest.TestCase):
    def setUp(self) -> None:
        temp_root = ROOT / ".dev" / "tmp"
        temp_root.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=temp_root)
        self.root = Path(self.temp.name)
        artifact = self.root / ".dev" / "builds" / "artifact-a"
        artifact.mkdir(parents=True)
        exe_bytes = b"test-only-exe-artifact"
        (self.root / "mimita.exe").write_bytes(exe_bytes)
        (artifact / "mimita.exe").write_bytes(exe_bytes)
        self.source = {
            "revision": "revision-a",
            "dirty": True,
            "worktree_sha256": "source-hash-a",
        }
        (artifact / "build-identity.json").write_text(json.dumps({
            "artifact_id": "artifact-a",
            "source_generation": 1,
            "source_at_publish": self.source,
        }), encoding="utf-8")
        self.loop = _Loop()

    def tearDown(self) -> None:
        self.temp.cleanup()

    def _run_noop(self, *, on_wait=None) -> dict:
        process = _Process([
            "Nothing changed. (current)\n",
            "Time    : 0.33 sec\n",
        ], on_wait)
        with patch.object(build_worker.subprocess, "Popen", return_value=process):
            build_worker.run_build(
                self.loop, 14, ["request-a"], "debug",
                root=self.root,
                make_artifact_id=lambda: (_ for _ in ()).throw(
                    AssertionError("a no-op must not create an artifact")),
                publish_build=lambda *_args: (_ for _ in ()).throw(
                    AssertionError("a no-op must not publish an artifact")),
                find_ccache=lambda: None,
                creationflags=0,
                hidden_startupinfo=lambda: None,
            )
        return self.loop.build_completions.get_nowait()

    def test_stable_noop_reuses_artifact_identity_without_git_scans(self) -> None:
        with patch.object(build_worker, "source_identity",
                          side_effect=AssertionError("unexpected Git identity scan")):
            result = self._run_noop()

        self.assertTrue(result["success"])
        self.assertEqual(result["artifact_id"], "artifact-a")
        self.assertFalse(result["source_changed_during_build"])
        self.assertEqual(result["source_identity"]["start_basis"],
                         "published_exe_artifact")
        self.assertEqual(result["source_identity"]["publish_seconds"], 0.0)
        self.assertEqual(result["source_identity"]["identity_source_generation"], 1)
        timing = result["worker_timing"]
        self.assertEqual(timing["source_identity_start_basis"],
                         "published_exe_artifact")
        self.assertEqual(timing["source_identity_publish_seconds"], 0.0)
        self.assertAlmostEqual(timing["total_worker_seconds"],
                               result["duration_seconds"], places=3)
        accounted = sum(timing[name] for name in (
            "source_identity_start_seconds", "setup_seconds",
            "build_script_seconds", "post_build_processing_seconds",
            "output_log_seconds", "runtime_event_seconds",
            "unattributed_seconds"))
        self.assertAlmostEqual(accounted, timing["total_worker_seconds"],
                               delta=0.02)

    def test_generation_change_keeps_fresh_identity_and_is_reported(self) -> None:
        fresh_source = {
            "revision": "revision-b",
            "dirty": True,
            "worktree_sha256": "source-hash-b",
        }
        with patch.object(build_worker, "source_identity", return_value=fresh_source):
            result = self._run_noop(
                on_wait=lambda: setattr(self.loop, "generation", 15))

        event = self.loop.events[-1][1]
        self.assertTrue(result["source_changed_during_build"])
        self.assertEqual(event["source_generation_start"], 14)
        self.assertEqual(event["source_generation_end"], 15)
        self.assertEqual(event["publish_basis"], "full_worktree_git")
        self.assertEqual(event["identity_source_generation"], 15)


if __name__ == "__main__":
    unittest.main()
