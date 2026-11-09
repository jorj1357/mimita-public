"""Focused tests for UTC build identities and published-artifact selection."""

from __future__ import annotations

import datetime
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from .artifacts import (
    build_artifact_id,
    folder_name_for_id,
    identity_for_folder,
    published_builds,
)
from .launch import select_launch_artifact


class PublishedArtifactTests(unittest.TestCase):
    def setUp(self) -> None:
        temp_root = Path(__file__).resolve().parents[2] / ".dev" / "tmp"
        temp_root.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(
            prefix="dev-loop-artifacts-", dir=temp_root)
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    @staticmethod
    def _executable_identity(path: Path) -> dict:
        if not path.is_file():
            return {"valid": False, "validation": "missing"}
        return {
            "valid": True,
            "validation": "ok",
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }

    def _write_artifact(self, folder_name: str, artifact_id: str,
                        *, valid_sidecar: bool = True) -> Path:
        folder = self.root / folder_name
        folder.mkdir()
        executable = folder / "mimita.exe"
        executable.write_bytes(folder_name.encode("utf-8"))
        digest = hashlib.sha256(executable.read_bytes()).hexdigest()
        if valid_sidecar:
            (folder / "build-identity.json").write_text(json.dumps({
                "artifact_id": artifact_id,
                "build_id": f"{artifact_id}:{digest}",
                "sha256": digest,
                "source_generation": 4,
            }), encoding="utf-8")
        return folder

    def test_same_time_ids_advance_by_one_microsecond_and_use_safe_folder(self) -> None:
        timestamp = datetime.datetime(2026, 11, 3, 14, 5, 27, 123456,
                                      tzinfo=datetime.timezone.utc)
        first_id, first_folder = build_artifact_id(timestamp, self.root)
        (self.root / first_folder).mkdir()
        second_id, second_folder = build_artifact_id(timestamp, self.root)

        self.assertEqual(first_id, "2026-11-03T14:05:27.123456Z")
        self.assertEqual(second_id, "2026-11-03T14:05:27.123457Z")
        self.assertEqual(first_folder, folder_name_for_id(first_id))
        self.assertNotEqual(first_folder, second_folder)

    def test_timestamp_artifacts_sort_newest_first_and_keep_legacy_folders(self) -> None:
        older_id = "2026-11-03T14:05:27.123456Z"
        newer_id = "2026-11-03T14:05:27.123457Z"
        older_folder = self._write_artifact(folder_name_for_id(older_id), older_id)
        newer_folder = self._write_artifact(folder_name_for_id(newer_id), newer_id)
        self._write_artifact("4000", "4000", valid_sidecar=False)

        ordered = published_builds(self.root)
        self.assertEqual(ordered[:2], [newer_folder, older_folder])
        self.assertIn(self.root / "4000", ordered)

    def test_sidecar_requires_matching_utc_id_hash_and_generation(self) -> None:
        artifact_id = "2026-11-03T14:05:27.123456Z"
        folder = self._write_artifact(folder_name_for_id(artifact_id), artifact_id)
        valid = identity_for_folder(folder, self._executable_identity)
        self.assertIsNotNone(valid)
        self.assertEqual(valid["artifact_id"], artifact_id)

        metadata_path = folder / "build-identity.json"
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        metadata["sha256"] = "0" * 64
        metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
        self.assertIsNone(identity_for_folder(folder, self._executable_identity))

    def test_launch_selection_falls_back_to_newest_valid_previous_artifact(self) -> None:
        older_id = "2026-11-03T14:05:27.123456Z"
        newest_id = "2026-11-03T14:05:27.123457Z"
        older_folder = self._write_artifact(folder_name_for_id(older_id), older_id)
        newest_folder = self._write_artifact(folder_name_for_id(newest_id), newest_id,
                                            valid_sidecar=False)

        selected = select_launch_artifact(newest_folder.name, self.root,
                                          self._executable_identity)
        self.assertEqual(selected["folder"], older_folder.name)
        self.assertTrue(selected["fallback"])
        self.assertEqual(selected["identity"]["artifact_id"], older_id)


if __name__ == "__main__":
    unittest.main()
