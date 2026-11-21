"""Focused tests for per-attempt hardlink source snapshots."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.dev_loop.game_build import (  # noqa: E402
    _copy_into_mirror,
    _create_overlay_snapshot,
    _create_source_snapshot,
    _prune_logs,
)
from devscripts.game_build_worker import (  # noqa: E402
    cached_dependency_tokens,
    dependency_relative,
    remember_dependency_tokens,
)

TEMP_ROOT = ROOT / ".dev" / "tmp"


class SourceSnapshotTests(unittest.TestCase):
    def setUp(self) -> None:
        TEMP_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(
            prefix="source-snapshot-", dir=TEMP_ROOT)
        self.root = Path(self.temporary.name) / "root"
        self.mirror = self.root / ".dev" / "build-source"
        (self.mirror / "src").mkdir(parents=True)
        (self.mirror / "src" / "a.cpp").write_text("old\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_snapshot_keeps_old_content_when_mirror_is_replaced(self) -> None:
        snapshot = _create_source_snapshot(self.root, self.mirror, "attempt-1")
        self.assertEqual((snapshot / "src" / "a.cpp").read_text(),
                         "old\n")
        source = self.root / "src"
        source.mkdir(parents=True)
        (source / "a.cpp").write_text("new\n", encoding="utf-8")
        mismatches: list[str] = []
        _copy_into_mirror(self.root, self.mirror, ["src/a.cpp"], mismatches)
        self.assertEqual(mismatches, [])
        self.assertEqual((snapshot / "src" / "a.cpp").read_text(), "old\n")
        self.assertEqual((self.mirror / "src" / "a.cpp").read_text(), "new\n")

    def test_snapshot_metadata_is_written(self) -> None:
        snapshot = _create_source_snapshot(self.root, self.mirror, "attempt-2")
        self.assertTrue((snapshot / ".snapshot.json").is_file())

    def test_preview_overlay_replaces_only_named_files(self) -> None:
        snapshot = _create_source_snapshot(self.root, self.mirror, "attempt-3")
        source = self.root / "src"
        source.mkdir(parents=True, exist_ok=True)
        (source / "a.cpp").write_text("preview\n", encoding="utf-8")
        (source / "b.cpp").write_text("new\n", encoding="utf-8")
        overlay = _create_overlay_snapshot(
            self.root, snapshot, ["src/a.cpp"], "attempt-4")
        self.assertEqual((overlay / "src" / "a.cpp").read_text(), "preview\n")
        self.assertFalse((overlay / "src" / "b.cpp").exists())
        metadata = json.loads((overlay / ".snapshot.json").read_text())
        self.assertEqual(metadata["base_snapshot"], str(snapshot.relative_to(self.root)))
        self.assertEqual(metadata["overlay_paths"], ["src/a.cpp"])

    def test_dependency_index_reuses_unchanged_depfile(self) -> None:
        dep = self.root / "unit.d"
        dep.write_text("src/a.cpp.o: src/a.cpp src/a.h\n", encoding="utf-8")
        index = {}
        self.assertTrue(remember_dependency_tokens(
            index, "src/a.cpp", "source-hash", dep,
            ["src/a.cpp", "src/a.h"]))
        self.assertEqual(len(index), 1)
        self.assertEqual(cached_dependency_tokens(
            index, "src/a.cpp", "source-hash", dep),
            [str(ROOT / "src/a.cpp"), str(ROOT / "src/a.h")])
        self.assertIsNone(cached_dependency_tokens(
            index, "src/a.cpp", "different-hash", dep))

    def test_log_prune_keeps_newest_date_directories(self) -> None:
        logs = self.root / "logs"
        for name in ("2026-01-01", "2026-01-02", "2026-01-03"):
            (logs / name).mkdir(parents=True)
        self.assertEqual(_prune_logs(self.root, 2), 1)
        self.assertFalse((logs / "2026-01-01").exists())
        self.assertTrue((logs / "2026-01-03").exists())

    def test_vendor_headers_are_not_project_fanout(self) -> None:
        self.assertEqual(dependency_relative("include/glm/glm.hpp"), "")
        self.assertEqual(dependency_relative("include/GLFW/glfw3.h"), "")
        self.assertEqual(dependency_relative("src/map/map_common.h"),
                         "src/map/map_common.h")


if __name__ == "__main__":
    unittest.main()
