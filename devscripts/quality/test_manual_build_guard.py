"""Focused tests for agent build-request authorization."""

from __future__ import annotations

import importlib.util
import os
import subprocess
import sys
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEV_LOOP = REPO / "devscripts" / "dev-loop.py"


def load_dev_loop():
    devscripts = str(REPO / "devscripts")
    if devscripts not in sys.path:
        sys.path.insert(0, devscripts)
    spec = importlib.util.spec_from_file_location("mimita_dev_loop_main", DEV_LOOP)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load dev-loop.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class ManualBuildGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dev_loop = load_dev_loop()

    def test_override_environment_controls_manual_build(self) -> None:
        previous = os.environ.pop("MIMITA_BUILD_OVERRIDE", None)
        try:
            self.assertFalse(self.dev_loop._manual_build_override_enabled())
            os.environ["MIMITA_BUILD_OVERRIDE"] = "1"
            self.assertTrue(self.dev_loop._manual_build_override_enabled())
        finally:
            if previous is None:
                os.environ.pop("MIMITA_BUILD_OVERRIDE", None)
            else:
                os.environ["MIMITA_BUILD_OVERRIDE"] = previous

    def test_daemon_authorizes_named_agents(self) -> None:
        self.assertTrue(self.dev_loop._build_request_authorized("human"))
        self.assertTrue(self.dev_loop._build_request_authorized("dashboard"))
        self.assertTrue(self.dev_loop._build_request_authorized("agent-a"))
        self.assertTrue(self.dev_loop._build_request_authorized("ai"))
        self.assertFalse(self.dev_loop._build_request_authorized(""))

    def test_build_completion_never_auto_replaces_live_process(self) -> None:
        self.assertFalse(self.dev_loop._should_relaunch_after_build(False))
        self.assertTrue(self.dev_loop._should_relaunch_after_build(True))


if __name__ == "__main__":
    unittest.main()
