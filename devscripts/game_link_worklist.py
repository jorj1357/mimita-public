#!/usr/bin/env python3
"""Turn whole-game linker errors into an owner/family worklist.

The linker is the authoritative missing-symbol list. This tool groups it so the
foundation-first bridge work is measurable instead of a wall of mangled names.
"""

from __future__ import annotations

import argparse
import collections
import csv
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UNDEFINED_RE = re.compile(r"undefined reference to `(.+?)'")


def demangle(symbols: list[str]) -> list[str]:
    if not symbols:
        return []
    candidates = [
        shutil.which("c++filt"),
        r"C:\important\msys64\mingw64\bin\c++filt.exe",
    ]
    for candidate in candidates:
        if not candidate:
            continue
        try:
            completed = subprocess.run(
                [candidate],
                input="\n".join(symbols),
                text=True,
                capture_output=True,
                check=False,
            )
            lines = completed.stdout.splitlines()
            if len(lines) == len(symbols):
                return lines
        except OSError:
            continue
    return symbols


def family(symbol: str) -> str:
    lower = symbol.lower()
    if symbol.startswith("ModuleHost"):
        return "ModuleHost policy"
    if symbol.startswith("MimitaNet"):
        return "MimitaNet (session/server/world-rebuild)"
    if symbol.startswith("IceAgent"):
        return "ICE transport"
    if symbol.startswith("DuelQueue"):
        return "duel queue"
    if symbol.startswith("HotReloadSystem"):
        return "HotReloadSystem"
    if symbol.startswith("LogManager"):
        return "LogManager"
    if symbol.startswith("Renderer"):
        return "Renderer device"
    if symbol.startswith("glad"):
        return "GLAD device"
    if re.match(r"^g[pA-Z]", symbol):
        return "process globals"
    if symbol.startswith("PersistenceQueue"):
        return "persistence"
    if symbol.startswith("ReplaySaveWorker"):
        return "replay save jobs"
    if symbol.startswith("JsonFileWatch"):
        return "json watch jobs"
    if symbol.startswith("mimita::host::HostRuntime"):
        return "direct host calls"
    if "selftest" in lower or "self_test" in lower:
        return "self-tests"
    if "crosshair" in lower:
        return "crosshair/GUI"
    if "crash" in lower:
        return "crash handler"
    if any(
        token in lower
        for token in (
            "audios",
            "playsound",
            "replaymusic",
            "audiobuffer",
            "resolvesound",
            "getaudiofile",
            "setaudiolistener",
            "audiolistener",
            "globalduck",
            "audioupdate",
            "audioownerkey",
            "logaudio",
            "airjump",
            "menuclick",
            "menuhover",
        )
    ):
        return "audio helpers"
    return "other"


def signature_class(symbol: str) -> str:
    if symbol.startswith("glad"):
        return "device-function"
    if any(
        token in symbol
        for token in (
            "std::",
            "unordered_map",
            "std::vector",
            "std::function",
            "shared_ptr",
            "unique_ptr",
        )
    ):
        return "complex-ownership"
    if "&" in symbol:
        return "reference"
    if "std::string" in symbol or "char" in symbol:
        return "string"
    if "glm::vec" in symbol:
        return "vec3"
    return "scalar"


def strategy(family_name: str, signature: str) -> str:
    if family_name in {
        "ModuleHost policy",
        "audio helpers",
        "LogManager",
        "HotReloadSystem",
        "duel queue",
        "persistence",
    }:
        return "typed-family-bridge"
    if family_name in {"GLAD device", "Renderer device"}:
        return "CAP_RENDER command stream"
    if family_name in {"ICE transport"}:
        return "keep resident; CAP_TRANSPORT signaling"
    if family_name.startswith("MimitaNet"):
        return "split: transport vs session/state"
    if family_name == "process globals":
        return "CAP_ENTITY/CAP_WORLD handle"
    if family_name in {"replay save jobs", "json watch jobs"}:
        return "CAP_JOBS lease"
    if family_name == "self-tests":
        return "exclude or test owner"
    if family_name == "direct host calls":
        return "use HostServices, not HostRuntime"
    if family_name == "crosshair/GUI":
        return "CAP_GUI"
    if family_name == "crash handler":
        return "resident bridge"
    if signature == "complex-ownership":
        return "do not bridge; redesign bounded"
    return "classify"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, default=ROOT / "build/game/progress.log")
    parser.add_argument(
        "--out",
        type=Path,
        default=ROOT / "docs/handoffs/game-dll-undefined-worklist-20261112.csv",
    )
    parser.add_argument(
        "--md",
        type=Path,
        default=ROOT / "docs/handoffs/game-dll-undefined-worklist-20261112.md",
    )
    parser.add_argument("--top", type=int, default=25)
    args = parser.parse_args()

    if not args.log.exists():
        parser.error(f"link log not found: {args.log}")
    text = args.log.read_text(encoding="utf-8", errors="replace")
    symbols = UNDEFINED_RE.findall(text)
    if not symbols:
        parser.error(f"no undefined references in {args.log}")

    demangled = demangle(symbols)
    rows = []
    for raw, symbol in zip(symbols, demangled):
        family_name = family(symbol)
        signature = signature_class(symbol)
        rows.append(
            {
                "symbol": symbol,
                "refs": 1,
                "family": family_name,
                "signature": signature,
                "strategy": strategy(family_name, signature),
                "mangled": raw,
            }
        )

    counts: dict[str, dict[str, object]] = {}
    for row in rows:
        entry = counts.setdefault(
            row["symbol"],
            {
                "symbol": row["symbol"],
                "refs": 0,
                "family": row["family"],
                "signature": row["signature"],
                "strategy": row["strategy"],
            },
        )
        entry["refs"] = int(entry["refs"]) + 1

    worklist = sorted(
        counts.values(),
        key=lambda item: (-int(item["refs"]), str(item["family"]), str(item["symbol"])),
    )
    family_counts = collections.Counter(str(item["family"]) for item in worklist)
    strategy_counts = collections.Counter(str(item["strategy"]) for item in worklist)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["symbol", "refs", "family", "signature", "strategy"],
        )
        writer.writeheader()
        writer.writerows(worklist)

    lines = [
        "# Game-DLL undefined-symbol worklist (2026-11-12)",
        "",
        "**Status:** generated from the whole-game linker log. This is a worklist,",
        "not a runtime or correctness proof.",
        "",
        f"- Link log: `{args.log.as_posix()}`",
        f"- Undefined references: **{len(rows)}**",
        f"- Unique symbols: **{len(worklist)}**",
        f"- Machine table: `{args.out.as_posix()}`",
        "",
        "## By family",
        "",
        "| Family | Unique | Refs |",
        "|---|---:|---:|",
    ]
    refs_by_family = collections.Counter()
    for row in rows:
        refs_by_family[str(row["family"])] += 1
    for name, unique in family_counts.most_common():
        lines.append(f"| {name} | {unique} | {refs_by_family[name]} |")
    lines += [
        "",
        "## By strategy",
        "",
        "| Strategy | Unique |",
        "|---|---:|",
    ]
    for name, unique in strategy_counts.most_common():
        lines.append(f"| {name} | {unique} |")
    lines += [
        "",
        f"## Top {args.top}",
        "",
        "| Refs | Family | Signature | Symbol |",
        "|---:|---|---|---|",
    ]
    for item in worklist[: args.top]:
        symbol = str(item["symbol"]).replace("|", "\\|")
        lines.append(
            f"| {item['refs']} | {item['family']} | {item['signature']} | `{symbol}` |"
        )
    lines.append("")
    args.md.parent.mkdir(parents=True, exist_ok=True)
    args.md.write_text("\n".join(lines), encoding="utf-8")

    print(f"[WORKLIST] refs={len(rows)} unique={len(worklist)}")
    for name, unique in family_counts.most_common():
        print(f"[WORKLIST] {unique:4d} unique {refs_by_family[name]:4d} refs  {name}")
    print(f"[WORKLIST] csv={args.out}")
    print(f"[WORKLIST] md={args.md}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
