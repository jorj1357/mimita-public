"""Focused tests for per-attempt hardlink source snapshots."""

from __future__ import annotations

import json
import os
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from devscripts.dev_loop.game_build import (  # noqa: E402
    _copy_into_mirror,
    _create_junction_overlay_snapshot,
    _create_overlay_snapshot,
    _create_source_snapshot,
    _snapshot_for_revision,
    _snapshot_manifest,
    _write_snapshot_manifest,
    _prune_logs,
    _prune_snapshots,
    _SNAPSHOT_CLEANUP_QUEUE,
    _worker_metrics_summary,
)
from devscripts.dev_loop import game_build as game_build_module  # noqa: E402
from devscripts import game_build_worker as game_build_worker_module  # noqa: E402
from devscripts.game_build_worker import (  # noqa: E402
    cached_dependency_tokens,
    cached_stat,
    dependency_relative,
    identity_provider_text,
    identity_text,
    remember_dependency_tokens,
    write_identity_if_changed,
)

TEMP_ROOT = ROOT / ".dev" / "tmp"


class SourceSnapshotTests(unittest.TestCase):
    def setUp(self) -> None:
        TEMP_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(
            prefix="source-snapshot-", dir=TEMP_ROOT)
        self.root = Path(self.temporary.name) / "root"
        self.mirror = self.root / ".dev" / "build-source"
        (self.mirror / "src").mkdir(parents=True)
        (self.mirror / "src" / "a.cpp").write_text("old\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_snapshot_keeps_old_content_when_mirror_is_replaced(self) -> None:
        snapshot = _create_source_snapshot(self.root, self.mirror, "attempt-1")
        self.assertEqual((snapshot / "src" / "a.cpp").read_text(),
                         "old\n")
        source = self.root / "src"
        source.mkdir(parents=True)
        (source / "a.cpp").write_text("new\n", encoding="utf-8")
        mismatches: list[str] = []
        _copy_into_mirror(self.root, self.mirror, ["src/a.cpp"], mismatches)
        self.assertEqual(mismatches, [])
        self.assertEqual((snapshot / "src" / "a.cpp").read_text(), "old\n")
        self.assertEqual((self.mirror / "src" / "a.cpp").read_text(), "new\n")

    def test_snapshot_metadata_is_written(self) -> None:
        snapshot = _create_source_snapshot(self.root, self.mirror, "attempt-2")
        self.assertTrue((snapshot / ".snapshot.json").is_file())
        metadata = json.loads((snapshot / ".snapshot.json").read_text())
        self.assertEqual(metadata["files"], 1)
        self.assertEqual(metadata["linked"] + metadata["copied"], 1)
        self.assertEqual(metadata["directories"], 1)
        self.assertGreaterEqual(metadata["hardlink_seconds"], 0.0)
        self.assertGreaterEqual(metadata["directory_prepare_seconds"], 0.0)
        self.assertGreaterEqual(metadata["walk_and_path_seconds"], 0.0)

    def test_failed_candidate_snapshot_retains_its_manifest_revision(self) -> None:
        snapshot = _create_source_snapshot(self.root, self.mirror,
                                           "failed-candidate")
        manifest = {
            "revision": "failed-revision",
            "sources": [{"path": "src/a.cpp", "sha256": "abc", "owner": "game"}],
            "headers": [{"path": "src/a.h", "sha256": "def"}],
        }
        _write_snapshot_manifest(snapshot, manifest)

        self.assertEqual(_snapshot_for_revision(self.root, "failed-revision"),
                         snapshot)
        recovered = _snapshot_manifest(snapshot, "failed-revision")
        self.assertEqual(recovered["revision"], manifest["revision"])
        self.assertEqual(recovered["sources"], manifest["sources"])
        self.assertEqual(recovered["headers"], manifest["headers"])
        self.assertIsNone(_snapshot_for_revision(self.root, "another-revision"))

    def test_snapshot_walk_covers_nested_files_in_all_source_roots(self) -> None:
        nested = {
            "src/npc/behavior.cpp": "npc\n",
            "include/api/types.h": "header\n",
            "external/library/src/library.cpp": "vendor\n",
        }
        for relative, contents in nested.items():
            source = self.mirror / relative
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text(contents, encoding="utf-8")

        snapshot = _create_source_snapshot(self.root, self.mirror, "attempt-nested")
        for relative, contents in nested.items():
            self.assertEqual((snapshot / relative).read_text(encoding="utf-8"), contents)
        metadata = json.loads((snapshot / ".snapshot.json").read_text())
        self.assertEqual(metadata["files"], len(nested) + 1)
        self.assertGreaterEqual(metadata["directories"], 5)

    def test_preview_overlay_replaces_only_named_files(self) -> None:
        snapshot = _create_source_snapshot(self.root, self.mirror, "attempt-3")
        source = self.root / "src"
        source.mkdir(parents=True, exist_ok=True)
        (source / "a.cpp").write_text("preview\n", encoding="utf-8")
        (source / "b.cpp").write_text("new\n", encoding="utf-8")
        overlay = _create_overlay_snapshot(
            self.root, snapshot, ["src/a.cpp"], "attempt-4")
        self.assertEqual((overlay / "src" / "a.cpp").read_text(), "preview\n")
        self.assertFalse((overlay / "src" / "b.cpp").exists())
        metadata = json.loads((overlay / ".snapshot.json").read_text())
        self.assertEqual(metadata["base_snapshot"], str(snapshot.relative_to(self.root)))
        self.assertEqual(metadata["overlay_paths"], ["src/a.cpp"])

    @unittest.skipUnless(os.name == "nt", "NTFS directory junctions are Windows-only")
    def test_junction_snapshot_materializes_changes_and_shares_unchanged_dirs(self) -> None:
        base_mirror = self.root / ".dev" / "base-mirror"
        files = {
            "src/a.cpp": "old\n",
            "src/deleted.h": "remove me\n",
            "src/unchanged/common.h": "stable\n",
            "include/api.h": "api\n",
        }
        for relative, contents in files.items():
            path = base_mirror / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents, encoding="utf-8")
        base = _create_source_snapshot(self.root, base_mirror, "base-attempt")

        (self.mirror / "src" / "a.cpp").write_text("new\n", encoding="utf-8")
        added = self.mirror / "src" / "new" / "file.cpp"
        added.parent.mkdir(parents=True)
        added.write_text("added\n", encoding="utf-8")
        overlay = _create_junction_overlay_snapshot(
            self.root, base, self.mirror,
            ["src/a.cpp", "src/new/file.cpp"], ["src/deleted.h"],
            "overlay-attempt")

        self.assertEqual((overlay / "src" / "a.cpp").read_text(), "new\n")
        self.assertEqual((overlay / "src" / "new" / "file.cpp").read_text(),
                         "added\n")
        self.assertFalse((overlay / "src" / "deleted.h").exists())
        self.assertEqual((overlay / "src" / "unchanged" / "common.h").read_text(),
                         "stable\n")
        self.assertTrue(os.path.isjunction(overlay / "src" / "unchanged"))
        self.assertTrue(os.path.isjunction(overlay / "include"))
        marker = json.loads((overlay / ".snapshot.json").read_text())
        self.assertEqual(marker["strategy"], "junction_overlay")
        self.assertIn(".dev/build-snapshots/base-attempt/source/include",
                      marker["junction_targets"])

        new_unrelated = self.root / ".dev" / "build-snapshots" / "newest-attempt"
        new_unrelated.mkdir(parents=True)
        os.utime(base.parent, (1, 1))
        self.assertEqual(_prune_snapshots(
            self.root, keep=1, protect_attempts=["overlay-attempt"]), 0)
        self.assertTrue(base.parent.exists())

    @unittest.skipUnless(os.name == "nt", "NTFS directory junctions are Windows-only")
    def test_junction_snapshot_hash_mismatch_cleans_only_its_overlay(self) -> None:
        base_mirror = self.root / ".dev" / "base-mirror"
        base_file = base_mirror / "src" / "a.cpp"
        base_file.parent.mkdir(parents=True, exist_ok=True)
        base_file.write_text("old\n", encoding="utf-8")
        base = _create_source_snapshot(self.root, base_mirror, "base-mismatch")
        changed = self.mirror / "src" / "a.cpp"
        changed.write_text("new\n", encoding="utf-8")

        with self.assertRaisesRegex(RuntimeError, "changed during snapshot"):
            _create_junction_overlay_snapshot(
                self.root, base, self.mirror, ["src/a.cpp"], [],
                "overlay-mismatch", {"src/a.cpp": "wrong-digest"})

        self.assertTrue(base_file.is_file())
        self.assertEqual((base / "src" / "a.cpp").read_text(), "old\n")
        self.assertFalse((self.root / ".dev" / "build-snapshots"
                          / "overlay-mismatch").exists())

    def test_dependency_index_reuses_unchanged_depfile(self) -> None:
        dep = self.root / "unit.d"
        dep.write_text("src/a.cpp.o: src/a.cpp src/a.h\n", encoding="utf-8")
        index = {}
        self.assertTrue(remember_dependency_tokens(
            index, "src/a.cpp", "source-hash", dep,
            ["src/a.cpp", "src/a.h"]))
        self.assertEqual(len(index), 1)
        self.assertEqual(cached_dependency_tokens(
            index, "src/a.cpp", "source-hash", dep),
            [str(ROOT / "src/a.cpp"), str(ROOT / "src/a.h")])
        self.assertIsNone(cached_dependency_tokens(
            index, "src/a.cpp", "different-hash", dep))

    def test_dependency_freshness_uses_frozen_project_inputs_then_live_fallback(self) -> None:
        source_root = self.root / ".dev" / "build-snapshots" / "attempt" / "source"
        source = self.root / "src" / "game.cpp"
        header = self.root / "include" / "game.h"
        for path in (source, header,
                     source_root / "src" / "game.cpp",
                     source_root / "include" / "game.h"):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(path.as_posix(), encoding="utf-8")

        # Simulate edits made after the build snapshot was frozen.
        os.utime(source, (200, 200))
        os.utime(header, (200, 200))
        os.utime(source_root / "src" / "game.cpp", (100, 100))
        os.utime(source_root / "include" / "game.h", (100, 100))

        with mock.patch.object(game_build_worker_module, "ROOT", self.root), \
                mock.patch.object(game_build_worker_module, "SOURCE_ROOT", source_root):
            cache = {}
            self.assertEqual(cached_stat(str(source), cache).st_mtime, 100)
            self.assertEqual(cached_stat("include/game.h", cache).st_mtime, 100)

            # A pruned snapshot keeps the long-lived dependency index usable.
            (source_root / "src" / "game.cpp").unlink()
            self.assertEqual(cached_stat("src/game.cpp", {}).st_mtime, 200)

    def test_log_prune_keeps_newest_date_directories(self) -> None:
        logs = self.root / "logs"
        for name in ("2026-01-01", "2026-01-02", "2026-01-03"):
            (logs / name).mkdir(parents=True)
        self.assertEqual(_prune_logs(self.root, 2), 1)
        self.assertFalse((logs / "2026-01-01").exists())
        self.assertTrue((logs / "2026-01-03").exists())

    def test_old_snapshot_is_renamed_then_deleted_in_background(self) -> None:
        snapshots = self.root / ".dev" / "build-snapshots"
        old = snapshots / "attempt-old"
        current = snapshots / "attempt-current"
        (old / "src").mkdir(parents=True)
        (old / "src" / "many-files.cpp").write_text("old\n", encoding="utf-8")
        (current / "src").mkdir(parents=True)
        (current / "src" / "main.cpp").write_text("current\n", encoding="utf-8")
        os.utime(old, (1, 1))
        timing_path = self.root / ".dev" / "build-timings" / "prune-test.jsonl"

        deferred = _prune_snapshots(
            self.root, keep=1, attempt_id="prune-test", timing_path=timing_path)

        self.assertEqual(deferred, 1)
        self.assertFalse(old.exists())
        self.assertTrue(current.exists())
        _SNAPSHOT_CLEANUP_QUEUE.join()
        self.assertEqual(list((self.root / ".dev" / "build-snapshot-trash").iterdir()), [])
        events = [json.loads(line) for line in timing_path.read_text().splitlines()]
        self.assertEqual(events[-1]["event"], "snapshot.cleanup.completed")
        self.assertEqual(events[-1]["snapshot"], "attempt-old")

    def test_snapshot_rename_failure_does_not_fail_candidate_prune(self) -> None:
        snapshots = self.root / ".dev" / "build-snapshots"
        old = snapshots / "attempt-old"
        current = snapshots / "attempt-current"
        old.mkdir(parents=True)
        current.mkdir(parents=True)
        os.utime(old, (1, 1))

        with mock.patch.object(game_build_module.os, "replace",
                               side_effect=PermissionError("simulated busy snapshot")):
            deferred = _prune_snapshots(self.root, keep=1, attempt_id="prune-fail")

        self.assertEqual(deferred, 0)
        self.assertTrue(old.exists())
        self.assertTrue(current.exists())

    def test_cleanup_exception_is_logged_and_worker_keeps_running(self) -> None:
        trash = self.root / ".dev" / "build-snapshot-trash"
        failed = trash / "snapshot-fails"
        succeeding = trash / "snapshot-succeeds"
        failed.mkdir(parents=True)
        succeeding.mkdir()
        timing_path = self.root / ".dev" / "build-timings" / "cleanup-error.jsonl"

        with mock.patch.object(game_build_module.shutil, "rmtree",
                               side_effect=RuntimeError("simulated cleanup error")):
            self.assertTrue(game_build_module._queue_snapshot_cleanup(
                failed, "snapshot-fails", "cleanup-test", timing_path))
            _SNAPSHOT_CLEANUP_QUEUE.join()

        self.assertTrue(game_build_module._SNAPSHOT_CLEANUP_THREAD.is_alive())
        self.assertTrue(game_build_module._queue_snapshot_cleanup(
            succeeding, "snapshot-succeeds", "cleanup-test", timing_path))
        _SNAPSHOT_CLEANUP_QUEUE.join()
        self.assertFalse(succeeding.exists())
        events = [json.loads(line) for line in timing_path.read_text().splitlines()]
        self.assertEqual(events[0]["event"], "snapshot.cleanup.failed")
        self.assertIn("simulated cleanup error", events[0]["error"])
        self.assertEqual(events[1]["event"], "snapshot.cleanup.completed")

    def test_vendor_headers_are_not_project_fanout(self) -> None:
        self.assertEqual(dependency_relative("include/glm/glm.hpp"), "")
        self.assertEqual(dependency_relative("include/GLFW/glfw3.h"), "")
        self.assertEqual(dependency_relative("src/map/map_common.h"),
                         "src/map/map_common.h")

    def test_snapshot_header_fanout_path_is_lexical_and_contained(self) -> None:
        snapshot = ROOT / ".dev" / "tmp" / "path-test" / "source"
        with mock.patch.object(game_build_worker_module, "SOURCE_ROOT", snapshot):
            self.assertEqual(dependency_relative(
                str(snapshot / "src" / "npc" / "npc.h")),
                "src/npc/npc.h")
            self.assertEqual(dependency_relative(
                str(ROOT.parent / "outside" / "src" / "external.h")), "")
            self.assertEqual(dependency_relative(
                "../outside/src/external.h"), "")


class ModuleIdentityTests(unittest.TestCase):
    def test_wrapper_identity_header_is_stable_across_generations(self) -> None:
        header = identity_text()
        self.assertIn("extern const std::uint32_t generation;", header)
        self.assertIn("extern const char source_hash[];", header)
        self.assertNotIn("#define MIMITA_MODULE_GENERATION 447", header)
        self.assertEqual(identity_text(), header)

    def test_provider_carries_exact_generation_and_revision(self) -> None:
        revision = "a" * 64
        source = identity_provider_text(447, revision)
        self.assertIn("generation = 447u;", source)
        self.assertIn(f'source_hash[] = "{revision}";', source)
        self.assertIn("state_schema = 1u;", source)

    def test_stable_header_is_not_rewritten_for_a_new_generation(self) -> None:
        with tempfile.TemporaryDirectory(
                prefix="module-identity-", dir=TEMP_ROOT) as directory:
            header = Path(directory) / "module-identity.h"
            self.assertTrue(write_identity_if_changed(header))
            first_mtime = header.stat().st_mtime_ns
            self.assertFalse(write_identity_if_changed(header))
            self.assertEqual(header.stat().st_mtime_ns, first_mtime)


class BuildAttemptMetricsTests(unittest.TestCase):
    def test_failed_attempt_does_not_borrow_previous_generation_metrics(self) -> None:
        current = {
            "source_count": 642,
            "compiled": 15,
            "skipped": 627,
            "jobs": 10,
            "object_scan_seconds": 4.8,
            "changed_source_preflight_seconds": 122.621,
            "compile_seconds": 124.155,
            "link_seconds": 26.003,
            "publish_seconds": 0.5,
            "total_seconds": 214.979,
            "worker_total_seconds": 158.285,
            "slowest_compiles": [{"path": "stale-generation.cpp"}],
        }
        progress = {
            "build_attempt_id": "failed-attempt",
            "scan_total": 642,
            "compiled": 2,
            "skipped": 641,
            "jobs": 10,
            "object_scan_s": 5.342,
            "changed_source_preflight_s": 39.785,
            "compile_s": 39.785,
            "link_s": 0.0,
            "publish_s": 0.0,
            "total_s": 79.503,
            "worker_total_s": 61.25,
            "slowest": [{"path": "src/npc/npc-navigator.cpp"}],
        }

        metrics = _worker_metrics_summary(
            current, progress, {"build_attempt_id": "failed-attempt"},
            "failed-attempt", no_op=False, success=False)

        self.assertEqual(metrics["total_seconds"], 79.503)
        self.assertEqual(metrics["worker_total_seconds"], 61.25)
        self.assertEqual(metrics["changed_source_preflight_seconds"], 39.785)
        self.assertEqual(metrics["compile_seconds"], 39.785)
        self.assertEqual(metrics["link_seconds"], 0.0)
        self.assertEqual(metrics["slowest_compiles"][0]["path"],
                         "src/npc/npc-navigator.cpp")

    def test_failed_attempt_without_matching_progress_reports_unknown_metrics(self) -> None:
        metrics = _worker_metrics_summary(
            {"total_seconds": 200.0, "worker_total_seconds": 150.0},
            {"build_attempt_id": "some-other-attempt"},
            {"build_attempt_id": "failed-attempt"},
            "failed-attempt", no_op=False, success=False)

        self.assertIsNone(metrics["total_seconds"])
        self.assertIsNone(metrics["worker_total_seconds"])
        self.assertEqual(metrics["slowest_compiles"], [])


if __name__ == "__main__":
    unittest.main()
