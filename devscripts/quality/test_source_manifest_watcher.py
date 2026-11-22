from __future__ import annotations

import os
import tempfile
import time
import unittest
from pathlib import Path

from devscripts import source_manifest


@unittest.skipUnless(os.name == "nt", "Windows directory notifications required")
class SourceManifestWatcherTests(unittest.TestCase):
    def test_warm_inventory_tracks_added_renamed_and_removed_cpp_files(self) -> None:
        with tempfile.TemporaryDirectory(prefix="mimita-source-watch-") as temporary:
            root = Path(temporary)
            source_dir = root / "src"
            source_dir.mkdir()
            source_manifest._INVENTORY_CACHE.pop((root, "cpp"), None)
            source_manifest._INVENTORY_WATCHER_CURSORS.pop((root, "cpp"), None)
            (root / ".dev" / "source-inventory-cpp-cache.json").unlink(
                missing_ok=True)
            (source_dir / "first.cpp").write_text("int first;\n", encoding="utf-8")

            paths, _first = source_manifest._cached_inventory(root, "cpp")
            self.assertEqual(paths, ["src/first.cpp"])

            paths, warm = source_manifest._cached_inventory(root, "cpp")
            self.assertEqual(paths, ["src/first.cpp"])
            self.assertTrue(warm["watcher_cache"])

            added = source_dir / "second.cpp"
            added.write_text("int second;\n", encoding="utf-8")
            self._wait_for_paths(root, ["src/first.cpp", "src/second.cpp"])

            renamed = source_dir / "renamed.cpp"
            added.rename(renamed)
            self._wait_for_paths(root, ["src/first.cpp", "src/renamed.cpp"])

            renamed.unlink()
            self._wait_for_paths(root, ["src/first.cpp"])

    @staticmethod
    def _wait_for_paths(root: Path, expected: list[str]) -> None:
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            paths, _ = source_manifest._cached_inventory(root, "cpp")
            if paths == expected:
                return
            time.sleep(0.02)
        raise AssertionError(f"source inventory did not reach {expected!r}")


if __name__ == "__main__":
    unittest.main()
