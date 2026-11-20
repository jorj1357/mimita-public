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
)
from devscripts.game_build_worker import dependency_relative  # noqa: E402

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

    def test_vendor_headers_are_not_project_fanout(self) -> None:
        self.assertEqual(dependency_relative("include/glm/glm.hpp"), "")
        self.assertEqual(dependency_relative("include/GLFW/glfw3.h"), "")
        self.assertEqual(dependency_relative("src/map/map_common.h"),
                         "src/map/map_common.h")


if __name__ == "__main__":
    unittest.main()

