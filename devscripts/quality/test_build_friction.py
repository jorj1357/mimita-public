import importlib.util
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
if str(ROOT / "devscripts") not in sys.path:
    sys.path.insert(0, str(ROOT / "devscripts"))

from devscripts import build_friction
from devscripts.agent_coord import CoordStore


class WaitIssueTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / ".dev" / "tmp")
        self.root = Path(self.temp.name) / "coord"
        self.original_store = build_friction.CoordStore

        def make_store():
            return CoordStore(root=self.root, publish_docs=False)

        build_friction.CoordStore = make_store
        store = make_store()
        store.begin("wait-test", "wait issue test")
        store.close()

    def tearDown(self):
        build_friction.CoordStore = self.original_store
        self.temp.cleanup()

    def _wait(self):
        return {
            "wait_id": "exe-build:7:2026-01-01T00:00:00Z",
            "kind": "exe-build",
            "stage": "compiling",
            "reason": "compile 0/45",
            "elapsed_seconds": 61.2,
            "budget_seconds": 60,
            "started_utc": "2026-01-01T00:00:00Z",
            "build_id": "test-build",
            "generation": 7,
            "source_generation": 7,
            "request_ids": ["req-1"],
        }

    def test_one_issue_per_wait_and_occurrence(self):
        first = build_friction.record_wait_incident("wait-test", self._wait())
        second = build_friction.record_wait_incident("wait-test", self._wait())
        self.assertTrue(first["created"])
        self.assertFalse(second["created"])
        self.assertEqual(first["issue_id"], second["issue_id"])
        store = build_friction.CoordStore()
        try:
            issues = store.issue_list(kind="build-friction", limit=50)
            self.assertEqual(len(issues), 1)
            self.assertEqual(len(store.issue_show(first["issue_id"])["occurrences"]), 1)
        finally:
            store.close()

    def test_waiting_queue_returns_missing_evidence_task(self):
        issue = build_friction.record_wait_incident("wait-test", self._wait())
        store = build_friction.CoordStore()
        try:
            store.task_add(
                "wait-test", "wait-proof", "Prove wait incident",
                "wait-test", issue_id=issue["issue_id"],
                acceptance="A pinned run proves the wait path.")
            work = store.waiting_work("wait-test")
            self.assertEqual(work["work_kind"], "missing_evidence")
            self.assertEqual(work["issue"]["id"], issue["issue_id"])
            self.assertEqual(work["task"]["id"], "wait-proof")
        finally:
            store.close()

    def test_dev_loop_checks_a_61_second_build_wait(self):
        spec = importlib.util.spec_from_file_location(
            "mimita_dev_loop", ROOT / "devscripts" / "dev-loop.py")
        module = importlib.util.module_from_spec(spec)
        assert spec.loader is not None
        spec.loader.exec_module(module)
        loop = module.DevLoop.__new__(module.DevLoop)
        loop.lock = threading.RLock()
        loop.active_build_generation = 7
        loop.active_build_started_monotonic = time.monotonic() - 61.0
        loop.active_build_started_utc = "2026-01-01T00:00:00Z"
        loop.active_build_phase = "compiling"
        loop.active_build_detail = "compiling 0/45"
        loop.active_build_request_ids = ["req-1"]
        loop.build_pending = False
        loop.pending_build_since_monotonic = None
        loop.game_build_status = "idle"
        loop.game_build_started_utc = None
        loop.latest_build = "test-build"
        loop.generation = 7
        loop.reported_wait_ids = set()
        loop.last_wait_check_monotonic = 0.0
        events = []
        loop.record_runtime_event = (
            lambda *args, **kwargs: events.append((args, kwargs)))
        wakes = []
        original_wake = build_friction.wake_reviewer
        build_friction.wake_reviewer = (
            lambda issue_id, wait: wakes.append((issue_id, wait)) or
            {"queued": True})
        try:
            loop.check_wait_budget()
        finally:
            build_friction.wake_reviewer = original_wake
        self.assertEqual(len(wakes), 1, events)
        self.assertEqual(wakes[0][1]["wait_id"],
                         "exe-build:7:2026-01-01T00:00:00Z")
        self.assertTrue(loop.reported_wait_ids)


if __name__ == "__main__":
    unittest.main()
