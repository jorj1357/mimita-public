from __future__ import annotations

import json
import os
import sys
import tempfile
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts import source_manifest as manifest  # noqa: E402


class PersistentSourceCacheTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="source-manifest-cache-")
        self.root = Path(self.temp.name)
        (self.root / "src").mkdir()
        manifest._INVENTORY_CACHE.pop((self.root, "cpp"), None)
        manifest._INVENTORY_WATCHER_CURSORS.pop((self.root, "cpp"), None)
        (self.root / ".dev" / "source-inventory-cpp-cache.json").unlink(
            missing_ok=True)

    def tearDown(self) -> None:
        self.temp.cleanup()

    def test_cpp_inventory_survives_worker_memory_reset_and_invalidates_on_add(self) -> None:
        source = self.root / "src" / "game.cpp"
        source.write_text("int game;\n", encoding="utf-8")
        first, _first_stats = manifest._cached_inventory(self.root, "cpp")
        self.assertEqual(first, ["src/game.cpp"])

        manifest._INVENTORY_CACHE.pop((self.root, "cpp"), None)
        second, _second_stats = manifest._cached_inventory(self.root, "cpp")
        self.assertEqual(second, first)

        inventory_cache = self.root / ".dev" / "source-inventory-cpp-cache.json"
        inventory_cache.write_text("[]", encoding="utf-8")
        manifest._INVENTORY_CACHE.pop((self.root, "cpp"), None)
        recovered, _recovered_stats = manifest._cached_inventory(self.root, "cpp")
        self.assertEqual(recovered, first)

        added = self.root / "src" / "added.cpp"
        added.write_text("int added;\n", encoding="utf-8")
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            third, _ = manifest._cached_inventory(self.root, "cpp")
            if third == ["src/added.cpp", "src/game.cpp"]:
                break
            time.sleep(0.02)
        self.assertEqual(third, ["src/added.cpp", "src/game.cpp"])

    def test_owner_resolution_cache_survives_worker_memory_reset_and_invalidates(self) -> None:
        config = self.root / "config"
        (config / "modules").mkdir(parents=True)
        source = self.root / "src" / "game.cpp"
        source.write_text("int game;\n", encoding="utf-8")
        owner_path = config / "source-owners.json"
        owner_path.write_text(json.dumps({
            "rules": [{"owner": "game", "reason": "test",
                       "include": ["src/game.cpp"]}],
            "overrides": [], "link_inputs": {},
        }), encoding="utf-8")
        (config / "modules" / "modules.json").write_text(
            json.dumps({"version": 2, "modules": []}), encoding="utf-8")
        cpp_paths = ["src/game.cpp"]

        first, first_hit = manifest._cached_owner_resolver(self.root, cpp_paths)
        self.assertFalse(first_hit)
        self.assertEqual(first.resolve(cpp_paths[0])[0], "game")

        manifest._RESOLVER_CACHE.pop(self.root, None)
        second, second_hit = manifest._cached_owner_resolver(self.root, cpp_paths)
        self.assertTrue(second_hit)
        self.assertEqual(second.resolve(cpp_paths[0]), first.resolve(cpp_paths[0]))

        resolver_cache = self.root / ".dev" / "source-owner-resolver-cache.json"
        resolver_cache.write_text("[]", encoding="utf-8")
        manifest._RESOLVER_CACHE.pop(self.root, None)
        recovered, recovered_hit = manifest._cached_owner_resolver(self.root, cpp_paths)
        self.assertFalse(recovered_hit)
        self.assertEqual(recovered.resolve(cpp_paths[0]), first.resolve(cpp_paths[0]))

        owner_path.write_text(json.dumps({
            "rules": [{"owner": "resident", "reason": "changed",
                       "include": ["src/game.cpp"]}],
            "overrides": [], "link_inputs": {},
        }), encoding="utf-8")
        third, third_hit = manifest._cached_owner_resolver(self.root, cpp_paths)
        self.assertFalse(third_hit)
        self.assertEqual(third.resolve(cpp_paths[0])[0], "resident")

    def test_digest_cache_skips_warm_stats_and_rehashes_same_stamp_edit(self) -> None:
        source = self.root / "src" / "game.cpp"
        header = self.root / "src" / "game.h"
        source.write_text("int a;\n", encoding="utf-8")
        header.write_text("int game();\n", encoding="utf-8")
        config = self.root / "config"
        config.mkdir()
        (config / "source-owners.json").write_text(json.dumps({
            "rules": [{"owner": "game", "reason": "test",
                       "include": ["src/**/*.cpp"]}],
            "overrides": [], "link_inputs": {},
        }), encoding="utf-8")
        (config / "modules").mkdir()
        (config / "modules" / "modules.json").write_text(
            json.dumps({"version": 2, "modules": []}), encoding="utf-8")

        resolver = manifest.OwnerResolver(self.root)
        hash_cache: dict[str, dict] = {}

        def snapshot():
            events: list[dict] = []
            result = manifest.build_manifest(
                self.root, resolver=resolver, hash_cache=hash_cache,
                progress_callback=events.append)
            stages = {event.get("subphase"): event for event in events
                      if event.get("state") == "completed"}
            return result, stages

        first, first_stages = snapshot()
        first_stats = first_stages["source_ownership_and_digest"]
        self.assertEqual(first_stats["files_hashed"], 1)
        self.assertEqual(first_stages["header_digest"][
            "files_hashed"], 2)
        first_revision = first["revision"]
        self.assertEqual(len(hash_cache), 2)

        second, second_stages = snapshot()
        warm_stats = second_stages["source_ownership_and_digest"]
        self.assertEqual(second["revision"], first_revision)
        self.assertEqual(warm_stats["event_cache_hits"], 1)
        self.assertEqual(warm_stats["stat_ms"], 0.0)
        self.assertEqual(second_stages["header_digest"]["event_cache_hits"], 2)
        self.assertEqual(second_stages["header_digest"]["files_hashed"], 0)

        # A daemon restart loses the in-memory event cursor. Validate stamps
        # once, but reuse hashes when size and write time still match.
        manifest._HASH_WATCHER_CURSORS.pop(self.root, None)
        restarted, restarted_stages = snapshot()
        baseline_stats = restarted_stages["source_ownership_and_digest"]
        self.assertEqual(restarted["revision"], first_revision)
        self.assertEqual(baseline_stats["baseline_stat_cache_hits"], 1)
        self.assertEqual(restarted_stages["header_digest"][
            "baseline_stat_cache_hits"], 2)
        self.assertEqual(restarted_stages["header_digest"]["files_hashed"], 0)

        previous = source.stat()
        source.write_text("int b;\n", encoding="utf-8")
        os.utime(source, ns=(previous.st_atime_ns, previous.st_mtime_ns))
        watcher = manifest._directory_watcher(self.root)
        self.assertIsNotNone(watcher)
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            pending = watcher.events_since(
                manifest._HASH_WATCHER_CURSORS[self.root])
            if (pending is not None and "src/game.cpp" in
                    manifest._watcher_changed_source_files(pending[1])):
                break
            time.sleep(0.02)
        else:
            self.fail("watcher did not report same-stamp source edit")

        third, third_stages = snapshot()
        changed_stats = third_stages["source_ownership_and_digest"]
        self.assertNotEqual(third["revision"], first_revision)
        self.assertEqual(changed_stats["event_forced_rehashes"], 1)
        self.assertEqual(changed_stats["files_hashed"], 1)

        # An overflow destroys event completeness; the next attempt validates
        # every cached stamp before trusting the cursor again.
        with watcher._lock:
            watcher._sequence += 1
            watcher._overflow_sequence = watcher._sequence
        overflow, overflow_stages = snapshot()
        overflow_stats = overflow_stages["source_ownership_and_digest"]
        self.assertEqual(overflow["revision"], third["revision"])
        self.assertEqual(overflow_stats["baseline_stat_cache_hits"], 1)
        self.assertEqual(overflow_stages["header_digest"][
            "baseline_stat_cache_hits"], 2)
        self.assertEqual(overflow_stages["header_digest"]["files_hashed"], 0)


if __name__ == "__main__":
    unittest.main()
