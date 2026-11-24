from __future__ import annotations

import json
import os
import tempfile
import time
import unittest
from datetime import datetime, timedelta, timezone
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
                            "pid": 321,
                            "event": "performance.server-tick-window",
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

    def test_runtime_context_uses_launch_id_when_instance_start_is_missing(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / "logs" / "2026-11-24" / "current-run"
            server_dir = run_dir / "server-01"
            server_dir.mkdir(parents=True)
            process_start = datetime.now(timezone.utc)
            launch_time = process_start - timedelta(seconds=8)
            launch_utc = launch_time.isoformat().replace("+00:00", "Z")
            observed = process_start.isoformat().replace("+00:00", "Z")
            journal = server_dir / "events.jsonl"
            journal.write_text(json.dumps({
                "wall_time": observed, "run_id": "current-run", "pid": 321,
                "event": "game.hotness.summary",
                "fields": {"generation": 581, "npcs": 67, "players": 0},
            }) + "\n", encoding="utf-8")
            (run_dir / "run.json").write_text(json.dumps({
                "run_id": "current-run", "created_utc": launch_utc,
                "instances": [{"instance_id": "server-01", "role": "server",
                               "pid": 321, "launch_id": launch_utc,
                               "executable": "C:\\game\\mimita.exe",
                               "journal_path": str(journal)}],
            }), encoding="utf-8")
            wrapper_dir = root / "logs" / "2026-11-24" / "later-wrapper"
            wrapper_dir.mkdir(parents=True)
            wrapper_journal = wrapper_dir / "server-01" / "events.jsonl"
            wrapper_journal.parent.mkdir(parents=True)
            wrapper_journal.write_text("", encoding="utf-8")
            wrapper_created = (process_start + timedelta(minutes=15))
            (wrapper_dir / "run.json").write_text(json.dumps({
                "run_id": "later-wrapper",
                "created_utc": wrapper_created.isoformat().replace(
                    "+00:00", "Z"),
                "instances": [{"instance_id": "server-01", "role": "server",
                               "pid": 321, "launch_id": launch_utc,
                               "runtime_run_id": "current-run",
                               "executable": "C:\\game\\mimita.exe",
                               "journal_path": str(wrapper_journal)}],
            }), encoding="utf-8")

            sampler = BuildResourceSampler(root, "runtime-launch-id")
            sampler._psutil = object()
            sampler._processes = {321: (None, {
                "role": "mimita", "executable": "mimita.exe",
                "create_time": process_start.timestamp()})}
            sampler._refresh_runtime_context()

            context = sampler._runtime_context_by_pid[321]
            self.assertEqual(context["run_id"], "current-run")
            self.assertEqual(context["role"], "server")
            self.assertAlmostEqual(
                context["identity_run_created_delta_seconds"], 8.0, delta=0.01)
            self.assertEqual(context["identity_match_basis"], "instance_launch_id")
            self.assertAlmostEqual(
                context["identity_start_delta_seconds"], 8.0, delta=0.01)

    def test_runtime_context_rejects_stale_run_creation_fallback_for_reused_pid(
            self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / "logs" / "2026-11-24" / "old-run"
            server_dir = run_dir / "server-01"
            server_dir.mkdir(parents=True)
            journal = server_dir / "events.jsonl"
            journal.write_text("", encoding="utf-8")
            old_start = datetime.now(timezone.utc) - timedelta(hours=3)
            old_utc = old_start.isoformat().replace("+00:00", "Z")
            (run_dir / "run.json").write_text(json.dumps({
                "run_id": "old-run", "created_utc": old_utc,
                "instances": [{"instance_id": "server-01", "role": "server",
                               "pid": 321, "executable": "mimita.exe",
                               "journal_path": str(journal)}],
            }), encoding="utf-8")

            sampler = BuildResourceSampler(root, "runtime-stale-run-fallback")
            sampler._psutil = object()
            sampler._processes = {321: (None, {
                "role": "mimita", "executable": "mimita.exe",
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
                              "server_tick_window": dict(fresh_row),
                              "journal_mode": dict(fresh_row),
                              "actual_mode": "jugger_horde"}},
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
        self.assertEqual(result["pair_alignment_status"], "observed")
        self.assertEqual(result["mode_verification_status"], "matched")
        self.assertTrue(result["mode_match"])
        self.assertEqual(result["run_id"], "matched")
        self.assertEqual(result["server_pid"], 321)
        self.assertEqual(result["client_pid"], 322)
        self.assertEqual(result["actual_mode"], "jugger_horde")

        sampler._runtime_context_by_pid[321]["journal"][
            "server_tick_window"]["observed_utc"] = "2020-01-01T00:00:00Z"
        result = sampler._runtime_alignment_summary()
        self.assertFalse(result["same_run_server_client"])
        self.assertEqual(result["pair_alignment_status"], "unavailable")

    def test_runtime_alignment_rejects_same_run_mode_mismatch(self):
        sampler = BuildResourceSampler(Path("."), "runtime-mode-mismatch")
        observed = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
        fresh_row = {"observed_utc": observed}
        sampler._runtime_context_by_pid = {
            321: {"run_id": "matched", "role": "server", "pid": 321,
                  "journal": {"generation_npc_player": dict(fresh_row),
                              "server_tick_window": dict(fresh_row),
                              "journal_mode": dict(fresh_row),
                              "actual_mode": "bloodtest"}},
            322: {"run_id": "matched", "role": "client", "pid": 322,
                  "journal": {"client_frame_window": dict(fresh_row),
                              "client_npc_view_window": dict(fresh_row),
                              "journal_mode": dict(fresh_row),
                              "actual_mode": "repeat"}},
        }

        result = sampler._runtime_alignment_summary()

        self.assertTrue(result["same_run_server_client"])
        self.assertEqual(result["pair_alignment_status"], "observed")
        self.assertEqual(result["mode_verification_status"], "mode_mismatch")
        self.assertFalse(result["mode_match"])
        self.assertEqual(result["status"], "mode_mismatch")
        self.assertIsNone(result["actual_mode"])
        self.assertEqual(result["server_actual_mode"], "bloodtest")
        self.assertEqual(result["client_actual_mode"], "repeat")

    def test_runtime_alignment_keeps_same_run_when_client_mode_is_missing(self):
        sampler = BuildResourceSampler(Path("."), "runtime-mode-unverified")
        observed = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
        fresh_row = {"observed_utc": observed}
        server_hotness = {**fresh_row, "event": "game.hotness.summary",
                          "fields": {"npcs": 67, "players": 1}}
        server_tick = {**fresh_row, "event": "performance.server-tick-window",
                       "fields": {"ticks": 60, "p95_ms": 12.0}}
        client_frame = {**fresh_row, "event": "performance.fps",
                        "fields": {"frames": 240, "frames_over_budget": 1}}
        client_npcs = {**fresh_row, "event": "client.npc.viewport-window",
                       "fields": {"remote_npcs": 67, "alive": 67}}
        sampler._runtime_context_by_pid = {
            321: {"run_id": "matched", "role": "server", "pid": 321,
                  "journal": {"generation_npc_player": server_hotness,
                              "server_tick_window": server_tick,
                              "journal_mode": dict(fresh_row),
                              "actual_mode": "bloodtest"}},
            322: {"run_id": "matched", "role": "client", "pid": 322,
                  "journal": {"client_frame_window": client_frame,
                              "client_npc_view_window": client_npcs,
                              "actual_mode": None}},
        }

        result = sampler._runtime_alignment_summary()

        self.assertTrue(result["same_run_server_client"])
        self.assertEqual(result["pair_alignment_status"], "observed")
        self.assertEqual(result["mode_verification_status"],
                         "client_mode_unverified")
        self.assertFalse(result["mode_match"])
        self.assertEqual(result["status"], "mode_unverified")
        self.assertEqual(result["reason"],
                         "fresh_authoritative_mode_missing_or_stale")
        self.assertEqual(result["server_actual_mode"], "bloodtest")
        self.assertEqual(result["server_authoritative_mode"], "bloodtest")
        self.assertEqual(result["server_mode_source"], "server_journal")
        self.assertEqual(result["client_mode_status"], "stale_or_unreported")
        self.assertIsNone(result["client_actual_mode"])
        self.assertIsNone(result["actual_mode"])
        self.assertEqual(
            result["server_workload"]["generation_npc_player"]["fields"]["npcs"],
            67)
        self.assertEqual(
            result["client_workload"]["client_frame_window"]["fields"]["frames"],
            240)

    def test_resource_summary_separates_pair_evidence_from_client_mode_gap(self):
        with tempfile.TemporaryDirectory() as temporary:
            sampler = BuildResourceSampler(Path(temporary),
                                            "runtime-pair-mode-gap")
            observed = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
            fresh_row = {"observed_utc": observed}
            sampler._status = "captured"
            sampler._sample_count = 1
            sampler._runtime_context_by_pid = {
                321: {"run_id": "matched", "role": "server", "pid": 321,
                      "journal": {
                          "generation_npc_player": {
                              **fresh_row, "fields": {"npcs": 67, "players": 1}},
                          "server_tick_window": {
                              **fresh_row, "fields": {"ticks": 60}},
                          "journal_mode": dict(fresh_row),
                          "actual_mode": "bloodtest"}},
                322: {"run_id": "matched", "role": "client", "pid": 322,
                      "journal": {
                          "client_frame_window": {
                              **fresh_row, "fields": {"frames": 240}},
                          "client_npc_view_window": {
                              **fresh_row, "fields": {"remote_npcs": 67}},
                          "actual_mode": None}},
            }

            summary = sampler.stop()

        self.assertEqual(summary["runtime_alignment"]["pair_alignment_status"],
                         "observed")
        self.assertEqual(summary["runtime_alignment"]["server_authoritative_mode"],
                         "bloodtest")
        self.assertEqual(summary["runtime_alignment"]["client_mode_status"],
                         "stale_or_unreported")
        self.assertNotIn("journal_aligned_live_mode_workload_and_frame_tick_metrics",
                         summary["unavailable_fields"])
        self.assertIn("client_mode_independent_confirmation",
                      summary["unavailable_fields"])

    def test_runtime_alignment_does_not_trust_stale_mode_rows(self):
        sampler = BuildResourceSampler(Path("."), "runtime-stale-mode")
        observed = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
        fresh_row = {"observed_utc": observed}
        stale_mode = {"observed_utc": "2020-01-01T00:00:00Z",
                      "mode": "repeat"}
        sampler._runtime_context_by_pid = {
            321: {"run_id": "matched", "role": "server", "pid": 321,
                  "journal": {"generation_npc_player": dict(fresh_row),
                              "server_tick_window": dict(fresh_row),
                              "journal_mode": dict(fresh_row),
                              "actual_mode": "bloodtest"}},
            322: {"run_id": "matched", "role": "client", "pid": 322,
                  "journal": {"client_frame_window": dict(fresh_row),
                              "client_npc_view_window": dict(fresh_row),
                              "journal_mode": stale_mode,
                              "actual_mode": "repeat"}},
        }

        result = sampler._runtime_alignment_summary()

        self.assertTrue(result["same_run_server_client"])
        self.assertEqual(result["pair_alignment_status"], "observed")
        self.assertEqual(result["mode_verification_status"],
                         "client_mode_unverified")
        self.assertFalse(result["mode_match"])
        self.assertEqual(result["status"], "mode_unverified")
        self.assertEqual(result["client_mode_observed_utc"],
                         "2020-01-01T00:00:00Z")
        self.assertIsNone(result["client_actual_mode"])
        self.assertIsNone(result["actual_mode"])

    def test_runtime_journal_ignores_configuration_mode_fields(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            journal = root / "events.jsonl"
            rows = [
                {"pid": 321, "run_id": "runtime", "event": "network.mode.active",
                 "wall_time": "2026-11-24T07:30:50.684Z",
                 "fields": {"mode": "bloodtest"}},
                {"pid": 321, "run_id": "runtime",
                 "event": "dev_timer_tips.config_loaded",
                 "wall_time": "2026-11-24T07:34:10.945Z",
                 "fields": {"mode": "repeat"}},
            ]
            journal.write_text("\n".join(json.dumps(row) for row in rows) + "\n",
                               encoding="utf-8")

            sampler = BuildResourceSampler(root, "runtime-mode-source")
            result = sampler._read_runtime_journal(str(journal), 321, "runtime")

        self.assertEqual(result["actual_mode"], "bloodtest")
        self.assertEqual(result["journal_mode"]["event"], "network.mode.active")


if __name__ == "__main__":
    unittest.main()
