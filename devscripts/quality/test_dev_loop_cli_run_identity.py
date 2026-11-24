"""Tests for fail-closed dev-loop agent-command target selection."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT / "devscripts") not in sys.path:
    sys.path.insert(0, str(ROOT / "devscripts"))

from dev_loop.cli import command_target_run_error, create_parser  # noqa: E402


class CommandRunIdentityTests(unittest.TestCase):
    def test_explicit_client_requires_expected_run(self) -> None:
        args = create_parser().parse_args([
            "--client", "client-01", "--command", "agent.gui_scan",
        ])
        self.assertIn("--expected-run-id is required",
                      command_target_run_error(args, {"run_id": "run-a"}))

    def test_matching_run_allows_explicit_client(self) -> None:
        args = create_parser().parse_args([
            "--client", "client-01", "--expected-run-id", "run-a",
            "--command", "agent.gui_scan",
        ])
        self.assertIsNone(command_target_run_error(args, {"run_id": "run-a"}))

    def test_mismatched_run_is_refused(self) -> None:
        args = create_parser().parse_args([
            "--client", "client-01", "--expected-run-id", "run-a",
            "--command", "agent.gui_scan",
        ])
        self.assertIn("active target run is run-b",
                      command_target_run_error(args, {"run_id": "run-b"}))

    def test_runtime_commands_pin_the_runtime_run(self) -> None:
        args = create_parser().parse_args([
            "--runtime", "payload-test", "--expected-run-id", "run-test",
            "--command", "agent.gui_scan",
        ])
        current = {
            "run_id": "different-root-run",
            "runtimes": [{"runtime_id": "payload-test", "run_id": "run-test"}],
        }
        self.assertIsNone(command_target_run_error(args, current))


if __name__ == "__main__":
    unittest.main()
