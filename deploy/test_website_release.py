from __future__ import annotations

import io
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

from deploy import website_release as wr


class AllowlistTests(unittest.TestCase):
    def test_runtime_and_content_are_allowed(self):
        self.assertTrue(wr.is_allowed("website/src/main.jsx"))
        self.assertTrue(wr.is_allowed("website/server/server.js"))
        self.assertTrue(wr.is_allowed("content/articles/welcome.md"))
        self.assertFalse(wr.is_allowed("src/npc/npc.cpp"))
        self.assertFalse(wr.is_allowed("content/other/note.md"))

    def test_secrets_and_dependencies_are_forbidden(self):
        self.assertTrue(wr.is_forbidden("website/.env"))
        self.assertTrue(wr.is_forbidden("website/.env.production"))
        self.assertTrue(wr.is_forbidden("website/node_modules/react/index.js"))
        self.assertTrue(wr.is_forbidden("website/dist/assets/index.js"))
        self.assertTrue(wr.is_forbidden("deploy/server.key"))
        self.assertFalse(wr.is_forbidden("website/src/App.jsx"))
        self.assertFalse(wr.is_forbidden("website/.env.example"))

    def test_validate_accepts_website_article_and_safeguard(self):
        result = wr.validate_paths(
            [
                "website/src/App.jsx",
                "content/articles/welcome.md",
                "deploy/nginx-mimita-site.conf",
            ]
        )
        self.assertEqual(len(result["runtime"]), 2)
        self.assertEqual(result["service_files"], ["deploy/nginx-mimita-site.conf"])

    def test_validate_rejects_migration(self):
        with self.assertRaises(wr.ReleaseError) as ctx:
            wr.validate_paths(["website/server/migrations/010_next.sql"])
        self.assertIn("migration", str(ctx.exception))

    def test_validate_rejects_game_source(self):
        with self.assertRaises(wr.ReleaseError) as ctx:
            wr.validate_paths(["src/network/server.cpp"])
        self.assertIn("allowlist", str(ctx.exception))

    def test_validate_rejects_secret(self):
        with self.assertRaises(wr.ReleaseError) as ctx:
            wr.validate_paths(["website/.env"])
        self.assertIn("forbidden", str(ctx.exception))


class ManifestTests(unittest.TestCase):
    def _archive(self, directory: Path, files: dict) -> Path:
        archive = directory / "site.tar.gz"
        with tarfile.open(archive, "w:gz") as tar:
            for name, data in files.items():
                payload = data.encode("utf-8")
                info = tarfile.TarInfo(name=name)
                info.size = len(payload)
                info.mode = 0o644
                tar.addfile(info, io.BytesIO(payload))
        return archive

    def test_round_trip(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            archive = self._archive(
                directory,
                {
                    "website/index.html": "<html></html>",
                    "content/articles/welcome.md": "# hello",
                },
            )
            manifest = wr.build_manifest(archive, "abc123", {"to": "abc123"})
            manifest_path = directory / "site.manifest.json"
            wr.write_manifest(manifest, manifest_path)
            result = wr.verify_manifest(archive, manifest_path)
            self.assertTrue(result["verified"])
            self.assertEqual(result["entries"], 2)

    def test_detects_tampered_archive(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            archive = self._archive(directory, {"website/index.html": "a"})
            manifest = wr.build_manifest(archive, "abc", {})
            manifest_path = directory / "m.json"
            wr.write_manifest(manifest, manifest_path)
            self._archive(directory, {"website/index.html": "b"})
            with self.assertRaises(wr.ReleaseError):
                wr.verify_manifest(archive, manifest_path)

    def test_rejects_member_outside_allowlist(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = self._archive(
                Path(tmp), {"src/npc/npc.cpp": "int main(){}"}
            )
            with self.assertRaises(wr.ReleaseError) as ctx:
                wr.build_manifest(archive, "abc", {})
            self.assertIn("outside allowlist", str(ctx.exception))

    def test_rejects_symlink_member(self):
        with tempfile.TemporaryDirectory() as tmp:
            archive = Path(tmp) / "site.tar.gz"
            with tarfile.open(archive, "w:gz") as tar:
                info = tarfile.TarInfo(name="website/link")
                info.type = tarfile.SYMTYPE
                info.linkname = "/etc/passwd"
                tar.addfile(info)
            with self.assertRaises(wr.ReleaseError):
                wr.build_manifest(archive, "abc", {})


class GitTests(unittest.TestCase):
    def test_changed_paths_between_commits(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = Path(tmp)
            git = ["git", "-c", "user.name=t", "-c", "user.email=t@example.com"]
            subprocess.run(["git", "init", "-q"], cwd=repo, check=True)
            (repo / "website").mkdir()
            (repo / "website/a.js").write_text("one", encoding="utf-8")
            subprocess.run(git + ["add", "."], cwd=repo, check=True)
            subprocess.run(git + ["commit", "-qm", "one"], cwd=repo, check=True)
            first = wr.require_commit(repo, "HEAD")
            (repo / "website/a.js").write_text("two", encoding="utf-8")
            (repo / "website/b.js").write_text("new", encoding="utf-8")
            subprocess.run(git + ["add", "."], cwd=repo, check=True)
            subprocess.run(git + ["commit", "-qm", "two"], cwd=repo, check=True)
            second = wr.require_commit(repo, "HEAD")
            changed = wr.changed_paths(repo, first, second)
            self.assertEqual(sorted(changed), ["website/a.js", "website/b.js"])


if __name__ == "__main__":
    unittest.main()
