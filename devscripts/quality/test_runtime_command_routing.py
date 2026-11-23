"""Focused contract checks for registered-runtime terminal command queues."""

import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from dev_loop.parallel_test_runs import append_command_queue
from dev_loop.runtimes import RuntimeManager


class RuntimeCommandQueueTests(unittest.TestCase):
    def test_runtime_command_uses_runtime_scenario_and_persisted_client(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            run_dir = root / "logs" / "run"
            run_dir.mkdir(parents=True)
            queue = run_dir / "dev-loop" / "agent-command-client-01.jsonl"
            runtime_dir = root / ".dev" / "runtimes"
            runtime_dir.mkdir(parents=True)
            state = {
                "runtime_id": "sample", "run_dir": str(run_dir),
                "command_queue": str(queue), "client_pid": 42,
                "client_journal": str(run_dir / "client-01" / "events.jsonl"),
            }
            (runtime_dir / "sample.json").write_text(json.dumps(state), encoding="utf-8")
            manager = RuntimeManager(root, lambda: None)
            with patch("dev_loop.runtimes.process_alive", return_value=True):
                result = manager.queue_command("sample", "perf_callsite_cpu on 15")
            record = json.loads(queue.read_text(encoding="utf-8").strip())
            self.assertEqual(record["scenario_id"], "runtime_command")
            self.assertEqual(record["command"], "perf_callsite_cpu on 15")
            self.assertEqual(result["client_pid"], 42)
            self.assertEqual(result["client_journal_path"], state["client_journal"])

    def test_parallel_queue_keeps_legacy_scenario_default(self):
        with tempfile.TemporaryDirectory() as temporary:
            queue = Path(temporary) / "commands.jsonl"
            append_command_queue(queue, "request-1", "status")
            record = json.loads(queue.read_text(encoding="utf-8").strip())
            self.assertEqual(record["scenario_id"], "parallel_agent_test")

    def test_empty_runtime_command_is_rejected(self):
        manager = RuntimeManager(Path("."), lambda: None)
        with self.assertRaisesRegex(RuntimeError, "command is empty"):
            manager.queue_command("missing", "  ")


if __name__ == "__main__":
    unittest.main()
