import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from devscripts.dev_loop.batch_targets import (
    can_share_source_generation_baseline,
    filter_reverted_batch_paths,
    restore_pending_batch,
)


class Mode3BatchNetDeltaTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.manifests = self.root / "manifests"
        self.manifests.mkdir()

    def tearDown(self):
        self.temp.cleanup()

    @staticmethod
    def _sha(text):
        return hashlib.sha256(text.encode("utf-8")).hexdigest()

    def _manifest(self, name, **entries):
        path = self.manifests / name
        payload = {
            "sources": [],
            "headers": [],
            "config_hashes": {},
        }
        for relative, (target, text) in entries.items():
            if target == "config":
                payload["config_hashes"][relative] = self._sha(text)
            else:
                payload["sources" if target == "source" else "headers"].append(
                    {"path": relative, "sha256": self._sha(text)})
        path.write_text(json.dumps(payload), encoding="utf-8")
        return path

    def test_reverted_cold_file_is_dropped_but_changed_game_file_stays(self):
        cold_path = self.root / "src" / "host" / "host-runtime.cpp"
        game_path = self.root / "src" / "npc" / "behavior.cpp"
        cold_path.parent.mkdir(parents=True)
        game_path.parent.mkdir(parents=True)
        cold_path.write_text("published cold bytes", encoding="utf-8")
        game_path.write_text("new game bytes", encoding="utf-8")
        cold_manifest = self._manifest(
            "exe.json", **{"src/host/host-runtime.cpp": ("source", "published cold bytes")})
        game_manifest = self._manifest(
            "game.json", **{"src/npc/behavior.cpp": ("source", "old game bytes")})

        rows = [
            {"path": "src/host/host-runtime.cpp", "target": "cold"},
            {"path": "src/npc/behavior.cpp", "target": "game"},
        ]
        retained, reverted = filter_reverted_batch_paths(
            self.root, rows, {"cold": cold_manifest, "game": game_manifest})

        self.assertEqual(retained, [rows[1]])
        self.assertEqual(reverted, ["src/host/host-runtime.cpp"])

    def test_reverted_config_hash_is_dropped(self):
        path = self.root / "config" / "modules" / "modules.json"
        path.parent.mkdir(parents=True)
        path.write_text("published config", encoding="utf-8")
        manifest = self._manifest(
            "exe.json", **{"config/modules/modules.json": ("config", "published config")})
        row = {"path": "config/modules/modules.json", "target": "cold"}

        retained, reverted = filter_reverted_batch_paths(
            self.root, [row], {"cold": manifest})

        self.assertEqual(retained, [])
        self.assertEqual(reverted, [row["path"]])

    def test_unknown_or_unreadable_baseline_fails_open(self):
        file_path = self.root / "src" / "new.cpp"
        file_path.parent.mkdir(parents=True)
        file_path.write_text("new content", encoding="utf-8")
        row = {"path": "src/new.cpp", "target": "game"}
        bad_manifest = self.manifests / "missing.json"

        retained, reverted = filter_reverted_batch_paths(
            self.root, [row], {"game": bad_manifest})

        self.assertEqual(retained, [row])
        self.assertEqual(reverted, [])

    def test_deleted_or_unlisted_path_fails_open(self):
        manifest = self._manifest(
            "game.json", **{"src/known.cpp": ("source", "published bytes")})
        deleted = {"path": "src/known.cpp", "target": "game"}
        unlisted = {"path": "src/new.cpp", "target": "game"}

        retained, reverted = filter_reverted_batch_paths(
            self.root, [deleted, unlisted], {"game": manifest})

        self.assertEqual(retained, [deleted, unlisted])
        self.assertEqual(reverted, [])

    def test_game_manifest_can_cover_cold_paths_for_same_published_generation(self):
        cold_path = self.root / "src" / "host" / "host-runtime.cpp"
        cold_path.parent.mkdir(parents=True)
        cold_path.write_text("published bytes", encoding="utf-8")
        game_manifest = self._manifest(
            "same-generation.json",
            **{"src/host/host-runtime.cpp": ("source", "published bytes")})
        row = {"path": "src/host/host-runtime.cpp", "target": "cold"}

        shared = can_share_source_generation_baseline(
            27, 27, "rev-a", "rev-a",
            ["batch-1:mode3:cold"], ["batch-1:mode3:game"])
        retained, reverted = filter_reverted_batch_paths(
            self.root, [row], {"cold": game_manifest} if shared else {})

        self.assertTrue(shared)
        self.assertEqual(retained, [])
        self.assertEqual(reverted, [row["path"]])

    def test_game_manifest_does_not_cover_cold_paths_without_exact_provenance(self):
        cases = [
            (27, 28, "rev-a", "rev-a", ["batch:mode3:cold"], ["batch:mode3:game"]),
            (27, 27, "rev-a", "rev-b", ["batch:mode3:cold"], ["batch:mode3:game"]),
            (0, 0, "rev-a", "rev-a", ["batch:mode3:cold"], ["batch:mode3:game"]),
            (None, 27, "rev-a", "rev-a", ["batch:mode3:cold"], ["batch:mode3:game"]),
            (27, 27, "rev-a", "rev-a", ["old:mode3:cold"], ["new:mode3:game"]),
            (27, 27, "rev-a", "rev-a", [], ["batch:mode3:game"]),
        ]
        for values in cases:
            with self.subTest(values=values):
                self.assertFalse(can_share_source_generation_baseline(*values))

    def test_restart_restores_only_valid_unsealed_batch(self):
        saved = {
            "batch_pending": True,
            "batch_generation": 8,
            "batch_targets": ["game"],
            "batch_changed_paths": [
                {"path": "src/npc/npc.cpp", "target": "game"}],
            "batch_started_utc": "2026-11-23T15:01:26.169Z",
            "active_batch": None,
        }

        restored = restore_pending_batch(saved)

        self.assertEqual(restored["generation"], 8)
        self.assertEqual(restored["targets"], {"game"})
        self.assertEqual(restored["changed_paths"], {
            "src/npc/npc.cpp": {"path": "src/npc/npc.cpp", "target": "game"}})
        self.assertEqual(restored["started_utc"], saved["batch_started_utc"])

    def test_restart_does_not_replay_active_or_malformed_batch(self):
        base = {
            "batch_pending": True,
            "batch_generation": 8,
            "batch_targets": ["game"],
            "batch_changed_paths": [
                {"path": "src/npc/npc.cpp", "target": "game"}],
        }

        self.assertIsNone(restore_pending_batch({**base, "active_batch": {"request_id": "x"}}))
        self.assertIsNone(restore_pending_batch({**base, "batch_targets": ["exe"]}))
        self.assertIsNone(restore_pending_batch({**base, "batch_generation": "bad"}))
        self.assertIsNone(restore_pending_batch({**base, "batch_pending": False}))


if __name__ == "__main__":
    unittest.main()
