#!/usr/bin/env python3
# 2026-11-12
# purpose
# Static render-command and world-query inventories for the hot render/query
# target. Read-only: no compile, link, launch, or ownership change.

from __future__ import annotations

import argparse
import csv
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RENDER_MD = Path("docs/handoffs/cpp-render-command-inventory-20261112.md")
RENDER_CSV = Path("docs/handoffs/cpp-render-command-inventory-20261112.csv")
QUERY_MD = Path("docs/handoffs/cpp-world-query-inventory-20261112.md")
QUERY_CSV = Path("docs/handoffs/cpp-world-query-inventory-20261112.csv")

RENDER_PATTERNS = {
    "draw": re.compile(r"\bglDraw[A-Za-z]+\s*\("),
    "buffer_alloc": re.compile(r"\bgl(?:Gen|Delete)Buffers?\s*\(|\bglBuffer(?:Data|SubData)\s*\("),
    "texture_alloc": re.compile(r"\bgl(?:Gen|Delete)Textures?\s*\(|\bglTex(?:Image|SubImage|Parameter)\w*\s*\("),
    "vao_alloc": re.compile(r"\bgl(?:Gen|Delete)VertexArrays\s*\(|\bglBindVertexArray\s*\("),
    "shader": re.compile(r"\bgl(?:CreateShader|ShaderSource|CompileShader|CreateProgram|LinkProgram|UseProgram)\s*\("),
    "state": re.compile(r"\bgl(?:Enable|Disable|BlendFunc|DepthMask|DepthFunc|CullFace|Viewport|Scissor)\s*\("),
    "framebuffer": re.compile(r"\bgl(?:Gen|Delete|Bind)Framebuffers?\s*\(|\bgl(?:Gen|Bind)Renderbuffers?\s*\("),
}

QUERY_PATTERNS = {
    "ray_triangle": re.compile(r"\brayTriangle\b|\bcastWorldRay\b|\bselectWorldTriangle\b"),
    "ray_aabb": re.compile(r"\brayAabb\b|\brayBox\b|\brayAABB\b"),
    "closest_point": re.compile(r"\bclosestPointOn(?:Triangle|Segment|Aabb|Box)\b"),
    "sphere_capsule": re.compile(r"\bsweptPointSphere\b|\bcapsule\b|\bsphere\b|\bSphere\b"),
    "aabb_box": re.compile(r"\bAABB\b|\bAabb\b|\bboxOverlap\b|\boverlapBounds\b"),
    "sweep": re.compile(r"\bsweep\w*\b|\bswept\w*\b"),
    "navmesh": re.compile(r"\bnavmesh\b|\bNavMesh\b|\bnavQuery\b|\bDetour\b|\bRecast\b"),
    "broadphase": re.compile(r"\bgather\w*Triangles\b|\bbroadphase\b|\bcandidate\w*\b"),
}


def source_files() -> list[Path]:
    return sorted((ROOT / "src").rglob("*.cpp"))


def scan(patterns: dict[str, re.Pattern]) -> list[dict]:
    rows: list[dict] = []
    for path in source_files():
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(ROOT).as_posix()
        for index, line in enumerate(text.splitlines(), start=1):
            for kind, pattern in patterns.items():
                if pattern.search(line):
                    rows.append({"path": rel, "line": index, "kind": kind,
                                 "snippet": line.strip()[:160]})
    return rows


def write_render() -> None:
    rows = scan(RENDER_PATTERNS)
    csv_path = ROOT / RENDER_CSV
    with open(csv_path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["path", "line", "kind", "snippet"])
        writer.writeheader()
        writer.writerows(rows)

    by_kind = Counter(row["kind"] for row in rows)
    by_file: dict[str, Counter] = defaultdict(Counter)
    for row in rows:
        by_file[row["path"]][row["kind"]] += 1
    shader_files = sorted(path.relative_to(ROOT).as_posix()
                          for path in (ROOT / "shaders").glob("*") if path.is_file())

    lines = [
        "# Render command inventory (2026-11-12)",
        "",
        "**Status:** static scan of draw calls, GPU resource allocation, shader",
        "compile/link, state changes, and framebuffers. It is the hot-render inventory,",
        "not a runtime resource-lifetime proof.",
        "",
        f"- Sites: {len(rows)}",
        f"- Files with GPU work: {len(by_file)}",
        f"- Machine table: `{RENDER_CSV.as_posix()}`",
        "",
        "## Hot-render target",
        "",
        "`game.dll` owns draw policy and builds generic render commands. The resident",
        "renderer owns the device, context, shader program service, and GPU resource",
        "registry. A new primitive should be a command + mesh + shader asset, not a new",
        "EXE draw call. A new backend/device feature remains a cold boundary.",
        "",
        "## Sites by category",
        "",
        "| Category | Sites |",
        "|---|---:|",
    ]
    for kind, count in by_kind.most_common():
        lines.append(f"| {kind} | {count} |")
    lines.extend([
        "",
        "## Files by GPU work",
        "",
        "| File | draw | buffer | texture | vao | shader | state | framebuffer |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
    ])
    for path, counts in sorted(by_file.items(),
                               key=lambda item: (-sum(item[1].values()), item[0])):
        lines.append(
            f"| `{path}` | {counts.get('draw', 0)} | {counts.get('buffer_alloc', 0)} | "
            f"{counts.get('texture_alloc', 0)} | {counts.get('vao_alloc', 0)} | "
            f"{counts.get('shader', 0)} | {counts.get('state', 0)} | "
            f"{counts.get('framebuffer', 0)} |")
    lines.extend(["", "## Shader assets", ""])
    lines.extend(f"- `{path}`" for path in shader_files)
    lines.extend([
        "",
        "## Required before render code moves",
        "",
        "- A generic `CAP_RENDER` draw-command struct (vertices, indices, material, transform, state).",
        "- A `CAP_RESOURCES` registry for VAO/VBO/texture/framebuffer ownership and release under the current context.",
        "- A shader-asset service that compiles/validates/swaps independently of the code generation.",
        "- Per-file classification of cached GPU objects vs per-call setup; cached objects need a resident owner or generation release.",
    ])
    (ROOT / RENDER_MD).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[RENDER] sites={len(rows)} files={len(by_file)} report={RENDER_MD}")


def write_query() -> None:
    rows = scan(QUERY_PATTERNS)
    csv_path = ROOT / QUERY_CSV
    with open(csv_path, "w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=["path", "line", "kind", "snippet"])
        writer.writeheader()
        writer.writerows(rows)

    by_kind = Counter(row["kind"] for row in rows)
    by_file: dict[str, Counter] = defaultdict(Counter)
    for row in rows:
        by_file[row["path"]][row["kind"]] += 1

    lines = [
        "# World-query inventory (2026-11-12)",
        "",
        "**Status:** static scan of ray, closest-point, sphere/capsule, AABB, sweep,",
        "navmesh, and broadphase candidate queries. It is the hot world-query inventory,",
        "not a correctness or performance proof.",
        "",
        f"- Sites: {len(rows)}",
        f"- Files with query work: {len(by_file)}",
        f"- Machine table: `{QUERY_CSV.as_posix()}`",
        "",
        "## Hot world-query target",
        "",
        "The resident host owns the world geometry, broadphase, and stable entity",
        "handles. `game.dll` composes new queries from generic primitives: ray, sphere,",
        "capsule, box, closest-point, and broadphase candidates. A genuinely new query",
        "primitive is a one-time capability add; a new composition is hot.",
        "",
        "## Sites by category",
        "",
        "| Category | Sites |",
        "|---|---:|",
    ]
    for kind, count in by_kind.most_common():
        lines.append(f"| {kind} | {count} |")
    lines.extend([
        "",
        "## Files by query kind",
        "",
        "| File | ray-tri | ray-aabb | closest | sphere/capsule | aabb | sweep | navmesh | broadphase |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ])
    for path, counts in sorted(by_file.items(),
                               key=lambda item: (-sum(item[1].values()), item[0])):
        lines.append(
            f"| `{path}` | {counts.get('ray_triangle', 0)} | {counts.get('ray_aabb', 0)} | "
            f"{counts.get('closest_point', 0)} | {counts.get('sphere_capsule', 0)} | "
            f"{counts.get('aabb_box', 0)} | {counts.get('sweep', 0)} | "
            f"{counts.get('navmesh', 0)} | {counts.get('broadphase', 0)} |")
    lines.extend([
        "",
        "## Required before query code moves",
        "",
        "- A bounded `CAP_WORLD` query descriptor (shape, mask, max results) with plain results.",
        "- Read-only triangle/entity span access for compositions the host does not natively expose.",
        "- A stable-handle mapping for actors and world objects.",
        "- Per-file classification of query callers that mutate host state; those use a capability write, not a reference.",
    ])
    (ROOT / QUERY_MD).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[QUERY] sites={len(rows)} files={len(by_file)} report={QUERY_MD}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Render and world-query inventories")
    parser.add_argument("--render", action="store_true")
    parser.add_argument("--query", action="store_true")
    args = parser.parse_args(argv)
    run_render = args.render or not (args.render or args.query)
    run_query = args.query or not (args.render or args.query)
    if run_render:
        write_render()
    if run_query:
        write_query()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
