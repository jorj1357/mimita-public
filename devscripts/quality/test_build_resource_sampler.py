from __future__ import annotations

import json
import os
import tempfile
import time
import unittest
from datetime import datetime, timezone
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from devscripts.dev_loop.build_resource_sampler import BuildResourceSampler


class _FakePsutil:
    @staticmethod
    def cpu_count():
        return 12

    @staticmethod
    def cpu_percent(interval=None):
        return 42.0

    @staticmethod
    def virtual_memory():
        return SimpleNamespace(total=16_000, available=4_000,
                               percent=75.0, used=12_000)

    @staticmethod
    def swap_memory():
        return SimpleNamespace(total=8_000, used=1_000)

    @staticmethod
    def disk_io_counters():
        return SimpleNamespace(read_bytes=100, write_bytes=200)

    @staticmethod
    def process_iter(attrs=()):
        return []


class _FakeWindowsHostMetrics:
    def sample(self):
        return {
            "logical_cpu_count": 12,
            "cpu_percent": 42.0,
            "memory_total_bytes": 16_000,
            "memory_available_bytes": 4_000,
            "memory_used_percent": 75.0,
            "physical_used_bytes": 12_000,
            "pagefile_total_bytes": 8_000,
            "pagefile_used_bytes": 1_000,
            "disk_read_bytes_total": None,
            "disk_write_bytes_total": None,
        }


class _FakeWindowsProcessMetrics:
    def sample(self):
        return [{
            "pid": 12345,
            "parent_pid": 1,
            "executable": "g++.exe",
            "thread_count": 4,
            "create_time": 100,
            "cpu_time_100ns": 10_000_000,
            "rss_bytes": 2_000,
            "private_bytes": 1_000,
            "pagefile_usage_bytes": 1_500,
        }]

    @staticmethod
    def memory(pid):
        return {"rss_bytes": 2_000, "private_bytes": 1_000,
                "pagefile_usage_bytes": 1_500,
                "process_io_counters_available": True,
                "process_io_read_bytes_total": 100,
                "process_io_write_bytes_total": 200,
                "process_io_other_bytes_total": 0,
                "process_io_read_operations_total": 1,
                "process_io_write_operations_total": 2,
                "process_io_other_operations_total": 0}


class BuildResourceSamplerTests(unittest.TestCase):
    def test_records_host_and_build_samples_to_attempt_sidecar(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            progress = root / "build" / "game" / "progress.json"
            progress.parent.mkdir(parents=True)
            progress.write_text(json.dumps({
                "status": "running", "phase": "compile", "elapsed_s": 2.0,
                "completed": 1, "total": 3, "compiled": 1, "skipped": 0,
            }), encoding="utf-8")
            sampler = BuildResourceSampler(root, "game-dll-test", progress,
                                            interval_seconds=0.5)
            with patch(
                    "devscripts.dev_loop.build_resource_sampler.importlib.import_module",
                    return_value=_FakePsutil()):
                sampler.start()
                time.sleep(0.6)
                summary = sampler.stop()

            self.assertEqual(summary["status"], "partial")
            self.assertGreaterEqual(summary["sample_count"], 1)
            sidecar = root / summary["jsonl_path"]
            self.assertTrue(sidecar.is_file())
            records = [json.loads(line) for line in sidecar.read_text(
                encoding="utf-8").splitlines()]
            samples = [row for row in records if row.get("event") == "resource.sample"]
            self.assertTrue(samples)
            self.assertEqual(samples[0]["host"]["logical_cpu_count"], 12)
            self.assertEqual(samples[0]["build"]["phase"], "compile")
            self.assertTrue(summary["unavailable_fields"])

    def test_missing_psutil_uses_windows_host_fallback(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sampler = BuildResourceSampler(root, "game-dll-no-psutil")
            with patch(
                    "devscripts.dev_loop.build_resource_sampler._WindowsHostMetrics",
                    return_value=_FakeWindowsHostMetrics()):
                with patch(
                        "devscripts.dev_loop.build_resource_sampler._WindowsProcessMetrics",
                        side_effect=OSError("process APIs unavailable")):
                    with patch(
                            "devscripts.dev_loop.build_resource_sampler.importlib.import_module",
                            side_effect=ImportError("psutil unavailable")):
                        sampler.start()
                        summary = sampler.stop()

            self.assertEqual(summary["status"], "partial")
            self.assertEqual(summary["provider"], "windows_kernel32_host_only")
            self.assertIn("tracked_process_metrics", summary["unavailable_fields"])
            records = [json.loads(line) for line in
                       (root / summary["jsonl_path"]).read_text(
                           encoding="utf-8").splitlines()]
            samples = [row for row in records
                       if row.get("event") == "resource.sample"]
            self.assertTrue(samples)
            self.assertEqual(samples[0]["host"]["cpu_percent"], 42.0)
            self.assertEqual(samples[0]["host"]["memory_available_bytes"], 4_000)
            self.assertIsNone(samples[0]["process_discovery_age_seconds"])

    def test_native_fallback_captures_short_attempt_and_marks_cpu_warmup(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sampler = BuildResourceSampler(root, "game-dll-native-short",
                                            interval_seconds=10.0)
            with patch(
                    "devscripts.dev_loop.build_resource_sampler._WindowsHostMetrics",
                    return_value=_FakeWindowsHostMetrics()):
                with patch(
                        "devscripts.dev_loop.build_resource_sampler._WindowsProcessMetrics",
                        return_value=_FakeWindowsProcessMetrics()):
                    with patch(
                            "devscripts.dev_loop.build_resource_sampler.importlib.import_module",
                            side_effect=ImportError("psutil unavailable")):
                        sampler.start()
                        summary = sampler.stop()

            self.assertEqual(summary["status"], "captured")
            self.assertEqual(summary["provider"],
                             "windows_kernel32_host_and_process")
            self.assertIn("per_process_cpu_rate_not_observed",
                          summary["unavailable_fields"])
            records = [json.loads(line) for line in
                       (root / summary["jsonl_path"]).read_text(
                           encoding="utf-8").splitlines()]
            samples = [row for row in records
                       if row.get("event") == "resource.sample"]
            self.assertEqual(len(samples), 1)
            self.assertEqual(samples[0]["tracked_processes"][0]["role"],
                             "compiler_linker")
            self.assertEqual(samples[0]["tracked_processes"][0]["rss_bytes"],
                             2_000)

    def test_native_process_fallback_records_role_cpu_and_memory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sampler = BuildResourceSampler(root, "game-dll-native-process")
            sampler._native_processes = _FakeWindowsProcessMetrics()
            with patch(
                    "devscripts.dev_loop.build_resource_sampler.time.monotonic",
                    side_effect=[100.0, 105.0]):
                sampler._discover_native_processes()
                sampler._native_processes.sample = lambda: [{
                    "pid": 12345,
                    "parent_pid": 1,
                    "executable": "g++.exe",
                    "create_time": 100,
                    "cpu_time_100ns": 60_000_000,
                    "rss_bytes": 2_000,
                    "private_bytes": 1_000,
                    "pagefile_usage_bytes": 1_500,
                }]
                sampler._discover_native_processes()

            row = sampler._top_processes[0]
            self.assertEqual(row["role"], "compiler_linker")
            self.assertEqual(row["cpu_percent_one_logical_cpu"], 100.0)
            self.assertEqual(row["cpu_percent_whole_host"],
                             round(100.0 / max(1, os.cpu_count()), 3))
            self.assertEqual(row["rss_bytes"], 2_000)
            self.assertEqual(row["private_bytes"], 1_000)

    def test_native_process_io_rates_and_thread_count_are_pid_start_scoped(self):
        class ChangingIoMetrics:
            def __init__(self):
                self.snapshots = [
                    {"pid": 12345, "parent_pid": 1, "executable": "g++.exe",
                     "create_time": 100, "cpu_time_100ns": 10_000_000,
                     "thread_count": 7},
                    {"pid": 12345, "parent_pid": 1, "executable": "g++.exe",
                     "create_time": 100, "cpu_time_100ns": 20_000_000,
                     "thread_count": 8},
                    {"pid": 12345, "parent_pid": 1, "executable": "g++.exe",
                     "create_time": 101, "cpu_time_100ns": 30_000_000,
                     "thread_count": 9},
                ]
                self.memory_rows = [
                    {"rss_bytes": 2_000, "private_bytes": 1_000,
                     "pagefile_usage_bytes": 1_500,
                     "process_io_counters_available": True,
                     "process_io_read_bytes_total": 1_000,
                     "process_io_write_bytes_total": 200,
                     "process_io_other_bytes_total": 50,
                     "process_io_read_operations_total": 10,
                     "process_io_write_operations_total": 2,
                     "process_io_other_operations_total": 1},
                    {"rss_bytes": 2_000, "private_bytes": 1_000,
                     "pagefile_usage_bytes": 1_500,
                     "process_io_counters_available": True,
                     "process_io_read_bytes_total": 6_000,
                     "process_io_write_bytes_total": 1_200,
                     "process_io_other_bytes_total": 100,
                     "process_io_read_operations_total": 20,
                     "process_io_write_operations_total": 7,
                     "process_io_other_operations_total": 2},
                    {"rss_bytes": 2_000, "private_bytes": 1_000,
                     "pagefile_usage_bytes": 1_500,
                     "process_io_counters_available": True,
                     "process_io_read_bytes_total": 50_000,
                     "process_io_write_bytes_total": 2_200,
                     "process_io_other_bytes_total": 200,
                     "process_io_read_operations_total": 40,
                     "process_io_write_operations_total": 12,
                     "process_io_other_operations_total": 4},
                ]

            def sample(self):
                return [self.snapshots.pop(0)]

            def memory(self, _pid):
                return self.memory_rows.pop(0)

        with tempfile.TemporaryDirectory() as temporary:
            sampler = BuildResourceSampler(Path(temporary), "native-io-rates")
            sampler._native_processes = ChangingIoMetrics()
            with patch(
                    "devscripts.dev_loop.build_resource_sampler.time.monotonic",
                    side_effect=[100.0, 105.0, 110.0]):
                sampler._discover_native_processes()
                sampler._discover_native_processes()
                row = sampler._top_processes[0]
                self.assertEqual(row["thread_count"], 8)
                self.assertEqual(row["process_io_read_bytes_per_second"], 1_000.0)
                self.assertEqual(row["process_io_write_bytes_per_second"], 200.0)
                self.assertEqual(row["process_io_read_operations_per_second"], 2.0)
                self.assertEqual(row["process_io_sample_interval_seconds"], 5.0)

                sampler._discover_native_processes()
                row = sampler._native_tracked_processes[12345]
                self.assertEqual(row["thread_count"], 9)
                self.assertNotIn("process_io_read_bytes_per_second", row)
                self.assertIsNone(row["process_io_sample_interval_seconds"])

    def test_runtime_context_uses_the_matching_pid_run_and_journal(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / "logs" / "2026-11-23" / "isolated-horde"
            server_dir = run_dir / "server-01"
            server_dir.mkdir(parents=True)
            run_id = "isolated-horde"
            started = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
            journal = server_dir / "events.jsonl"
            journal.write_text("\n".join([
                json.dumps({"wall_time": started, "run_id": "root-bloodtest",
                            "pid": 321, "event": "game.hotness.summary",
                            "fields": {"generation": 99, "npcs": 0}}),
                json.dumps({"wall_time": started, "run_id": run_id,
                            "pid": 321, "event": "anim.melee.pose_probe",
                            "fields": {"mode": "jugger_horde"}}),
                json.dumps({"wall_time": started, "run_id": run_id,
                            "pid": 321, "event": "game.hotness.summary",
                            "fields": {"generation": 573, "npcs": 67,
                                       "players": 1}}),
                json.dumps({"wall_time": started, "run_id": run_id,
                            "pid": 321, "event": "npc.server-phase-window",
                            "fields": {"npc_count": 67, "ticks": 60,
                                       "total_ms": 900}}),
            ]) + "\n", encoding="utf-8")
            (run_dir / "run.json").write_text(json.dumps({
                "run_id": run_id, "test_run_kind": "parallel_agent_test",
                "mode": "launcher label", "map": "dust2cyberiav4",
                "build_identity": {"game_generation": 573},
                "instances": [{"instance_id": "server-01", "role": "server",
                               "pid": 321, "process_started_utc": started,
                               "journal_path": str(journal)}],
            }), encoding="utf-8")

            sampler = BuildResourceSampler(root, "runtime-attribution")
            sampler._psutil = object()
            sampler._processes = {321: (None, {
                "role": "mimita",
                "create_time": datetime.fromisoformat(
                    started.replace("Z", "+00:00")).timestamp()})}
            sampler._refresh_runtime_context()

            context = sampler._runtime_context_by_pid[321]
            self.assertEqual(context["run_id"], run_id)
            self.assertEqual(context["manifest_mode"], "launcher label")
            self.assertEqual(context["game_generation"], 573)
            self.assertEqual(context["journal"]["actual_mode"], "jugger_horde")
            self.assertEqual(context["journal"]["generation_npc_player"]["fields"],
                             {"generation": 573, "npcs": 67, "players": 1})
            self.assertEqual(context["journal"]["server_tick_window"]["fields"],
                             {"npc_count": 67, "ticks": 60, "total_ms": 900})

    def test_runtime_context_rejects_manifest_for_reused_pid(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / "logs" / "2026-11-23" / "old-run"
            server_dir = run_dir / "server-01"
            server_dir.mkdir(parents=True)
            journal = server_dir / "events.jsonl"
            journal.write_text("", encoding="utf-8")
            old_start = "2026-11-23T00:00:00Z"
            (run_dir / "run.json").write_text(json.dumps({
                "run_id": "old-run", "mode": "old mode",
                "instances": [{"instance_id": "server-01", "role": "server",
                               "pid": 321, "process_started_utc": old_start,
                               "journal_path": str(journal)}],
            }), encoding="utf-8")

            sampler = BuildResourceSampler(root, "runtime-pid-reuse")
            sampler._psutil = object()
            sampler._processes = {321: (None, {
                "role": "mimita",
                "create_time": datetime.now(timezone.utc).timestamp()})}
            sampler._refresh_runtime_context()
            self.assertNotIn(321, sampler._runtime_context_by_pid)

    def test_runtime_alignment_requires_same_run_and_fresh_mode_frame_tick_rows(self):
        sampler = BuildResourceSampler(Path("."), "runtime-alignment")
        observed = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
        fresh_row = {"observed_utc": observed}
        sampler._runtime_context_by_pid = {
            321: {"run_id": "matched", "role": "server", "pid": 321,
                  "journal": {"generation_npc_player": dict(fresh_row),
                              "server_tick_window": dict(fresh_row)}},
            322: {"run_id": "matched", "role": "client", "pid": 322,
                  "journal": {"client_frame_window": dict(fresh_row),
                              "client_npc_view_window": dict(fresh_row),
                              "journal_mode": dict(fresh_row),
                              "actual_mode": "jugger_horde"}},
            323: {"run_id": "other", "role": "client", "pid": 323,
                  "journal": {"client_frame_window": dict(fresh_row),
                              "client_npc_view_window": dict(fresh_row),
                              "journal_mode": dict(fresh_row),
                              "actual_mode": "bloodtest"}},
        }

        result = sampler._runtime_alignment_summary()
        self.assertTrue(result["same_run_server_client"])
        self.assertEqual(result["run_id"], "matched")
        self.assertEqual(result["server_pid"], 321)
        self.assertEqual(result["client_pid"], 322)
        self.assertEqual(result["actual_mode"], "jugger_horde")

        sampler._runtime_context_by_pid[321]["journal"][
            "server_tick_window"]["observed_utc"] = "2020-01-01T00:00:00Z"
        result = sampler._runtime_alignment_summary()
        self.assertFalse(result["same_run_server_client"])


if __name__ == "__main__":
    unittest.main()
