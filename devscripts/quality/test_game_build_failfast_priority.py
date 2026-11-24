"""Safety checks for prioritizing changed-header compile failures."""

from pathlib import Path
import unittest

from devscripts import game_build_worker


def _entry(path: str) -> tuple[dict, Path, Path]:
    return ({"path": path}, Path(path + ".o"), Path(path + ".d"))


class ChangedHeaderPriorityTests(unittest.TestCase):
    def test_prioritizes_direct_and_transitive_depfile_matches_stably(self) -> None:
        work = [
            _entry("src/unrelated.cpp"),
            _entry("src/vehicles/vehicle-system.cpp"),
            _entry("src/vehicles/vehicle-profile.cpp"),
        ]
        changed = {"src/vehicles/vehicle-types.h",
                   "src/vehicles/vehicle-profile.h"}
        dependencies = {
            "src/vehicles/vehicle-system.cpp": [
                "src/vehicles/vehicle-types.h"],
            "src/vehicles/vehicle-profile.cpp": [
                "src/vehicles/vehicle-profile.h"],
        }

        ordered, metrics = game_build_worker.prioritize_changed_header_dependents(
            work, dependencies, changed)

        self.assertEqual(
            [item[0]["path"] for item in ordered],
            ["src/vehicles/vehicle-system.cpp",
             "src/vehicles/vehicle-profile.cpp", "src/unrelated.cpp"])
        self.assertEqual(len(ordered), len(work))
        self.assertEqual({item[0]["path"] for item in ordered},
                         {item[0]["path"] for item in work})
        self.assertEqual(metrics["prioritized_count"], 2)

    def test_does_not_prioritize_unrelated_or_drop_stale_units(self) -> None:
        work = [_entry("src/a.cpp"), _entry("src/b.cpp")]
        ordered, metrics = game_build_worker.prioritize_changed_header_dependents(
            work,
            {"src/a.cpp": ["src/other.h"]},
            {"src/changed.h"})

        self.assertEqual(ordered, work)
        self.assertEqual(metrics["prioritized_count"], 0)
        self.assertEqual(metrics["candidate_count"], 2)


if __name__ == "__main__":
    unittest.main()
