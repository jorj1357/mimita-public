#!/usr/bin/env python3
"""Report raw OpenGL use in game-owned sources that should use CAP_RENDER."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GL_CALL = re.compile(r"\bgl[A-Z][A-Za-z0-9_]*\s*\(")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Check raw GL in hot game sources")
    parser.add_argument("--json", dest="json_path",
                        default="build/hot-reload/cap-render.json")
    parser.add_argument("--report-only", action="store_true")
    parser.add_argument("--max-list", type=int, default=40)
    args = parser.parse_args(argv)

    manifest = json.loads((ROOT / "build/source-manifest.json").read_text(
        encoding="utf-8"))
    rows = [row for row in manifest.get("sources", [])
            if row.get("owner") == "game"]
    hits: list[dict] = []
    for row in rows:
        relative = str(row.get("path", "")).replace("\\", "/")
        path = ROOT / relative
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        calls = sorted(set(GL_CALL.findall(text)))
        if calls or "#include <glad/glad.h>" in text:
            hits.append({"path": relative, "calls": calls})

    payload = {
        "schema_version": 1,
        "revision": manifest.get("revision", ""),
        "game_sources": len(rows),
        "raw_gl_sources": len(hits),
        "raw_gl_paths": [hit["path"] for hit in hits],
        "clean": not hits,
    }
    output = ROOT / args.json_path
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print(f"[CAP RENDER] game_sources={len(rows)} raw_gl_sources={len(hits)}")
    for hit in hits[:max(0, args.max_list)]:
        print(f"[CAP RENDER] RAW_GL {hit['path']} calls={len(hit['calls'])}")
    if len(hits) > args.max_list:
        print(f"[CAP RENDER] ... {len(hits) - args.max_list} more")
    print(f"[CAP RENDER] {'PASS' if not hits else 'FAIL'} "
          f"report={output.relative_to(ROOT)}")
    return 0 if args.report_only or not hits else 1


if __name__ == "__main__":
    raise SystemExit(main())
