import unittest
import os
import subprocess

from devscripts.game_build_worker import (
    PRIORITY_CLASSES,
    build_tool_creation_flags,
    is_unsupported_time_report,
    parse_gcc_time_report,
)


class GccTimeReportTests(unittest.TestCase):
    def test_parses_phase_wall_times_and_total(self):
        output = """compiler diagnostic before report
Time variable                                   usr           sys          wall           GGC
 phase parsing                    :   1.25 ( 25%)   0.20 ( 10%)   1.45 ( 22%)  120M ( 8%)
 phase opt and generate            :   2.00 ( 40%)   0.30 ( 15%)   2.30 ( 35%)  200M (13%)
 TOTAL                              :   5.00           2.00           6.50           1.5G
"""

        report = parse_gcc_time_report(output)

        self.assertEqual(report["status"], "parsed")
        self.assertFalse(report["truncated"])
        self.assertEqual(len(report["rows"]), 3)
        self.assertEqual(report["rows"][0]["phase"], "phase parsing")
        self.assertEqual(report["rows"][0]["wall_seconds"], 1.45)
        self.assertEqual(report["rows"][0]["wall_percent"], 22)
        self.assertEqual(report["rows"][0]["ggc"], {
            "value": 120.0, "unit": "M", "percent": 8,
        })
        self.assertEqual(report["rows"][-1]["phase"], "TOTAL")
        self.assertEqual(report["rows"][-1]["wall_seconds"], 6.5)

    def test_missing_report_is_explicit_and_nonfatal(self):
        report = parse_gcc_time_report("compiled successfully\n")

        self.assertEqual(report, {
            "format": "gcc-ftime-report", "status": "missing", "rows": [],
        })

    def test_unparsed_report_keeps_only_a_bounded_excerpt(self):
        output = "Time variable usr sys wall GGC\nphase parsing: unavailable\n"
        report = parse_gcc_time_report(output)

        self.assertEqual(report["status"], "empty")
        self.assertIn("phase parsing: unavailable", report["excerpt"])
        self.assertLessEqual(len(report["excerpt"]), 1600)

    def test_parses_wall_only_report_format(self):
        report = parse_gcc_time_report("""Time variable                                   wall          GGC
 phase parsing                    :   1.45 ( 22%)  120M ( 8%)
 TOTAL                              :   6.50           1.5G
""")

        self.assertEqual(report["status"], "parsed")
        self.assertEqual(report["rows"][0]["wall_seconds"], 1.45)
        self.assertEqual(report["rows"][0]["wall_percent"], 22)
        self.assertNotIn("user_seconds", report["rows"][0])
        self.assertEqual(report["rows"][0]["ggc"]["unit"], "M")
        self.assertEqual(report["rows"][-1]["wall_seconds"], 6.5)

    def test_report_rows_are_bounded(self):
        rows = "\n".join(
            f" phase {i} : 1.0 0.1 1.1 2M" for i in range(3))
        report = parse_gcc_time_report(
            "Time variable usr sys wall GGC\n" + rows, max_rows=2)

        self.assertEqual(len(report["rows"]), 2)
        self.assertTrue(report["truncated"])

    def test_report_at_row_limit_is_not_marked_truncated(self):
        rows = "\n".join(
            f" phase {i} : 1.0 0.1 1.1 2M" for i in range(2))
        report = parse_gcc_time_report(
            "Time variable usr sys wall GGC\n" + rows, max_rows=2)

        self.assertEqual(len(report["rows"]), 2)
        self.assertFalse(report["truncated"])

    def test_only_flag_rejection_enables_optional_fallback(self):
        self.assertTrue(is_unsupported_time_report(
            "g++.exe: error: unrecognized command-line option '-ftime-report'"))
        self.assertTrue(is_unsupported_time_report(
            "error: unknown argument '-ftime-report'"))
        self.assertFalse(is_unsupported_time_report(
            "undefined reference to '-ftime-report'"))
        self.assertFalse(is_unsupported_time_report(
            "g++.exe: fatal error: source file not found"))

    @unittest.skipUnless(os.name == "nt", "Windows process flags only")
    def test_compiler_child_flags_include_requested_priority(self):
        flags = build_tool_creation_flags("high")

        self.assertEqual(flags & PRIORITY_CLASSES["high"],
                         PRIORITY_CLASSES["high"])
        self.assertEqual(flags & subprocess.CREATE_NO_WINDOW,
                         subprocess.CREATE_NO_WINDOW)


if __name__ == "__main__":
    unittest.main()
