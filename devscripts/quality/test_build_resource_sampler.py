from __future__ import annotations

import json
import tempfile
import time
import unittest
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

            self.assertEqual(summary["status"], "captured")
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
                        "devscripts.dev_loop.build_resource_sampler.importlib.import_module",
                        side_effect=ImportError("psutil unavailable")):
                    sampler.start()
                    summary = sampler.stop()

            self.assertEqual(summary["status"], "captured")
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


if __name__ == "__main__":
    unittest.main()
