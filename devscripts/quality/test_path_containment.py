"""Focused path-construction tests for repository containment."""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

try:
    from devscripts.path_containment import (
        is_within,
        repository_temp_dir,
        resolve_coordinator_root,
        resolve_within,
    )
except ModuleNotFoundError:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from devscripts.path_containment import (
        is_within,
        repository_temp_dir,
        resolve_coordinator_root,
        resolve_within,
    )


REPO = Path(__file__).resolve().parents[2]
TEMP_ROOT = REPO / ".dev" / "tmp"


class RepositoryContainmentTests(unittest.TestCase):
    def setUp(self) -> None:
        TEMP_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(
            prefix="path-containment-", dir=TEMP_ROOT)
        self.root = Path(self.temporary.name)
        (self.root / ".dev" / "tmp").mkdir(parents=True)
        self.previous_coordinator = os.environ.get("MIMITA_COORDINATOR_ROOT")
        os.environ.pop("MIMITA_COORDINATOR_ROOT", None)

    def tearDown(self) -> None:
        if self.previous_coordinator is None:
            os.environ.pop("MIMITA_COORDINATOR_ROOT", None)
        else:
            os.environ["MIMITA_COORDINATOR_ROOT"] = self.previous_coordinator
        self.temporary.cleanup()

    def test_inside_path_is_allowed(self) -> None:
        candidate = self.root / ".dev" / "tmp" / "lane"
        self.assertEqual(resolve_within(self.root, candidate), candidate.resolve())

    def test_parent_and_absolute_outside_paths_are_rejected(self) -> None:
        with self.assertRaises(RuntimeError):
            resolve_within(self.root, self.root / ".." / "outside")
        with self.assertRaises(RuntimeError):
            resolve_within(self.root, Path.home())

    def test_foreign_coordinator_is_rejected(self) -> None:
        os.environ["MIMITA_COORDINATOR_ROOT"] = str(self.root.parent)
        with self.assertRaises(RuntimeError):
            resolve_coordinator_root(self.root)

    def test_lane_root_accepts_its_own_coordinator(self) -> None:
        lane = self.root / ".dev" / "agent-lanes" / "lane"
        lane.mkdir(parents=True)
        os.environ["MIMITA_COORDINATOR_ROOT"] = str(self.root)
        self.assertEqual(resolve_coordinator_root(lane), self.root.resolve())

    def test_repository_temp_dir_stays_inside(self) -> None:
        path = repository_temp_dir(self.root, "../escape")
        self.assertTrue(is_within(self.root, path))
        self.assertEqual(path.parent, (self.root / ".dev" / "tmp").resolve())

    def test_escaping_junction_is_rejected(self) -> None:
        if os.name != "nt":
            self.skipTest("Windows junction test")
        link = self.root / "escape"
        result = subprocess.run(
            ["cmd", "/c", "mklink", "/J", str(link), str(self.root.parent)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if result.returncode != 0:
            self.skipTest("could not create a junction")
        try:
            with self.assertRaises(RuntimeError):
                resolve_within(self.root, link, label="junction")
        finally:
            os.rmdir(link)


if __name__ == "__main__":
    unittest.main()
