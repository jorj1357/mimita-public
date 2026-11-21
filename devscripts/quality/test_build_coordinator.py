"""Focused tests for single-revision whole-game build queueing."""

from __future__ import annotations

import importlib.util
import sys
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
if str(ROOT / "devscripts") not in sys.path:
    sys.path.insert(0, str(ROOT / "devscripts"))


def load_dev_loop():
    spec = importlib.util.spec_from_file_location(
        "mimita_dev_loop_build_coordinator", ROOT / "devscripts" / "dev-loop.py")
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


class BuildCoordinatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.module = load_dev_loop()
        self.loop = self.module.DevLoop.__new__(self.module.DevLoop)
        self.loop.lock = threading.RLock()
        self.loop.generation = 5
        self.loop.game_build_worker = None
        self.loop.active_game_build_request_ids = []
        self.loop.pending_game_build_request_ids = []
        self.loop.game_build_requests = {}
        self.loop.game_build_request_meta = {}
        self.loop.game_build_request_revisions = {}
        self.loop.game_build_pending_revision = ""
        self.loop.game_build_active_revision = ""
        self.loop.game_build_desired_revision = ""
        self.loop.game_build_failed_revision = ""
        self.loop.game_build_failed_signature = ""
        self.loop.game_build_retry_counts = {}
        self.loop.game_build_superseded = 0
        self.loop.game_build_detail = ""

    def test_same_revision_requests_coalesce(self) -> None:
        self.assertTrue(self.loop.queue_game_dll_request("a", revision=5))
        self.assertTrue(self.loop.queue_game_dll_request("b", revision=5))
        self.assertEqual(self.loop.pending_game_build_request_ids, ["a", "b"])
        self.assertEqual(self.loop.game_build_pending_revision, "5")

    def test_new_revision_supersedes_older_pending_requests(self) -> None:
        self.loop.queue_game_dll_request("old-1", revision=5)
        self.loop.queue_game_dll_request("old-2", revision=5)
        self.loop.queue_game_dll_request("new", revision=6)
        self.assertEqual(self.loop.pending_game_build_request_ids, ["new"])
        self.assertEqual(self.loop.game_build_requests["old-1"], "superseded")
        self.assertEqual(self.loop.game_build_requests["old-2"], "superseded")
        self.assertEqual(self.loop.game_build_superseded, 2)
        self.assertEqual(self.loop.game_build_desired_revision, "6")

    def test_active_revision_attaches_without_new_pending_build(self) -> None:
        self.loop.game_build_worker = object()
        self.loop.game_build_active_revision = "5"
        self.assertTrue(self.loop.queue_game_dll_request("active", revision=5))
        self.assertEqual(self.loop.pending_game_build_request_ids, [])
        self.assertEqual(self.loop.active_game_build_request_ids, ["active"])
        self.assertEqual(self.loop.game_build_requests["active"], "building")

    def test_failed_revision_is_blocked_until_forced(self) -> None:
        self.loop.game_build_failed_revision = "5"
        self.assertFalse(self.loop.queue_game_dll_request("same", revision=5))
        self.assertEqual(self.loop.game_build_requests["same"], "blocked")
        self.assertTrue(self.loop.queue_game_dll_request(
            "retry", revision=5, force=True))
        self.assertEqual(self.loop.pending_game_build_request_ids, ["retry"])

    def test_changed_revision_is_allowed_after_failure(self) -> None:
        self.loop.game_build_failed_revision = "5"
        self.assertTrue(self.loop.queue_game_dll_request("next", revision=6))
        self.assertEqual(self.loop.game_build_pending_revision, "6")


if __name__ == "__main__":
    unittest.main()
