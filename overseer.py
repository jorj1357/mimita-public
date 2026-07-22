#!/usr/bin/env python3
"""
overseer.py — deterministic repository drift scanner for MiMITA.

This script does not load OpenCode skills or AI-authored checker plugins.
It directly scans the repository for concrete architecture and code-quality signals.

Default behavior:
    - scans the whole repository
    - prints exact file:line findings
    - writes logs/overseer-latest.txt
    - exits 0 even when findings exist

Strict behavior:
    python overseer.py --strict
    - exits 1 when ERROR findings exist

Useful commands:
    python overseer.py
    python overseer.py --strict
    python overseer.py --json
    python overseer.py --include-third-party
    python overseer.py --check printf
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import os
import re
import sys
from collections import Counter, defaultdict
from dataclasses import asdict, dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable, Iterator, Sequence

REPO_ROOT = Path(__file__).resolve().parent
LOG_DIR = REPO_ROOT / "logs"

SOURCE_EXTENSIONS = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
    ".py", ".cs", ".java", ".js", ".jsx", ".ts", ".tsx",
}

DEFAULT_IGNORED_DIRS = {
    ".git", ".idea", ".vs", ".vscode", "__pycache__",
    "build", "dist", "out", "node_modules",
    "third_party", "third-party", "vendor", "external", "extern",
    "generated", "deps", "dependencies",
}

HEADER_REQUIRED_EXTENSIONS = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
}

# File size is informational only.
LARGE_FILE_WARNING = 700
VERY_LARGE_FILE_WARNING = 1000

# Function sizing is heuristic, not a parser-level guarantee.
LONG_FUNCTION_WARNING = 100
VERY_LONG_FUNCTION_WARNING = 180
HIGH_PARAMETER_WARNING = 10

# Similarity is deliberately conservative to reduce noise.
SIMILAR_BODY_THRESHOLD = 0.88
MIN_FUNCTION_BODY_LINES_FOR_DUPLICATION = 8
MIN_CALLS_FOR_CALLSET_WARNING = 4

RAW_OUTPUT_PATTERNS = [
    ("printf", re.compile(r"(?<![\w:])printf\s*\(")),
    ("fprintf", re.compile(r"(?<![\w:])fprintf\s*\(")),
    ("puts", re.compile(r"(?<![\w:])puts\s*\(")),
    ("std::cout", re.compile(r"\bstd::cout\b")),
    ("std::cerr", re.compile(r"\bstd::cerr\b")),
    ("OutputDebugString", re.compile(r"\bOutputDebugString(?:A|W)?\s*\(")),
]

SUSPICIOUS_NAME_WORDS = {
    "intent", "wanted", "should", "aboutto", "maybe",
    "temporary", "temp", "legacy", "old", "new", "final", "final2",
}

SUSPICIOUS_FILENAME_PARTS = {
    "copy", "backup", "bak", "old", "new", "final", "final2", "temp", "tmp",
}

REPO_CLEANLINESS_PATTERNS = [
    ("merge conflict marker", re.compile(r"^(<<<<<<<|=======|>>>>>>>)", re.MULTILINE)),
    ("disabled code block", re.compile(r"^\s*#if\s+0\b", re.MULTILINE)),
    ("empty catch block", re.compile(r"catch\s*\([^)]*\)\s*\{\s*\}", re.MULTILINE | re.DOTALL)),
    ("broad catch", re.compile(r"catch\s*\(\s*\.\.\.\s*\)")),
    ("TODO", re.compile(r"\bTODO\b")),
    ("FIXME", re.compile(r"\bFIXME\b")),
    ("HACK", re.compile(r"\bHACK\b")),
]

DUPLICATE_DEFAULT_PATTERNS = {
    "simulation rate": re.compile(
        r"\b(?:SIMULATION_HZ|SERVER_TICK_RATE|MOVEMENT_SIMULATION_HZ|GAMEPLAY_SIMULATION_HZ|kSimulationHz)\b"
        r"\s*(?:=|:)\s*([0-9]+(?:\.[0-9]+)?)"
    ),
    "reserve ammo fallback": re.compile(
        r"\breserveAmmo\b[^\n]{0,100}?\?\s*[^:\n]+:\s*([0-9]+)"
    ),
}

CONTROL_WORDS = {
    "if", "for", "while", "switch", "catch", "return", "sizeof",
    "alignof", "decltype", "static_cast", "dynamic_cast",
    "reinterpret_cast", "const_cast",
}

CALL_PATTERN = re.compile(r"\b([A-Za-z_]\w*(?:::\w+)*)\s*\(")
IDENTIFIER_PATTERN = re.compile(r"\b[A-Za-z_]\w*\b")


@dataclass(frozen=True)
class Finding:
    severity: str
    check: str
    path: str
    line: int
    message: str
    evidence: str = ""


@dataclass
class FunctionInfo:
    path: Path
    name: str
    start_line: int
    end_line: int
    signature: str
    body: str
    normalized_body: str
    body_hash: str
    calls: tuple[str, ...]
    parameter_count: int

    @property
    def line_count(self) -> int:
        return self.end_line - self.start_line + 1


def relative(path: Path) -> str:
    try:
        return path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def iter_source_files(include_third_party: bool) -> Iterator[Path]:
    ignored = set(DEFAULT_IGNORED_DIRS)
    if include_third_party:
        ignored -= {"third_party", "third-party", "vendor", "external", "extern", "deps", "dependencies"}

    for root, dirs, files in os.walk(REPO_ROOT):
        dirs[:] = [d for d in dirs if d not in ignored]
        root_path = Path(root)

        for name in files:
            path = root_path / name
            if path.resolve() == Path(__file__).resolve():
                yield path
                continue
            if path.suffix.lower() in SOURCE_EXTENSIONS:
                yield path


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8-sig", errors="replace")
    except OSError:
        return ""


def line_number(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def add_finding(
    findings: list[Finding],
    severity: str,
    check: str,
    path: Path,
    line: int,
    message: str,
    evidence: str = "",
) -> None:
    findings.append(
        Finding(
            severity=severity,
            check=check,
            path=relative(path),
            line=max(1, line),
            message=message,
            evidence=evidence.strip(),
        )
    )


def check_raw_output(path: Path, text: str, findings: list[Finding]) -> None:
    lines = text.splitlines()
    for index, raw_line in enumerate(lines, start=1):
        stripped = raw_line.strip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        for label, pattern in RAW_OUTPUT_PATTERNS:
            if pattern.search(raw_line):
                add_finding(
                    findings,
                    "ERROR",
                    "printf",
                    path,
                    index,
                    f"Raw output via {label}; use centralized Debug::log / Debug::warn / Debug::logThrottled.",
                    raw_line,
                )


def check_file_size(path: Path, text: str, findings: list[Finding]) -> None:
    count = len(text.splitlines())
    if count >= VERY_LARGE_FILE_WARNING:
        add_finding(
            findings,
            "WARNING",
            "file-size",
            path,
            1,
            f"Very large file: {count} lines. Review whether it owns multiple responsibilities.",
        )
    elif count >= LARGE_FILE_WARNING:
        add_finding(
            findings,
            "INFO",
            "file-size",
            path,
            1,
            f"Large file: {count} lines. Consider splitting only when ownership becomes unclear.",
        )


def check_header(path: Path, text: str, findings: list[Finding]) -> None:
    if path.suffix.lower() not in HEADER_REQUIRED_EXTENSIONS:
        return

    first = "\n".join(text.splitlines()[:14])
    date_ok = re.search(r"^\s*//\s*\d{1,2}\s+\d{1,2}\s+\d{4}\s*,\s*\d{1,2}\s+\d{2}", first, re.MULTILINE)
    purpose_ok = "/* purpose" in first.lower()
    does_not_ok = "does not" in first.lower()
    placeholder = re.search(r"\bfill in\b", first, re.IGNORECASE)

    if not date_ok:
        add_finding(findings, "WARNING", "header", path, 1, "Missing required dated file header.")
    if not purpose_ok:
        add_finding(findings, "WARNING", "header", path, 1, "Missing required /* purpose header.")
    if not does_not_ok:
        add_finding(findings, "WARNING", "header", path, 1, "Purpose header does not explain what the file does NOT do.")
    if placeholder:
        add_finding(
            findings,
            "WARNING",
            "header",
            path,
            line_number(first, placeholder.start()),
            "Purpose header still contains placeholder text.",
            placeholder.group(0),
        )


def strip_comments_and_strings(code: str) -> str:
    code = re.sub(r'R"\w*\(.*?\)\w*"', '""', code, flags=re.DOTALL)
    code = re.sub(r'"(?:\\.|[^"\\])*"', '""', code)
    code = re.sub(r"'(?:\\.|[^'\\])*'", "''", code)
    code = re.sub(r"//.*", "", code)
    code = re.sub(r"/\*.*?\*/", "", code, flags=re.DOTALL)
    return code


def normalize_cpp_body(body: str) -> str:
    clean = strip_comments_and_strings(body)
    tokens = re.findall(
        r"[A-Za-z_]\w*|==|!=|<=|>=|&&|\|\||->|::|\+\+|--|[-+*/%<>{}()[\],.;:=!?&|]",
        clean,
    )
    return " ".join(tokens)


def count_parameters(signature: str) -> int:
    open_paren = signature.find("(")
    close_paren = signature.rfind(")")
    if open_paren < 0 or close_paren <= open_paren:
        return 0
    inside = signature[open_paren + 1:close_paren].strip()
    if not inside or inside == "void":
        return 0

    depth = 0
    count = 1
    for char in inside:
        if char in "(<[{":
            depth += 1
        elif char in ")>]}":
            depth = max(0, depth - 1)
        elif char == "," and depth == 0:
            count += 1
    return count


def extract_cpp_functions(path: Path, text: str) -> list[FunctionInfo]:
    if path.suffix.lower() not in {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}:
        return []

    functions: list[FunctionInfo] = []
    lines = text.splitlines()
    brace_depth = 0
    candidate_start = 0
    signature_lines: list[str] = []
    in_block_comment = False

    i = 0
    while i < len(lines):
        raw = lines[i]
        stripped = raw.strip()

        if "/*" in stripped:
            in_block_comment = True
        if in_block_comment:
            if "*/" in stripped:
                in_block_comment = False
            i += 1
            continue

        if brace_depth == 0:
            if stripped and not stripped.startswith(("#", "//")):
                if not signature_lines:
                    candidate_start = i
                signature_lines.append(stripped)

                joined = " ".join(signature_lines)
                if ";" in joined and "{" not in joined:
                    signature_lines = []
                elif "{" in joined:
                    before_brace = joined.split("{", 1)[0].strip()
                    looks_function = (
                        "(" in before_brace
                        and ")" in before_brace
                        and not before_brace.startswith(("if ", "for ", "while ", "switch ", "catch "))
                        and not before_brace.startswith(("class ", "struct ", "enum ", "namespace "))
                    )
                    if looks_function:
                        name_match = re.search(r"([~A-Za-z_]\w*(?:::\w+)*)\s*\([^()]*\)\s*(?:const)?\s*$", before_brace)
                        name = name_match.group(1) if name_match else before_brace[:100]

                        body_lines = []
                        local_depth = 0
                        j = candidate_start
                        found_open = False
                        while j < len(lines):
                            current = lines[j]
                            body_lines.append(current)
                            sanitized = strip_comments_and_strings(current)
                            for ch in sanitized:
                                if ch == "{":
                                    local_depth += 1
                                    found_open = True
                                elif ch == "}":
                                    local_depth -= 1
                            if found_open and local_depth == 0:
                                break
                            j += 1

                        body = "\n".join(body_lines)
                        normalized = normalize_cpp_body(body)
                        calls = tuple(
                            sorted(
                                {
                                    call
                                    for call in CALL_PATTERN.findall(strip_comments_and_strings(body))
                                    if call.split("::")[-1] not in CONTROL_WORDS
                                    and call != name
                                }
                            )
                        )
                        functions.append(
                            FunctionInfo(
                                path=path,
                                name=name,
                                start_line=candidate_start + 1,
                                end_line=j + 1,
                                signature=before_brace,
                                body=body,
                                normalized_body=normalized,
                                body_hash=hashlib.sha256(normalized.encode("utf-8")).hexdigest(),
                                calls=calls,
                                parameter_count=count_parameters(before_brace),
                            )
                        )
                        i = j + 1
                        signature_lines = []
                        continue
                    signature_lines = []

        sanitized = strip_comments_and_strings(raw)
        brace_depth += sanitized.count("{")
        brace_depth -= sanitized.count("}")
        brace_depth = max(0, brace_depth)
        i += 1

    return functions


def extract_python_functions(path: Path, text: str) -> list[FunctionInfo]:
    if path.suffix.lower() != ".py":
        return []
    try:
        tree = ast.parse(text)
    except SyntaxError:
        return []

    functions: list[FunctionInfo] = []
    lines = text.splitlines()

    for node in ast.walk(tree):
        if not isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            continue
        end_line = getattr(node, "end_lineno", node.lineno)
        body = "\n".join(lines[node.lineno - 1:end_line])
        normalized = normalize_cpp_body(body)
        calls: set[str] = set()
        for child in ast.walk(node):
            if isinstance(child, ast.Call):
                if isinstance(child.func, ast.Name):
                    calls.add(child.func.id)
                elif isinstance(child.func, ast.Attribute):
                    calls.add(child.func.attr)

        parameter_count = (
            len(node.args.posonlyargs)
            + len(node.args.args)
            + len(node.args.kwonlyargs)
            + (1 if node.args.vararg else 0)
            + (1 if node.args.kwarg else 0)
        )
        functions.append(
            FunctionInfo(
                path=path,
                name=node.name,
                start_line=node.lineno,
                end_line=end_line,
                signature=lines[node.lineno - 1].strip() if lines else node.name,
                body=body,
                normalized_body=normalized,
                body_hash=hashlib.sha256(normalized.encode("utf-8")).hexdigest(),
                calls=tuple(sorted(calls)),
                parameter_count=parameter_count,
            )
        )
    return functions


def check_function_size(functions: Sequence[FunctionInfo], findings: list[Finding]) -> None:
    for fn in functions:
        if fn.line_count >= VERY_LONG_FUNCTION_WARNING:
            add_finding(
                findings,
                "WARNING",
                "function-size",
                fn.path,
                fn.start_line,
                f"Very long function {fn.name}: {fn.line_count} lines.",
                fn.signature,
            )
        elif fn.line_count >= LONG_FUNCTION_WARNING:
            add_finding(
                findings,
                "INFO",
                "function-size",
                fn.path,
                fn.start_line,
                f"Long function {fn.name}: {fn.line_count} lines.",
                fn.signature,
            )

        if fn.parameter_count >= HIGH_PARAMETER_WARNING:
            add_finding(
                findings,
                "INFO",
                "function-parameters",
                fn.path,
                fn.start_line,
                f"Function {fn.name} has {fn.parameter_count} parameters.",
                fn.signature,
            )


def check_exact_duplicate_functions(functions: Sequence[FunctionInfo], findings: list[Finding]) -> None:
    groups: dict[str, list[FunctionInfo]] = defaultdict(list)
    for fn in functions:
        if fn.line_count >= MIN_FUNCTION_BODY_LINES_FOR_DUPLICATION and fn.normalized_body:
            groups[fn.body_hash].append(fn)

    for group in groups.values():
        unique_locations = {(relative(fn.path), fn.start_line) for fn in group}
        if len(unique_locations) < 2:
            continue

        locations = "; ".join(
            f"{relative(fn.path)}:{fn.start_line}-{fn.end_line} ({fn.name})"
            for fn in group
        )
        first = group[0]
        add_finding(
            findings,
            "WARNING",
            "duplicate-function-exact",
            first.path,
            first.start_line,
            f"EXACT duplicate function bodies found in {len(group)} locations: {locations}",
        )


def token_similarity(a: str, b: str) -> float:
    a_tokens = a.split()
    b_tokens = b.split()
    if not a_tokens or not b_tokens:
        return 0.0
    a_counts = Counter(a_tokens)
    b_counts = Counter(b_tokens)
    overlap = sum((a_counts & b_counts).values())
    total = max(sum(a_counts.values()), sum(b_counts.values()))
    return overlap / total if total else 0.0


def check_similar_functions(functions: Sequence[FunctionInfo], findings: list[Finding]) -> None:
    candidates = [
        fn for fn in functions
        if fn.line_count >= MIN_FUNCTION_BODY_LINES_FOR_DUPLICATION
        and len(fn.normalized_body.split()) >= 35
    ]

    # Avoid an O(n^2) explosion in huge repos.
    max_pairs = 75_000
    checked = 0

    for i, left in enumerate(candidates):
        for right in candidates[i + 1:]:
            if checked >= max_pairs:
                return
            checked += 1

            if left.body_hash == right.body_hash:
                continue

            left_tokens = len(left.normalized_body.split())
            right_tokens = len(right.normalized_body.split())
            ratio = min(left_tokens, right_tokens) / max(left_tokens, right_tokens)
            if ratio < 0.70:
                continue

            similarity = token_similarity(left.normalized_body, right.normalized_body)
            if similarity >= SIMILAR_BODY_THRESHOLD:
                add_finding(
                    findings,
                    "INFO",
                    "duplicate-function-similar",
                    left.path,
                    left.start_line,
                    (
                        f"Possible duplicate implementations ({similarity:.0%} token overlap): "
                        f"{relative(left.path)}:{left.start_line} {left.name} and "
                        f"{relative(right.path)}:{right.start_line} {right.name}."
                    ),
                )


def check_same_call_sets(functions: Sequence[FunctionInfo], findings: list[Finding]) -> None:
    groups: dict[tuple[str, ...], list[FunctionInfo]] = defaultdict(list)
    for fn in functions:
        if len(fn.calls) >= MIN_CALLS_FOR_CALLSET_WARNING:
            groups[fn.calls].append(fn)

    for calls, group in groups.items():
        names = {fn.name for fn in group}
        locations = {(relative(fn.path), fn.start_line) for fn in group}
        if len(names) < 2 or len(locations) < 2:
            continue
        first = group[0]
        formatted = "; ".join(
            f"{relative(fn.path)}:{fn.start_line} {fn.name}"
            for fn in group[:8]
        )
        add_finding(
            findings,
            "INFO",
            "same-call-set",
            first.path,
            first.start_line,
            f"Functions call the same {len(calls)} functions and may duplicate orchestration: {formatted}. Calls={', '.join(calls)}",
        )


def camel_tokens(name: str) -> list[str]:
    parts = re.findall(r"[A-Z]?[a-z]+|[A-Z]+(?=[A-Z]|$)|\d+", name)
    return [p.lower() for p in parts]


def normalized_concept(name: str) -> str:
    removable = {
        "is", "has", "was", "did", "can", "should", "wants", "want",
        "pressed", "just", "held", "intent", "requested", "request",
        "pending", "current", "next", "last", "local", "server",
        "authoritative", "predicted", "prediction", "state", "value",
        "flag", "active", "available", "enabled", "about", "to",
    }
    tokens = [token for token in camel_tokens(name) if token not in removable]
    return "_".join(tokens[:3])


def check_identifier_drift(path: Path, text: str, findings: list[Finding]) -> None:
    clean = strip_comments_and_strings(text)
    identifiers: dict[str, list[int]] = defaultdict(list)

    for match in IDENTIFIER_PATTERN.finditer(clean):
        ident = match.group(0)
        if len(ident) < 5:
            continue
        identifiers[ident].append(line_number(clean, match.start()))

    concept_groups: dict[str, set[str]] = defaultdict(set)
    for ident in identifiers:
        concept = normalized_concept(ident)
        if concept and len(concept) >= 4:
            concept_groups[concept].add(ident)

        lower_joined = "".join(camel_tokens(ident))
        suspicious = [word for word in SUSPICIOUS_NAME_WORDS if word in lower_joined]
        if suspicious:
            add_finding(
                findings,
                "INFO",
                "suspicious-name",
                path,
                identifiers[ident][0],
                f"Suspicious drift-prone identifier '{ident}' contains: {', '.join(sorted(suspicious))}.",
            )

    for concept, names in concept_groups.items():
        if len(names) < 4:
            continue
        meaningful = sorted(names)
        add_finding(
            findings,
            "INFO",
            "identifier-drift",
            path,
            min(identifiers[name][0] for name in names),
            f"Many identifiers normalize to concept '{concept}': {', '.join(meaningful[:16])}",
        )


def check_cleanliness(path: Path, text: str, findings: list[Finding]) -> None:
    for label, pattern in REPO_CLEANLINESS_PATTERNS:
        for match in pattern.finditer(text):
            severity = "WARNING" if label == "merge conflict marker" else "INFO"
            add_finding(
                findings,
                severity,
                "cleanliness",
                path,
                line_number(text, match.start()),
                f"Repository cleanliness signal: {label}.",
                match.group(0).splitlines()[0],
            )


def check_filename(path: Path, findings: list[Finding]) -> None:
    stem_tokens = set(re.split(r"[-_.\s]+", path.stem.lower()))
    suspicious = sorted(stem_tokens & SUSPICIOUS_FILENAME_PARTS)
    if suspicious:
        add_finding(
            findings,
            "INFO",
            "filename",
            path,
            1,
            f"Suspicious temporary/versioned filename contains: {', '.join(suspicious)}.",
        )


def collect_duplicate_defaults(files: Sequence[Path], findings: list[Finding]) -> None:
    for concept, pattern in DUPLICATE_DEFAULT_PATTERNS.items():
        values: dict[str, list[tuple[Path, int, str]]] = defaultdict(list)
        for path in files:
            text = read_text(path)
            for match in pattern.finditer(text):
                value = match.group(1)
                values[value].append(
                    (path, line_number(text, match.start()), match.group(0).strip())
                )

        if len(values) <= 1:
            continue

        all_locations = []
        for value, entries in sorted(values.items()):
            for path, line, evidence in entries[:8]:
                all_locations.append(f"{relative(path)}:{line}={value}")

        first_value_entries = next(iter(values.values()))
        first_path, first_line, _ = first_value_entries[0]
        add_finding(
            findings,
            "WARNING",
            "conflicting-defaults",
            first_path,
            first_line,
            f"Conflicting values for '{concept}': " + "; ".join(all_locations[:20]),
        )


def select_checks(requested: str | None) -> set[str] | None:
    if not requested:
        return None
    aliases = {
        "printf": {"printf"},
        "size": {"file-size", "function-size", "function-parameters"},
        "duplicates": {
            "duplicate-function-exact",
            "duplicate-function-similar",
            "same-call-set",
            "conflicting-defaults",
        },
        "names": {"identifier-drift", "suspicious-name", "filename"},
        "headers": {"header"},
        "cleanliness": {"cleanliness"},
    }
    return aliases.get(requested, {requested})


def scan(include_third_party: bool) -> list[Finding]:
    findings: list[Finding] = []
    files = list(iter_source_files(include_third_party))
    functions: list[FunctionInfo] = []

    for path in files:
        text = read_text(path)
        if not text:
            continue

        check_raw_output(path, text, findings)
        check_file_size(path, text, findings)
        check_header(path, text, findings)
        check_identifier_drift(path, text, findings)
        check_cleanliness(path, text, findings)
        check_filename(path, findings)

        functions.extend(extract_cpp_functions(path, text))
        functions.extend(extract_python_functions(path, text))

    check_function_size(functions, findings)
    check_exact_duplicate_functions(functions, findings)
    check_similar_functions(functions, findings)
    check_same_call_sets(functions, findings)
    collect_duplicate_defaults(files, findings)

    severity_order = {"ERROR": 0, "WARNING": 1, "INFO": 2}
    findings.sort(
        key=lambda f: (
            severity_order.get(f.severity, 99),
            f.check,
            f.path,
            f.line,
            f.message,
        )
    )
    return findings


def build_text_report(findings: Sequence[Finding]) -> str:
    lines: list[str] = []
    lines.append("=" * 78)
    lines.append("MIMITA OVERSEER REPORT")
    lines.append("=" * 78)
    lines.append(f"Generated: {datetime.now().isoformat(timespec='seconds')}")
    lines.append(f"Repository: {REPO_ROOT}")
    lines.append("Mode: report-only by default; use --strict to make ERROR findings block.")
    lines.append("")

    counts = Counter(f.severity for f in findings)
    lines.append(
        f"Findings: {len(findings)} total | "
        f"ERROR {counts.get('ERROR', 0)} | "
        f"WARNING {counts.get('WARNING', 0)} | "
        f"INFO {counts.get('INFO', 0)}"
    )
    lines.append("")

    if not findings:
        lines.append("Overall Status: CLEAN")
        return "\n".join(lines) + "\n"

    current_check = None
    for finding in findings:
        if finding.check != current_check:
            current_check = finding.check
            lines.append("")
            lines.append(f"[{current_check}]")
            lines.append("-" * 78)

        location = f"{finding.path}:{finding.line}"
        lines.append(f"{finding.severity:<7} {location}")
        lines.append(f"        {finding.message}")
        if finding.evidence:
            lines.append(f"        > {finding.evidence}")

    lines.append("")
    lines.append("=" * 78)
    lines.append("Overall Status: FINDINGS REPORTED")
    lines.append("=" * 78)
    return "\n".join(lines) + "\n"


def write_logs(text_report: str, findings: Sequence[Finding], write_json: bool) -> None:
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    (LOG_DIR / "overseer-latest.txt").write_text(text_report, encoding="utf-8")

    timestamp = datetime.now().strftime("%m-%d-%Y-%H-%M-%S")
    (LOG_DIR / f"{timestamp}-overseer.txt").write_text(text_report, encoding="utf-8")

    if write_json:
        payload = {
            "generated": datetime.now().isoformat(timespec="seconds"),
            "repo_root": str(REPO_ROOT),
            "findings": [asdict(finding) for finding in findings],
        }
        (LOG_DIR / "overseer-latest.json").write_text(
            json.dumps(payload, indent=2),
            encoding="utf-8",
        )

    old_logs = sorted(LOG_DIR.glob("*-overseer.txt"))
    while len(old_logs) > 30:
        old_logs.pop(0).unlink(missing_ok=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Deterministic MiMITA repository drift scanner.")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit 1 when ERROR findings exist. Default always exits 0.",
    )
    parser.add_argument(
        "--include-third-party",
        action="store_true",
        help="Scan third_party/vendor/external/dependency directories.",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Also write logs/overseer-latest.json.",
    )
    parser.add_argument(
        "--check",
        help="Only show one check group: printf, size, duplicates, names, headers, cleanliness.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    findings = scan(include_third_party=args.include_third_party)

    selected = select_checks(args.check)
    if selected is not None:
        findings = [finding for finding in findings if finding.check in selected]

    report = build_text_report(findings)
    print(report, end="")
    write_logs(report, findings, write_json=args.json)

    has_errors = any(finding.severity == "ERROR" for finding in findings)
    if args.strict and has_errors:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
