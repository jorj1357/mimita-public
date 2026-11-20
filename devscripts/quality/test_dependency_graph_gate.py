"""Focused tests for the header fan-out allowlist gate."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.dependency_graph import evaluate_fanout  # noqa: E402


def graph(*rows: tuple[str, int]) -> dict:
    return {"fan_in": [{"path": path, "dependents": count}
                       for path, count in rows]}


class HeaderFanoutGateTests(unittest.TestCase):
    def test_unlisted_wide_header_is_a_violation(self) -> None:
        result = evaluate_fanout(graph(("src/new-wide.h", 150)), limit=100)
        self.assertFalse(result["ok"])
        self.assertEqual(result["violations"][0]["path"], "src/new-wide.h")
        self.assertEqual(result["violations"][0]["reason"],
                         "fan-out exceeds limit without allowlist entry")

    def test_allowlisted_header_within_max_is_ok(self) -> None:
        result = evaluate_fanout(
            graph(("src/pch.h", 150)), limit=100,
            allowlist={"allow": [{"path": "src/pch.h", "max": 200}]})
        self.assertTrue(result["ok"])

    def test_allowlisted_header_growth_is_a_violation(self) -> None:
        result = evaluate_fanout(
            graph(("src/pch.h", 250)), limit=100,
            allowlist={"allow": [{"path": "src/pch.h", "max": 200}]})
        self.assertFalse(result["ok"])
        self.assertEqual(result["violations"][0]["reason"],
                         "fan-out exceeds allowlist maximum")

    def test_narrow_allowlist_entry_is_reported_stale_not_failed(self) -> None:
        result = evaluate_fanout(
            graph(("src/old-wide.h", 50)), limit=100,
            allowlist={"allow": [{"path": "src/old-wide.h", "max": 100}]})
        self.assertTrue(result["ok"])
        self.assertEqual(result["rows"][0]["status"], "stale")

    def test_under_limit_is_ok(self) -> None:
        result = evaluate_fanout(graph(("src/narrow.h", 20)), limit=100)
        self.assertTrue(result["ok"])


if __name__ == "__main__":
    unittest.main()
