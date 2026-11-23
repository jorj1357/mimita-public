"""Safety gates for the source-only object-cache marker fast path."""

from __future__ import annotations

import os
import time
from pathlib import Path
import tempfile
import unittest

from devscripts import game_build_worker


def _change_report() -> dict:
    return {
        "old_revision": "old-revision",
        "new_revision": "new-revision",
        "added": [],
        "removed": [],
        "renamed": [],
        "changed": ["src/a.cpp"],
        "added_headers": [],
        "removed_headers": [],
        "changed_headers": [],
        "changed_owners": [],
    }


class SourceOnlyCacheMarkerTests(unittest.TestCase):
    def test_accepts_only_existing_cpp_content_changes(self) -> None:
        self.assertEqual(game_build_worker.source_only_changed_paths(
            _change_report()), {"src/a.cpp"})
        report = _change_report()
        report["changed"] = [r"src\gui\ui.cpp"]
        self.assertEqual(game_build_worker.source_only_changed_paths(report),
                         {"src/gui/ui.cpp"})

    def test_rejects_incomplete_or_non_source_only_reports(self) -> None:
        hazards = (
            ("added", ["src/new.cpp"]),
            ("removed", ["src/old.cpp"]),
            ("renamed", [{"from": "src/a.cpp", "to": "src/b.cpp"}]),
            ("added_headers", ["src/new.h"]),
            ("removed_headers", ["src/old.h"]),
            ("changed_headers", ["src/header.h"]),
            ("changed_owners", [{"path": "src/a.cpp"}]),
            ("changed", ["src/a.cpp", "src/header.h"]),
            ("changed", ["../outside.cpp"]),
            ("changed", []),
        )
        for key, value in hazards:
            with self.subTest(key=key, value=value):
                report = _change_report()
                report[key] = value
                self.assertIsNone(
                    game_build_worker.source_only_changed_paths(report))

    def test_reuses_last_published_marker_for_unchanged_object_set(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            object_dir = Path(temporary) / "objects"
            object_dir.mkdir()
            prior_changed = object_dir / "old-a.o"
            unchanged = object_dir / "b.o"
            current_changed = object_dir / "new-a.o"
            prior_changed.write_bytes(b"old compiled object")
            unchanged.write_bytes(b"unchanged compiled object")
            current_changed.write_bytes(b"new compiled object")
            now_ns = time.time_ns() - 10_000_000
            for path in (prior_changed, unchanged):
                path.touch()
                os.utime(path, ns=(now_ns, now_ns))

            marker_objects = game_build_worker.marker_objects(object_dir)
            marker = {
                "schema_version": 1,
                "revision": "old-revision",
                "generation": 7,
                "flags_identity": "flags",
                "link_identity": "link",
                "compiler_stamp": "compiler",
                "code_hash": "published-code",
                "objects": {
                    name: [size, mtime]
                    for name, (size, mtime) in marker_objects.items()
                    if name in {prior_changed.name, unchanged.name}
                },
                "external_dependencies": [],
            }
            published = {
                "status": "ok",
                "generation": 7,
                "revision": "old-revision",
                "code_hash": "published-code",
                "object_cache": "build\\game\\objects\\cache-key",
            }
            object_paths = {
                "src/a.cpp": current_changed,
                "src/b.cpp": unchanged,
            }

            self.assertTrue(game_build_worker.source_only_cache_marker_valid(
                _change_report(), "new-revision", {"src/a.cpp"},
                {"src/a.cpp"}, object_paths, object_dir, marker, published,
                "build\\game\\objects\\cache-key", "flags", "link",
                "compiler", False))

    def test_falls_back_when_changed_object_is_not_fresh(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            object_dir = Path(temporary) / "objects"
            object_dir.mkdir()
            old_a = object_dir / "old-a.o"
            b = object_dir / "b.o"
            new_a = object_dir / "new-a.o"
            old_a.write_bytes(b"old a")
            b.write_bytes(b"b")
            marker_objects = game_build_worker.marker_objects(object_dir)
            marker = {
                "schema_version": 1, "revision": "old-revision",
                "generation": 7, "flags_identity": "flags",
                "link_identity": "link", "compiler_stamp": "compiler",
                "code_hash": "published-code",
                "objects": {name: [*stat] for name, stat in marker_objects.items()},
                "external_dependencies": [],
            }
            published = {
                "status": "ok", "generation": 7,
                "revision": "old-revision", "code_hash": "published-code",
                "object_cache": "cache-key",
            }
            self.assertFalse(game_build_worker.source_only_cache_marker_valid(
                _change_report(), "new-revision", {"src/a.cpp"}, set(),
                {"src/a.cpp": new_a, "src/b.cpp": b}, object_dir, marker,
                published, "cache-key", "flags", "link", "compiler", False))

    def test_falls_back_when_an_unchanged_published_object_is_missing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            object_dir = Path(temporary) / "objects"
            object_dir.mkdir()
            old_a = object_dir / "old-a.o"
            b = object_dir / "b.o"
            old_a.write_bytes(b"old a")
            b.write_bytes(b"b")
            marker_objects = game_build_worker.marker_objects(object_dir)
            marker = {
                "schema_version": 1, "revision": "old-revision",
                "generation": 7, "flags_identity": "flags",
                "link_identity": "link", "compiler_stamp": "compiler",
                "code_hash": "published-code",
                "objects": {name: [*stat]
                            for name, stat in marker_objects.items()},
                "external_dependencies": [],
            }
            published = {
                "status": "ok", "generation": 7,
                "revision": "old-revision", "code_hash": "published-code",
                "object_cache": "cache-key",
            }
            b.unlink()
            self.assertFalse(game_build_worker.source_only_cache_marker_valid(
                _change_report(), "new-revision", {"src/a.cpp"},
                {"src/a.cpp"},
                {"src/a.cpp": object_dir / "new-a.o", "src/b.cpp": b},
                object_dir, marker, published, "cache-key", "flags", "link",
                "compiler", False))


if __name__ == "__main__":
    unittest.main()
