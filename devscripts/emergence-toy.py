"""Graph-only nodes-and-connections experiment toy.

The simulated world is an undirected adjacency graph. Node IDs are opaque
references; presentation layouts never enter graph state. Event causality is
recorded with explicit ``caused_by`` references, not timestamps or line order.
"""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
import random
import re
import sys
import threading
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from queue import Empty, Queue
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
LAB_ROOT = ROOT / ".dev" / "emergence-toy"
PRESET_DIR = LAB_ROOT / "presets"
RUN_DIR = LAB_ROOT / "runs"
DEFAULT_GRAPH = """0: 1, 2
1: 0, 3
2: 0, 3
3: 1, 2, 4
4: 3"""
DEFAULT_RULES = [
    {"id": "close-open-path", "pattern": "open_path", "action": "connect_ends"},
    {"id": "extend-edge", "pattern": "edge", "action": "extend_second"},
]


def utc_stamp() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="microseconds").replace("+00:00", "Z")


def file_stamp() -> str:
    # Windows filenames cannot contain colons; the rest remains ISO-8601-like UTC.
    return utc_stamp().replace(":", "-")


def parse_graph(source: str) -> dict[str, set[str]]:
    """Parse adjacency-list text into the only modeled node fact: adjacency."""
    graph: dict[str, set[str]] = {}
    for line_no, raw in enumerate(source.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if ":" not in line:
            raise ValueError(f"Line {line_no}: expected 'node: neighbor, neighbor'")
        node, neighbors_text = line.split(":", 1)
        node = node.strip()
        if not node:
            raise ValueError(f"Line {line_no}: node reference is empty")
        graph.setdefault(node, set())
        for neighbor in re.split(r"[\s,]+", neighbors_text.strip()):
            if not neighbor:
                continue
            if neighbor == node:
                raise ValueError(f"Line {line_no}: self-connections are not supported")
            graph.setdefault(neighbor, set())
            graph[node].add(neighbor)
            graph[neighbor].add(node)
    if not graph:
        raise ValueError("Graph is empty; add at least one node")
    return graph


def graph_text(graph: dict[str, set[str]]) -> str:
    return "\n".join(f"{node}: {', '.join(sorted(graph[node]))}" for node in sorted(graph))


def graph_hash(graph: dict[str, set[str]]) -> str:
    canonical = {node: sorted(graph[node]) for node in sorted(graph)}
    return hashlib.sha256(json.dumps(canonical, separators=(",", ":")).encode()).hexdigest()


def parse_rules(source: str) -> list[dict[str, str]]:
    try:
        rules = json.loads(source)
    except json.JSONDecodeError as exc:
        raise ValueError(f"Rules need to be a JSON list: {exc}") from exc
    if not isinstance(rules, list) or not rules:
        raise ValueError("Rules must be a non-empty JSON list")
    allowed = {"open_path": {"connect_ends"}, "edge": {"extend_second", "remove_edge"}}
    result = []
    for index, rule in enumerate(rules):
        if not isinstance(rule, dict):
            raise ValueError(f"Rule {index + 1} must be an object")
        rule_id = str(rule.get("id", f"rule-{index + 1}")).strip()
        pattern = str(rule.get("pattern", ""))
        action = str(rule.get("action", ""))
        if pattern not in allowed or action not in allowed[pattern]:
            choices = ", ".join(f"{p}: {', '.join(sorted(a))}" for p, a in allowed.items())
            raise ValueError(f"Rule {rule_id}: unsupported pattern/action. Available: {choices}")
        result.append({"id": rule_id, "pattern": pattern, "action": action})
    return result


def list_edges(graph: dict[str, set[str]]) -> list[tuple[str, str]]:
    return [(a, b) for a in sorted(graph) for b in sorted(graph[a]) if a < b]


def matches_for(graph: dict[str, set[str]], pattern: str) -> list[tuple[str, ...]]:
    if pattern == "edge":
        return [(a, b) for a, b in list_edges(graph)]
    matches = set()
    for middle in sorted(graph):
        neighbors = sorted(graph[middle])
        for a, c in itertools.combinations(neighbors, 2):
            if c not in graph[a]:
                matches.add((a, middle, c))
    return sorted(matches)


def pattern_counts(graph: dict[str, set[str]]) -> dict[str, int]:
    edges = len(list_edges(graph))
    triangle_triplets = sum(
        len(graph[a] & graph[b])
        for a, b in list_edges(graph)
    )
    triangles = triangle_triplets // 3
    open_paths = sum(len(neighbors) * (len(neighbors) - 1) // 2
                     for neighbors in graph.values()) - 3 * triangles
    return {"edge": edges, "open_path": open_paths, "triangle": triangles,
            "node_count": len(graph)}


def graph_components(graph: dict[str, set[str]]) -> int:
    unseen = set(graph)
    count = 0
    while unseen:
        count += 1
        stack = [unseen.pop()]
        while stack:
            node = stack.pop()
            for neighbor in graph[node] & unseen:
                unseen.remove(neighbor)
                stack.append(neighbor)
    return count


def stable_pattern_signatures(graph: dict[str, set[str]]) -> dict[str, int]:
    # Small, name-independent motif counters. They are graph facts, not node fields.
    return pattern_counts(graph)


@dataclass
class GraphRun:
    graph: dict[str, set[str]]
    rules: list[dict[str, str]]
    run_id: str
    journal_path: Path
    seed: int = 0
    match_policy: str = "first"
    max_rewrites: int = 100
    on_event: Any = None
    event_ids: set[str] = field(default_factory=set)
    edge_causes: dict[tuple[str, str], str] = field(default_factory=dict)
    node_causes: dict[str, str] = field(default_factory=dict)
    event_count: int = 0
    completed: bool = False

    def __post_init__(self):
        self.rng = random.Random(self.seed)
        self.journal_path.parent.mkdir(parents=True, exist_ok=True)
        self._write("experiment.started", fields={
            "graph_hash": graph_hash(self.graph), "rule_set": self.rules,
            "seed": self.seed, "match_policy": self.match_policy,
            "node_count": len(self.graph), "connection_count": len(list_edges(self.graph)),
        })

    def _write(self, event: str, *, event_id: str = "", caused_by: list[str] | None = None,
               rule_id: str = "", matched_pattern: str = "", graph_delta: dict | None = None,
               fields: dict | None = None) -> str:
        if not event_id:
            event_id = f"{self.run_id}-e{self.event_count:06d}"
        record = {"schema_version": 1, "experiment_id": self.run_id.rsplit("-run-", 1)[0],
                  "run_id": self.run_id, "event_id": event_id, "event": event,
                  "caused_by": sorted(set(caused_by or [])),
                  "rule_id": rule_id, "matched_pattern": matched_pattern,
                  "graph_hash": graph_hash(self.graph),
                  "graph_delta": graph_delta or {}, "fields": fields or {}}
        with self.journal_path.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps(record, separators=(",", ":"), ensure_ascii=False) + "\n")
            handle.flush()
        if self.on_event:
            self.on_event(record)
        self.event_ids.add(event_id)
        self.event_count += 1
        return event_id

    def _causes_for_match(self, match: tuple[str, ...], pattern: str) -> list[str]:
        causes = set()
        if pattern == "edge":
            pairs = [(match[0], match[1])]
        else:
            pairs = [(match[0], match[1]), (match[1], match[2])]
        for a, b in pairs:
            edge = tuple(sorted((a, b)))
            if edge in self.edge_causes:
                causes.add(self.edge_causes[edge])
        for node in match:
            if node in self.node_causes:
                causes.add(self.node_causes[node])
        return sorted(causes)

    def step(self) -> dict | None:
        if self.event_count - 1 >= self.max_rewrites:
            self.finish("completed")
            return None
        for rule in self.rules:
            matches = matches_for(self.graph, rule["pattern"])
            if not matches:
                continue
            match = self.rng.choice(matches) if self.match_policy == "seeded_random" else matches[0]
            causes = self._causes_for_match(match, rule["pattern"])
            counts_before = pattern_counts(self.graph)
            added_nodes: list[str] = []
            added_connections: list[list[str]] = []
            removed_connections: list[list[str]] = []
            if rule["action"] == "connect_ends":
                a, _, c = match
                self.graph[a].add(c)
                self.graph[c].add(a)
                added_connections.append([a, c])
            elif rule["action"] == "extend_second":
                _, endpoint = match
                new_id = self._next_node_id()
                self.graph[new_id] = {endpoint}
                self.graph[endpoint].add(new_id)
                added_nodes.append(new_id)
                added_connections.append([endpoint, new_id])
            elif rule["action"] == "remove_edge":
                a, b = match
                self.graph[a].remove(b)
                self.graph[b].remove(a)
                removed_connections.append([a, b])
            event_id = self._write(
                "rule.applied", caused_by=causes, rule_id=rule["id"],
                matched_pattern=rule["pattern"], graph_delta={
                    "added_nodes": added_nodes,
                    "added_connections": added_connections,
                    "removed_connections": removed_connections,
                }, fields={"event_number_for_display": self.event_count,
                          "pattern_counts_before": counts_before,
                          "pattern_counts_after": pattern_counts(self.graph),
                          "component_count_after": graph_components(self.graph)})
            for node in added_nodes:
                self.node_causes[node] = event_id
            for a, b in added_connections:
                self.edge_causes[tuple(sorted((a, b)))] = event_id
            for a, b in removed_connections:
                self.edge_causes.pop(tuple(sorted((a, b))), None)
            return {"event_id": event_id, "caused_by": causes, "rule": rule,
                    "match": match, "added_nodes": added_nodes,
                    "added_connections": added_connections,
                    "removed_connections": removed_connections}
        return None

    def _next_node_id(self) -> str:
        numeric = [int(node) for node in self.graph if node.isdigit()]
        suffix = max(numeric, default=-1) + 1
        candidate = str(suffix)
        while candidate in self.graph:
            suffix += 1
            candidate = str(suffix)
        return candidate

    def finish(self, status: str = "completed", error: str = "") -> None:
        if self.completed and status == "completed":
            return
        self._write("experiment." + status, fields={
            "event_count": self.event_count, "graph_hash": graph_hash(self.graph),
            "node_count": len(self.graph), "connection_count": len(list_edges(self.graph)),
            "pattern_counts": pattern_counts(self.graph), "error": error,
        })
        self.completed = True


def run_batch(manifest_path: Path, output_root: Path, on_event=None) -> list[dict]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    graph = parse_graph(manifest.get("graph", DEFAULT_GRAPH))
    base_rules = parse_rules(json.dumps(manifest.get("rules", DEFAULT_RULES)))
    seeds = manifest.get("seeds", [0, 1, 2, 3, 4])
    max_events = max(1, int(manifest.get("max_events", 100)))
    rule_sets = [[rule] for rule in base_rules]
    if len(base_rules) > 1:
        rule_sets.append(base_rules)
        rule_sets.append(list(reversed(base_rules)))
    summary = []
    exp_dir = output_root / f"experiment-{file_stamp()}"
    for variant_index, rules in enumerate(rule_sets, 1):
        for seed in seeds:
            run_id = f"{exp_dir.name}-run-{variant_index:02d}-seed-{seed}"
            run_graph = {node: set(neighbors) for node, neighbors in graph.items()}
            journal = exp_dir / run_id / "events.jsonl"
            run = GraphRun(run_graph, rules, run_id, journal, int(seed), "seeded_random",
                           max_rewrites=max_events, on_event=on_event)
            for _ in range(max_events):
                if run.step() is None:
                    break
            run.finish("completed")
            item = {"run_id": run_id, "journal": str(journal),
                    "rules": [rule["id"] for rule in rules], "seed": seed,
                    "graph_hash": graph_hash(run.graph), "pattern_counts": pattern_counts(run.graph),
                    "node_count": len(run.graph), "connection_count": len(list_edges(run.graph))}
            summary.append(item)
            print(json.dumps(item, ensure_ascii=False), flush=True)
    exp_dir.mkdir(parents=True, exist_ok=True)
    motif_families = ("edge", "open_path", "triangle")
    recurrence = {
        motif: {
            "runs_present": sum(1 for item in summary if item["pattern_counts"].get(motif, 0) > 0),
            "rule_sets_present": len({tuple(item["rules"]) for item in summary
                                       if item["pattern_counts"].get(motif, 0) > 0}),
        }
        for motif in motif_families
    }
    batch_record = {"runs": summary, "pattern_recurrence": recurrence}
    (exp_dir / "batch-summary.json").write_text(json.dumps(batch_record, indent=2) + "\n", encoding="utf-8")
    return summary


def make_gui():
    import tkinter as tk
    from tkinter import messagebox, ttk

    root = tk.Tk()
    root.title("Nodes & Connections Lab — v1")
    root.geometry("1320x820")
    root.minsize(980, 650)
    root.configure(bg="#111318")

    bg, panel, text, muted, accent = "#111318", "#1b1e26", "#e8eaf0", "#9ba3b4", "#64d9ce"
    style = ttk.Style(root)
    try:
        style.theme_use("clam")
    except tk.TclError:
        pass
    style.configure("TFrame", background=bg)
    style.configure("Panel.TFrame", background=panel)
    style.configure("TLabel", background=bg, foreground=text, font=("Segoe UI", 10))
    style.configure("Title.TLabel", font=("Segoe UI", 16, "bold"), foreground=accent)
    style.configure("Muted.TLabel", foreground=muted)
    style.configure("TButton", font=("Segoe UI", 10), padding=(9, 5))
    style.configure("TCombobox", padding=4)

    state: dict[str, Any] = {"graph": parse_graph(DEFAULT_GRAPH), "rules": list(DEFAULT_RULES),
                             "run": None, "view": "2D", "events": [], "batch_queue": Queue(),
                             "batch_thread": None, "auto_id": None, "drag": None,
                             "layout_cache": None}

    top = ttk.Frame(root, padding=(14, 10))
    top.pack(fill="x")
    ttk.Label(top, text="NODES & CONNECTIONS LAB", style="Title.TLabel").pack(side="left")
    ttk.Label(top, text="v1 · graph only · layout is for display", style="Muted.TLabel").pack(side="left", padx=16)
    status_var = tk.StringVar(value="Ready · no run active")
    ttk.Label(top, textvariable=status_var).pack(side="right")

    actions = ttk.Frame(root, padding=(12, 4))
    actions.pack(fill="x")

    body = ttk.Frame(root, padding=(10, 4))
    body.pack(fill="both", expand=True)
    body.columnconfigure(0, weight=0, minsize=300)
    body.columnconfigure(1, weight=1, minsize=440)
    body.columnconfigure(2, weight=0, minsize=305)
    body.rowconfigure(0, weight=1)

    left = ttk.Frame(body, style="Panel.TFrame", padding=10)
    left.grid(row=0, column=0, sticky="nsew", padx=(0, 6))
    center = ttk.Frame(body, style="Panel.TFrame", padding=8)
    center.grid(row=0, column=1, sticky="nsew", padx=6)
    right = ttk.Frame(body, style="Panel.TFrame", padding=10)
    right.grid(row=0, column=2, sticky="nsew", padx=(6, 0))
    center.rowconfigure(1, weight=1)
    center.columnconfigure(0, weight=1)
    left.rowconfigure(2, weight=1)
    left.rowconfigure(5, weight=1)
    right.rowconfigure(2, weight=1)
    right.rowconfigure(5, weight=1)

    ttk.Label(left, text="Initial graph (adjacency only)", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
    ttk.Label(left, text="One line per node: 0: 1, 2", style="Muted.TLabel").grid(row=1, column=0, sticky="w", pady=(2, 4))
    graph_textbox = tk.Text(left, height=10, bg="#0f1116", fg=text, insertbackground=text,
                            font=("Consolas", 10), relief="flat", wrap="none")
    graph_textbox.grid(row=2, column=0, sticky="nsew")
    graph_textbox.insert("1.0", DEFAULT_GRAPH)

    ttk.Label(left, text="Rewrite rules (JSON)", font=("Segoe UI", 11, "bold")).grid(row=3, column=0, sticky="w", pady=(10, 0))
    ttk.Label(left, text='open_path → connect_ends · edge → extend_second/remove_edge',
              style="Muted.TLabel", wraplength=280).grid(row=4, column=0, sticky="w", pady=(2, 4))
    rules_textbox = tk.Text(left, height=10, bg="#0f1116", fg=text, insertbackground=text,
                            font=("Consolas", 9), relief="flat", wrap="word")
    rules_textbox.grid(row=5, column=0, sticky="nsew")
    rules_textbox.insert("1.0", json.dumps(DEFAULT_RULES, indent=2))

    settings = ttk.Frame(left, style="Panel.TFrame")
    settings.grid(row=6, column=0, sticky="ew", pady=(10, 0))
    ttk.Label(settings, text="Max events per run").grid(row=0, column=0, sticky="w")
    max_events_var = tk.StringVar(value="100")
    ttk.Entry(settings, textvariable=max_events_var, width=9).grid(row=0, column=1, padx=6)
    ttk.Label(settings, text="Batch seeds").grid(row=1, column=0, sticky="w", pady=(6, 0))
    seeds_var = tk.StringVar(value="8")
    ttk.Entry(settings, textvariable=seeds_var, width=9).grid(row=1, column=1, padx=6, pady=(6, 0))
    ttk.Label(settings, text="Preset label (optional)").grid(row=2, column=0, sticky="w", pady=(6, 0))
    preset_label_var = tk.StringVar()
    ttk.Entry(settings, textvariable=preset_label_var, width=21).grid(row=2, column=1, padx=6, pady=(6, 0))

    viewbar = ttk.Frame(center, style="Panel.TFrame")
    viewbar.grid(row=0, column=0, sticky="ew", pady=(0, 4))
    ttk.Label(viewbar, text="Graph view", font=("Segoe UI", 11, "bold")).pack(side="left")
    view_var = tk.StringVar(value="2D")
    view_box = ttk.Combobox(viewbar, textvariable=view_var, values=("2D", "3D"), state="readonly", width=6)
    view_box.pack(side="left", padx=8)
    ttk.Label(viewbar, text="Display layout only", style="Muted.TLabel").pack(side="right")
    canvas = tk.Canvas(center, bg="#0c0e13", highlightthickness=0)
    canvas.grid(row=1, column=0, sticky="nsew")
    canvas.bind("<Configure>", lambda _e: draw_graph())
    view_box.bind("<<ComboboxSelected>>", lambda _e: (state.update(view=view_var.get()), draw_graph()))

    ttk.Label(right, text="Patterns & graph summary", font=("Segoe UI", 11, "bold")).grid(row=0, column=0, sticky="w")
    summary_text = tk.Text(right, height=7, bg="#0f1116", fg=text, font=("Consolas", 9), relief="flat", state="disabled")
    summary_text.grid(row=1, column=0, sticky="ew", pady=(4, 10))
    ttk.Label(right, text="Causal event feed", font=("Segoe UI", 11, "bold")).grid(row=2, column=0, sticky="nw")
    event_list = tk.Listbox(right, bg="#0f1116", fg=text, font=("Consolas", 9), relief="flat",
                            selectbackground="#25645f", activestyle="none")
    event_list.grid(row=3, column=0, sticky="nsew", pady=(4, 6))
    ttk.Label(right, text="Event details", font=("Segoe UI", 11, "bold")).grid(row=4, column=0, sticky="w", pady=(4, 0))
    detail_text = tk.Text(right, height=9, bg="#0f1116", fg=text, font=("Consolas", 8), relief="flat", state="disabled", wrap="word")
    detail_text.grid(row=5, column=0, sticky="nsew", pady=(4, 0))

    footer = ttk.Frame(root, padding=(12, 5))
    footer.pack(fill="x")
    journal_var = tk.StringVar(value="Journal: save a run to create events.jsonl")
    ttk.Label(footer, textvariable=journal_var, style="Muted.TLabel").pack(side="left")
    batch_var = tk.StringVar(value="Batch: 0 runs")
    ttk.Label(footer, textvariable=batch_var).pack(side="right")

    def edit_graph() -> bool:
        try:
            state["graph"] = parse_graph(graph_textbox.get("1.0", "end-1c"))
            state["layout_cache"] = None
            draw_graph()
            update_summary()
            return True
        except ValueError as exc:
            messagebox.showerror("Graph input", str(exc), parent=root)
            return False

    def edit_rules() -> bool:
        try:
            state["rules"] = parse_rules(rules_textbox.get("1.0", "end-1c"))
            return True
        except ValueError as exc:
            messagebox.showerror("Rule input", str(exc), parent=root)
            return False

    def save_preset():
        if not edit_graph() or not edit_rules():
            return
        PRESET_DIR.mkdir(parents=True, exist_ok=True)
        stamp = file_stamp()
        label = preset_label_var.get().strip()
        suffix = "-" + re.sub(r"[^A-Za-z0-9_-]+", "-", label).strip("-")[:48] if label else ""
        path = PRESET_DIR / f"{stamp}{suffix}.json"
        counter = 2
        while path.exists():
            path = PRESET_DIR / f"{stamp}-{counter}{suffix}.json"
            counter += 1
        data = {"preset_schema": 1, "created_utc": utc_stamp(), "label": label,
                "graph": graph_text(state["graph"]), "rules": state["rules"],
                "max_events": get_max_events(), "batch_seeds": get_seed_count()}
        path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        refresh_presets()
        preset_var.set(path.name)
        status_var.set(f"Preset saved · {path.name}")

    def get_max_events() -> int:
        try:
            return min(10000, max(1, int(max_events_var.get())))
        except ValueError:
            return 100

    def get_seed_count() -> int:
        try:
            return min(500, max(1, int(seeds_var.get())))
        except ValueError:
            return 8

    def refresh_presets():
        PRESET_DIR.mkdir(parents=True, exist_ok=True)
        names = sorted((p.name for p in PRESET_DIR.glob("*.json")), reverse=True)
        preset_box.configure(values=names)
        if names and not preset_var.get():
            preset_var.set(names[0])

    def load_preset():
        name = preset_var.get().strip()
        path = PRESET_DIR / name
        if not name or not path.is_file() or path.parent.resolve() != PRESET_DIR.resolve():
            messagebox.showinfo("Presets", "Choose a saved preset first.", parent=root)
            return
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
            graph_value = data["graph"]
            rules_value = data["rules"]
            state["graph"] = parse_graph(graph_value)
            state["rules"] = parse_rules(json.dumps(rules_value))
            graph_textbox.delete("1.0", "end")
            graph_textbox.insert("1.0", graph_value)
            rules_textbox.delete("1.0", "end")
            rules_textbox.insert("1.0", json.dumps(rules_value, indent=2))
            preset_label_var.set(data.get("label", ""))
            max_events_var.set(str(data.get("max_events", 100)))
            seeds_var.set(str(data.get("batch_seeds", 8)))
            state["layout_cache"] = None
            draw_graph()
            update_summary()
            status_var.set(f"Preset loaded · {name}")
        except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
            messagebox.showerror("Preset could not load", str(exc), parent=root)

    def reset_graph():
        if edit_graph():
            state["run"] = None
            state["events"] = []
            event_list.delete(0, "end")
            journal_var.set("Journal: press Start Run to begin a new recorded run")
            status_var.set("Graph reset from editor")
            update_summary()

    def begin_run():
        if not edit_graph() or not edit_rules():
            return
        state["run"] = None
        state["events"] = []
        event_list.delete(0, "end")
        run_id = "experiment-" + file_stamp() + "-run-manual"
        run_dir = RUN_DIR / run_id
        suffix = 2
        while run_dir.exists():
            run_id = f"experiment-{file_stamp()}-run-manual-{suffix}"
            run_dir = RUN_DIR / run_id
            suffix += 1
        journal = run_dir / "events.jsonl"
        run = GraphRun({n: set(v) for n, v in state["graph"].items()},
                       list(state["rules"]), run_id, journal, 0, "first",
                       max_rewrites=get_max_events())
        state["run"] = run
        state["events"].append({"event_id": run_id + "-e000000", "event": "experiment.started",
                                "caused_by": [], "fields": {"message": "Starting graph snapshot"}})
        event_list.insert("end", "experiment.started · initial graph")
        journal_var.set("Journal: " + str(journal))
        status_var.set("Run ready · Next Event applies one graph rewrite")
        draw_graph()
        update_summary()

    def do_step():
        run = state.get("run")
        if run is None:
            begin_run()
            run = state.get("run")
            if run is None:
                return
        result = run.step()
        if result is None:
            run.finish("completed")
            status_var.set("No matching rule · graph reached a stable point for this rule set")
            return
        event = {"event_id": result["event_id"], "event": "rule.applied",
                 "caused_by": result["caused_by"], "rule_id": result["rule"]["id"],
                 "matched_pattern": result["rule"]["pattern"],
                 "match": list(result["match"]),
                 "graph_delta": {"added_nodes": result["added_nodes"],
                                 "added_connections": result["added_connections"],
                                 "removed_connections": result["removed_connections"]},
                 "fields": {"pattern_counts": pattern_counts(run.graph),
                            "graph_hash": graph_hash(run.graph)}}
        state["events"].append(event)
        event_list.insert("end", f"{event['event_id']} · {event['rule_id']} · caused by {', '.join(event['caused_by']) or 'initial graph'}")
        event_list.yview_moveto(1.0)
        status_var.set(f"Applied {event['rule_id']} · {len(run.graph)} nodes · {len(list_edges(run.graph))} connections")
        state["layout_cache"] = None
        draw_graph()
        update_summary()

    def auto_run():
        if state.get("auto_id") is not None:
            root.after_cancel(state["auto_id"])
            state["auto_id"] = None
            status_var.set("Paused · use Next Event to continue one rewrite")
            return
        if state.get("run") is None:
            begin_run()
        if state.get("run") is None:
            return
        def tick():
            state["auto_id"] = None
            do_step()
            if not state["run"].completed:
                state["auto_id"] = root.after(400, tick)
        tick()

    def run_batch_gui():
        if not edit_graph() or not edit_rules():
            return
        if state["batch_thread"] and state["batch_thread"].is_alive():
            status_var.set("A batch is already running")
            return
        count = get_seed_count()
        max_events = get_max_events()
        graph_source = graph_text(state["graph"])
        rules_value = list(state["rules"])
        batch_root = RUN_DIR / ("batch-" + file_stamp())
        batch_var.set("Batch running…")
        status_var.set(f"Running rule variants × {count} seeds in background")
        def worker():
            try:
                manifest = {"graph": graph_source, "rules": rules_value,
                            "seeds": list(range(count)), "max_events": max_events}
                temp = batch_root / "manifest.json"
                temp.parent.mkdir(parents=True, exist_ok=True)
                temp.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
                def report_event(record):
                    state["batch_queue"].put(("event", record, str(batch_root)))
                results = run_batch(temp, batch_root, on_event=report_event)
                state["batch_queue"].put(("done", results, str(batch_root)))
            except Exception as exc:  # surfaced in the GUI and preserved in the runner output
                state["batch_queue"].put(("error", str(exc), str(batch_root)))
        state["batch_thread"] = threading.Thread(target=worker, name="graph-toy-batch", daemon=True)
        state["batch_thread"].start()

    def poll_batch():
        for _ in range(512):
            try:
                kind, payload, location = state["batch_queue"].get_nowait()
            except Empty:
                break
            if kind == "done":
                batch_var.set(f"Batch complete · {len(payload)} runs")
                status_var.set(f"Batch saved · {location}")
                for row in payload:
                    event = {"event": "run.summary", "run_id": row["run_id"],
                             "fields": row}
                    state["events"].append(event)
                    event_list.insert("end", f"batch · {row['run_id']} · patterns {row['pattern_counts']}")
            elif kind == "event":
                record = payload
                state["events"].append(record)
                event_list.insert("end", f"{record['run_id']} · {record['event_id']} · {record['event']}")
                status_var.set(f"Live batch · {record['run_id']} · {record['event']}")
            else:
                batch_var.set("Batch failed")
                status_var.set("Batch error · " + payload)
        if event_list.size():
            event_list.yview_moveto(1.0)
        root.after(200, poll_batch)

    def save_script_manifest():
        if not edit_graph() or not edit_rules():
            return
        PRESET_DIR.mkdir(parents=True, exist_ok=True)
        path = PRESET_DIR / (file_stamp() + "-batch-manifest.json")
        manifest = {"graph": graph_text(state["graph"]), "rules": state["rules"],
                    "seeds": list(range(get_seed_count())), "max_events": get_max_events()}
        path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        status_var.set(f"Batch manifest saved · {path}")

    ttk.Button(actions, text="Apply Edits", command=lambda: (edit_graph(), edit_rules())).pack(side="left", padx=3)
    ttk.Button(actions, text="Start Run", command=begin_run).pack(side="left", padx=3)
    ttk.Button(actions, text="Next Event ▶", command=do_step).pack(side="left", padx=3)
    ttk.Button(actions, text="Run / Pause", command=auto_run).pack(side="left", padx=3)
    ttk.Button(actions, text="Run Batch", command=run_batch_gui).pack(side="left", padx=3)
    ttk.Button(actions, text="Reset Graph", command=reset_graph).pack(side="left", padx=3)
    ttk.Button(actions, text="Save Preset", command=save_preset).pack(side="left", padx=(16, 3))
    preset_var = tk.StringVar()
    preset_box = ttk.Combobox(actions, textvariable=preset_var, width=31, state="readonly")
    preset_box.pack(side="left", padx=3)
    ttk.Button(actions, text="Load", command=load_preset).pack(side="left", padx=3)
    ttk.Button(actions, text="Save Batch Script", command=save_script_manifest).pack(side="left", padx=3)

    def update_summary():
        graph = state["run"].graph if state.get("run") else state["graph"]
        counts = pattern_counts(graph)
        content = [f"Nodes: {len(graph)}", f"Connections: {len(list_edges(graph))}",
                   f"Connected components: {graph_components(graph)}", "", "Graph motifs (counts):"]
        content.extend(f"  {key}: {value}" for key, value in counts.items() if key not in ("node_count", "edge"))
        content.append(f"  edges: {counts['edge']}")
        content.extend(["", "World model: adjacency only", "No node positions or fields."])
        summary_text.configure(state="normal")
        summary_text.delete("1.0", "end")
        summary_text.insert("1.0", "\n".join(content))
        summary_text.configure(state="disabled")

    def show_event_detail(_event=None):
        selection = event_list.curselection()
        detail_text.configure(state="normal")
        detail_text.delete("1.0", "end")
        if selection and selection[0] < len(state["events"]):
            detail_text.insert("1.0", json.dumps(state["events"][selection[0]], indent=2))
        detail_text.configure(state="disabled")

    def draw_graph():
        graph = state["run"].graph if state.get("run") else state["graph"]
        canvas.delete("all")
        width = max(100, canvas.winfo_width())
        height = max(100, canvas.winfo_height())
        nodes = sorted(graph)
        if not nodes:
            return
        cx, cy = width / 2, height / 2
        radius = max(40, min(width, height) * 0.34)
        if state["view"] == "2D":
            positions = {node: (cx + radius * math.cos(2 * math.pi * i / len(nodes) - math.pi / 2),
                                cy + radius * math.sin(2 * math.pi * i / len(nodes) - math.pi / 2))
                         for i, node in enumerate(nodes)}
        else:
            positions = layout3d(graph)
            yaw, pitch = state.get("rotation", (0.45, 0.28))
            projected = {}
            for node, (x, y, z) in positions.items():
                x1 = x * math.cos(yaw) - z * math.sin(yaw)
                z1 = x * math.sin(yaw) + z * math.cos(yaw)
                y1 = y * math.cos(pitch) - z1 * math.sin(pitch)
                scale = radius / (2.2 + (z1 * math.cos(pitch) + y * math.sin(pitch)) * 0.25)
                projected[node] = (cx + x1 * scale, cy + y1 * scale)
            positions = projected
        for a, b in list_edges(graph):
            x1, y1 = positions[a]
            x2, y2 = positions[b]
            canvas.create_line(x1, y1, x2, y2, fill="#516071", width=2)
        for node, (x, y) in positions.items():
            canvas.create_oval(x - 18, y - 18, x + 18, y + 18, fill="#1d716b",
                               outline=accent, width=2)
            canvas.create_text(x, y, text=node, fill="white", font=("Segoe UI", 10, "bold"))
        canvas.create_text(12, 12, anchor="nw", text=f"{state['view']} DISPLAY LAYOUT · not graph data",
                           fill=accent, font=("Segoe UI", 9, "bold"))

    def rotate_view(event):
        previous = state.get("drag")
        if previous and state["view"] == "3D":
            dx, dy = event.x - previous[0], event.y - previous[1]
            yaw, pitch = state.get("rotation", (0.45, 0.28))
            state["rotation"] = (yaw + dx * 0.008, max(-1.4, min(1.4, pitch + dy * 0.008)))
            draw_graph()
        state["drag"] = (event.x, event.y)

    def layout3d(graph):
        # Temporary deterministic display layout; never written into graph or presets.
        nodes = sorted(graph)
        count = len(nodes)
        positions = {}
        for i, node in enumerate(nodes):
            digest = hashlib.sha256(node.encode()).digest()
            u = int.from_bytes(digest[:4], "big") / 2**32
            v = int.from_bytes(digest[4:8], "big") / 2**32
            z = 2 * u - 1
            angle = 2 * math.pi * v
            radial = math.sqrt(max(0.0, 1 - z * z))
            positions[node] = [radial * math.cos(angle), radial * math.sin(angle), z]
        # A few force-layout passes use only graph adjacency; these are view coordinates.
        for _ in range(28):
            delta = {n: [0.0, 0.0, 0.0] for n in nodes}
            for i, a in enumerate(nodes):
                for b in nodes[i + 1:]:
                    d = [positions[a][k] - positions[b][k] for k in range(3)]
                    dist2 = max(0.01, sum(x * x for x in d))
                    force = 0.035 / dist2
                    for k in range(3):
                        delta[a][k] += d[k] * force
                        delta[b][k] -= d[k] * force
            for a, b in list_edges(graph):
                d = [positions[a][k] - positions[b][k] for k in range(3)]
                dist = max(0.001, math.sqrt(sum(x * x for x in d)))
                force = (dist - 0.9) * 0.035 / dist
                for k in range(3):
                    delta[a][k] -= d[k] * force
                    delta[b][k] += d[k] * force
            for node in nodes:
                for k in range(3):
                    positions[node][k] += max(-0.08, min(0.08, delta[node][k]))
        return {node: tuple(value) for node, value in positions.items()}

    # Bind callbacks after their definitions; screen layout remains presentation-only.
    canvas.bind("<ButtonPress-1>", lambda e: state.update(drag=(e.x, e.y)))
    canvas.bind("<B1-Motion>", rotate_view)
    event_list.bind("<<ListboxSelect>>", show_event_detail)
    refresh_presets()
    update_summary()
    draw_graph()
    root.after(200, poll_batch)
    root.mainloop()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Nodes & Connections graph-only experiment toy")
    parser.add_argument("--gui", action="store_true", help="open the interactive Tk window")
    parser.add_argument("--headless", action="store_true", help="run without opening a window")
    parser.add_argument("--batch", type=Path, help="JSON experiment batch manifest")
    parser.add_argument("--output", type=Path, default=RUN_DIR, help="directory for run journals")
    args = parser.parse_args(argv)
    if args.batch:
        run_batch(args.batch, args.output)
        return 0
    if args.gui or not args.headless:
        try:
            make_gui()
        except Exception as exc:
            print(f"Could not start GUI: {exc}", file=sys.stderr)
            return 1
        return 0
    parser.print_help()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
