"""Cache-safety checks for source-specific PCH policy."""

from __future__ import annotations

import hashlib
import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest import mock

from devscripts import game_build_worker


class PchProfileTests(unittest.TestCase):
    def test_default_source_keeps_legacy_object_identity(self):
        item = {"path": "src/npc/npc.cpp", "sha256": "ab" * 32}
        legacy = hashlib.sha256(
            f"{item['path']}:{item['sha256']}".encode("utf-8")
        ).hexdigest()[:20]

        self.assertEqual(game_build_worker.object_key(item), legacy)

    def test_pch_excluded_source_gets_stable_distinct_object_identity(self):
        item = {"path": "src/npc/npc-world-query.cpp", "sha256": "cd" * 32}
        legacy = hashlib.sha256(
            f"{item['path']}:{item['sha256']}".encode("utf-8")
        ).hexdigest()[:20]

        self.assertNotEqual(game_build_worker.object_key(item), legacy)
        self.assertEqual(game_build_worker.object_key(item),
                         game_build_worker.object_key(item))

    def test_pch_policy_is_source_scoped(self):
        self.assertFalse(game_build_worker.source_uses_pch(
            "src/npc/npc-world-query.cpp", True))
        self.assertTrue(game_build_worker.source_uses_pch(
            "src/npc/npc.cpp", True))
        self.assertFalse(game_build_worker.source_uses_pch(
            "src/npc/npc.cpp", False))

    def test_pch_mtime_only_invalidates_pch_enabled_object(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source_dir = root / "src"
            source_dir.mkdir()
            pch = source_dir / "pch.h.gch"
            pch.write_bytes(b"pch")
            obj = root / "test.o"
            dep = root / "test.d"
            obj.write_bytes(b"object")
            dep.write_text("test.o: src/pch.h.gch\n", encoding="utf-8")

            now_ns = time.time_ns()
            os.utime(obj, ns=(now_ns - 2_000_000_000,
                              now_ns - 2_000_000_000))
            os.utime(pch, ns=(now_ns - 1_000_000_000,
                              now_ns - 1_000_000_000))

            with mock.patch.object(game_build_worker, "ROOT", root):
                self.assertTrue(game_build_worker.dep_is_stale(
                    obj, dep, use_pch=True))
                self.assertFalse(game_build_worker.dep_is_stale(
                    obj, dep, use_pch=False))

    def test_no_pch_object_still_tracks_normal_header_freshness(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source_dir = root / "src"
            source_dir.mkdir()
            header = source_dir / "fresh.h"
            header.write_text("// fresh\n", encoding="utf-8")
            obj = root / "test.o"
            dep = root / "test.d"
            obj.write_bytes(b"object")
            dep.write_text("test.o: src/fresh.h\n", encoding="utf-8")

            old_ns = time.time_ns() - 2_000_000_000
            os.utime(obj, ns=(old_ns, old_ns))

            with mock.patch.object(game_build_worker, "ROOT", root):
                self.assertTrue(game_build_worker.dep_is_stale(
                    obj, dep, use_pch=False))


if __name__ == "__main__":
    unittest.main()
