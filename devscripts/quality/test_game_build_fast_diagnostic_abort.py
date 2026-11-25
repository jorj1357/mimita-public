"""Checks for immediate cancellation after GCC reports a hard compile error."""

import subprocess
import sys
import threading
import time
import unittest

from devscripts.game_build_worker import (
    _is_gcc_compile_error_diagnostic,
    _run_tracked_compile_process,
)


class GameBuildFastDiagnosticAbortTests(unittest.TestCase):
    def test_matches_gcc_and_mingw_errors_without_matching_notes(self):
        self.assertTrue(_is_gcc_compile_error_diagnostic(
            r"C:\work tree\src\bad.cpp:1186:23: error: no member named remainder"))
        self.assertTrue(_is_gcc_compile_error_diagnostic(
            "include/missing.h: fatal error: file not found"))
        self.assertFalse(_is_gcc_compile_error_diagnostic(
            "src/good.cpp:8:3: note: this string contains error: as text"))
        self.assertFalse(_is_gcc_compile_error_diagnostic(
            '    const char* message = "error: example";'))

    def test_kills_compiler_after_first_error_instead_of_waiting_for_exit(self):
        active = {}
        active_lock = threading.Lock()
        stop_event = threading.Event()
        command = [
            sys.executable, "-c",
            "import time; "
            "print(r'C:\\src\\bad.cpp:7:2: error: expected token', flush=True); "
            "time.sleep(20)",
        ]

        def terminate(_pid, process):
            if process.poll() is None:
                process.kill()

        started = time.monotonic()
        result = _run_tracked_compile_process(
            command,
            {"stdout": subprocess.PIPE, "stderr": subprocess.STDOUT,
             "text": True},
            active_processes=active,
            active_lock=active_lock,
            stop_event=stop_event,
            terminate_process=terminate,
        )
        elapsed = time.monotonic() - started

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("error: expected token", result.stdout)
        self.assertLess(elapsed, 4.0)
        self.assertEqual(active, {})


if __name__ == "__main__":
    unittest.main()
