"""Focused tests for agent-wake prompt delivery."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))


def load_agent_wake():
    spec = importlib.util.spec_from_file_location(
        "mimita_agent_wake", ROOT / "devscripts" / "agent-wake.py")
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


class WakePromptTests(unittest.TestCase):
    def setUp(self) -> None:
        self.module = load_agent_wake()

    def request(self, **overrides) -> dict:
        request = {
            "id": "wake-0123456789ab",
            "agent": "build-wait-reviewer",
            "issue": "issue-0123456789ab",
            "log_path": str(ROOT / ".dev" / "agent-wake" / "logs" / "x.log"),
            "prompt": "Review build/test wait issue issue-0123456789ab.",
        }
        request.update(overrides)
        return request

    def test_first_line_names_wake_agent_and_issue(self) -> None:
        request = self.request()
        first = self.module._prompt(request).splitlines()[0]
        self.assertIn("wake-0123456789ab", first)
        self.assertIn("build-wait-reviewer", first)
        self.assertIn("issue-0123456789ab", first)
        self.assertIn("task:", first)
        self.assertIn("x.prompt.txt", first)
        self.assertNotIn("bounded test activation", first)

    def test_first_line_recovers_context_when_issue_is_empty(self) -> None:
        request = self.request(
            issue="",
            prompt="New high-priority user context ctx-c6e9ba0bd914.\n\n"
                   "Acknowledge it after routing or repairing ownership.",
        )
        first = self.module._prompt(request).splitlines()[0]
        self.assertIn("context ctx-c6e9ba0bd914", first)
        self.assertIn("task: New high-priority user context", first)

    def test_full_prompt_still_contains_requested_work(self) -> None:
        request = self.request(
            prompt="X" * 400 + "\nRequested work marker")
        prompt = self.module._prompt(request)
        self.assertIn("Wake request: wake-0123456789ab", prompt)
        self.assertIn("Requested work:\n" + request["prompt"], prompt)


if __name__ == "__main__":
    unittest.main()
