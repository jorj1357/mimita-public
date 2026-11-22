from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.packet_registry_codegen import scan_source_references  # noqa: E402


class PacketReferenceCacheTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory(prefix="packet-ref-cache-")
        self.root = Path(self.temp.name)
        self.cache = self.root / ".dev" / "cache.json"
        self.packets = [{"name": "PACKET_ALPHA", "id": 1}]

    def tearDown(self) -> None:
        self.temp.cleanup()

    def add_source(self, path: str, owner: str, text: str) -> dict:
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")
        return {"path": path, "owner": owner}

    def scan(self, entries: list[dict], metrics: dict | None = None) -> dict:
        owners = {row["path"]: row["owner"] for row in entries}
        return scan_source_references(
            self.packets, owners, entries, self.cache, metrics, self.root)

    def test_cold_scan_then_warm_cache_avoids_source_reads(self) -> None:
        entry = self.add_source("src/game.cpp", "game", "PACKET_ALPHA\n")
        cold_metrics: dict = {}
        cold = self.scan([entry], cold_metrics)
        warm_metrics: dict = {}
        warm = self.scan([entry], warm_metrics)

        self.assertEqual(cold, warm)
        self.assertEqual(cold["PACKET_ALPHA"]["source"], "src/game.cpp")
        self.assertEqual(cold["PACKET_ALPHA"]["line"], 1)
        self.assertEqual(cold_metrics["cache_misses"], 1)
        self.assertEqual(warm_metrics["cache_hits"], 1)
        self.assertEqual(warm_metrics["files_read"], 0)

    def test_malformed_optional_cache_falls_back_to_source_scan(self) -> None:
        entry = self.add_source("src/game.cpp", "game", "PACKET_ALPHA\n")
        self.cache.parent.mkdir(parents=True)
        self.cache.write_text("[]", encoding="utf-8")
        metrics: dict = {}

        references = self.scan([entry], metrics)

        self.assertIn("PACKET_ALPHA", references)
        self.assertEqual(metrics["files_read"], 1)

    def test_changed_file_invalidates_and_new_packet_name_reuses_raw_tokens(self) -> None:
        entry = self.add_source("src/game.cpp", "game", "PACKET_ALPHA_MORE\n")
        first = self.scan([entry])
        self.assertIn("PACKET_ALPHA", first)

        self.packets.append({"name": "PACKET_ALPHA_MORE", "id": 2})
        warm_metrics: dict = {}
        warm = self.scan([entry], warm_metrics)
        self.assertIn("PACKET_ALPHA_MORE", warm)
        self.assertEqual(warm_metrics["files_read"], 0)

        source = self.root / "src/game.cpp"
        source.write_text("PACKET_ALPHA\n", encoding="utf-8")
        changed_metrics: dict = {}
        changed = self.scan([entry], changed_metrics)
        self.assertNotIn("PACKET_ALPHA_MORE", changed)
        self.assertEqual(changed_metrics["cache_misses"], 1)

    def test_game_owner_precedes_resident_and_deleted_sources_drop_out(self) -> None:
        resident = self.add_source("src/a-resident.cpp", "resident", "PACKET_ALPHA\n")
        game = self.add_source("src/z-game.cpp", "game", "PACKET_ALPHA\n")
        references = self.scan([resident, game])
        self.assertEqual(references["PACKET_ALPHA"]["owner"], "game")

        references = self.scan([resident])
        self.assertEqual(references["PACKET_ALPHA"]["owner"], "resident")
        self.assertEqual(references["PACKET_ALPHA"]["source"], resident["path"])


if __name__ == "__main__":
    unittest.main()
