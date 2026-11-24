"""Regression checks for game-worker timeout/failure races."""

import subprocess
import unittest

from devscripts.dev_loop.game_build import (
    _terminal_attempt_failure_after_timeout,
    _wait_for_process_exit,
)


class _FakeProcess:
    def __init__(self, *, result=None, exits_during_wait=False):
        self.returncode = result
        self.exits_during_wait = exits_during_wait
        self.wait_calls = 0

    def poll(self):
        return self.returncode

    def wait(self, timeout=None):
        self.wait_calls += 1
        if self.exits_during_wait:
            self.returncode = 1
            return self.returncode
        raise subprocess.TimeoutExpired("fake-worker", timeout)


class GameBuildTimeoutClassificationTests(unittest.TestCase):
    def test_already_exited_process_does_not_wait_again(self):
        process = _FakeProcess(result=1)

        self.assertTrue(_wait_for_process_exit(process, 2.0))
        self.assertEqual(process.wait_calls, 0)

    def test_process_that_exits_during_grace_is_not_a_timeout(self):
        process = _FakeProcess(exits_during_wait=True)

        self.assertTrue(_wait_for_process_exit(process, 2.0))
        self.assertEqual(process.wait_calls, 1)

    def test_still_running_process_exhausts_grace(self):
        process = _FakeProcess()

        self.assertFalse(_wait_for_process_exit(process, 2.0))
        self.assertEqual(process.wait_calls, 1)

    def test_matching_terminal_link_failure_is_preserved(self):
        record = {
            "build_attempt_id": "attempt-current",
            "status": "failed",
            "phase": "link",
            "error": "link failed in 129.32s",
        }

        self.assertTrue(_terminal_attempt_failure_after_timeout(
            "attempt-current", record))

    def test_stale_or_incomplete_attempt_record_cannot_mask_timeout(self):
        self.assertFalse(_terminal_attempt_failure_after_timeout(
            "attempt-current", {
                "build_attempt_id": "attempt-old",
                "status": "failed",
                "phase": "link",
                "error": "old failure",
            }))
        self.assertFalse(_terminal_attempt_failure_after_timeout(
            "attempt-current", {
                "build_attempt_id": "attempt-current",
                "status": "building",
                "phase": "link",
                "error": "",
            }))
        self.assertFalse(_terminal_attempt_failure_after_timeout(
            "attempt-current", {
                "build_attempt_id": "attempt-current",
                "status": "failed",
                "phase": "link",
                "error": "",
            }))


if __name__ == "__main__":
    unittest.main()
