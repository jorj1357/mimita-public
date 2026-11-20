"""Focused tests for launch timeline and failure reports."""

from __future__ import annotations

import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.dev_loop import launch_report  # noqa: E402


class LaunchReportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(
            prefix="launch-report-", dir=ROOT / ".dev" / "tmp")
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_failed_launch_has_code_stage_and_probe(self) -> None:
        launch_report.start_launch(self.root, "launch-1", "req-1")
        launch_report.append_stage(self.root, "launch-1", {
            "stage": "server_logger_readiness", "result": "failed",
            "stage_elapsed_ms": 20000,
        })
        summary = launch_report.finish_launch(
            self.root, "launch-1", "complete", "failed",
            "server logger.started timeout")
        self.assertEqual(summary["failure_code"], "server_logger_timeout")
        self.assertEqual(summary["failure_stage"], "server_logger_readiness")
        report = launch_report.probe_report(self.root, "launch-1")
        self.assertEqual(report["failed_stage"]["stage"],
                         "server_logger_readiness")
        self.assertTrue((launch_report.report_dir(self.root, "launch-1")
                         / "report.md").is_file())

    def test_failed_launch_detail_comes_from_first_failed_stage(self) -> None:
        launch_report.start_launch(self.root, "launch-first-failure")
        launch_report.append_stage(self.root, "launch-first-failure", {
            "stage": "client_logger_readiness", "result": "failed",
            "reason": "client pid=123 did not write logger.started",
        })
        launch_report.append_stage(self.root, "launch-first-failure", {
            "stage": "mode_start", "result": "failed",
            "reason": "managed mode did not become active",
        })

        summary = launch_report.finish_launch(
            self.root, "launch-first-failure", "complete", "failed",
            "managed mode did not become active")

        self.assertEqual(summary["failure_code"], "client_logger_timeout")
        self.assertEqual(summary["failure_stage"], "client_logger_readiness")
        self.assertEqual(summary["failure_detail"],
                         "client pid=123 did not write logger.started")

    def test_success_has_no_failure_code(self) -> None:
        launch_report.start_launch(self.root, "launch-2")
        launch_report.finish_launch(
            self.root, "launch-2", "complete", "succeeded",
            "ready server/client")
        report = launch_report.load_report(self.root, "launch-2")
        self.assertEqual(report["result"], "succeeded")
        self.assertEqual(report["failure_code"], "")

    def test_failed_launch_records_automatic_issue_result(self) -> None:
        launch_report.start_launch(self.root, "launch-issue")
        launch_report.append_stage(self.root, "launch-issue", {
            "stage": "client_connection", "result": "failed",
        })
        with mock.patch.object(launch_report, "_create_launch_issue",
                               return_value={"status": "appended",
                                             "issue_id": "issue-existing"}) as create:
            summary = launch_report.finish_launch(
                self.root, "launch-issue", "complete", "failed",
                "connection timeout", create_issue=True)
        create.assert_called_once()
        self.assertEqual(summary["issue_result"], {
            "status": "appended", "issue_id": "issue-existing"})
        report = (launch_report.report_dir(self.root, "launch-issue")
                  / "report.md").read_text(encoding="utf-8")
        self.assertIn("Automatic issue: `appended`", report)
        self.assertIn("Issue ID: `issue-existing`", report)

    def test_success_does_not_create_failure_issue(self) -> None:
        launch_report.start_launch(self.root, "launch-success")
        with mock.patch.object(launch_report, "_create_launch_issue") as create:
            summary = launch_report.finish_launch(
                self.root, "launch-success", "complete", "succeeded",
                create_issue=True)
        create.assert_not_called()
        self.assertEqual(summary["issue_result"]["status"], "not_needed")

    def test_latest_report_lists_newest(self) -> None:
        launch_report.start_launch(self.root, "launch-a")
        launch_report.finish_launch(
            self.root, "launch-a", "complete", "succeeded")
        launch_report.start_launch(self.root, "launch-b")
        launch_report.finish_launch(
            self.root, "launch-b", "complete", "succeeded")
        rows = launch_report.list_reports(self.root)
        self.assertEqual(rows[0]["launch_id"], "launch-b")


if __name__ == "__main__":
    unittest.main()

