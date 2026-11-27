# Graph-only nodes and connections toy v1

## User goal

Build and run a visible Python GUI for the graph-only v1. Support one-event stepping, editable adjacency and rewrite rules, quick timestamped presets with optional labels, batch experiments that AI/scripts can author, and an exploratory 2D/3D graph view. Keep node data to adjacency only; event causality is the modeled order.

## Changes

- Added `devscripts/emergence-toy.py`: graph parser, adjacency-only rewrite runner, Tkinter GUI, append-only per-run `events.jsonl`, timestamped preset save/load, JSON batch CLI, live batch event feed, and display-only 2D/3D layouts.
- Rules match `open_path` or `edge` and rewrite by connecting path ends, extending an edge, or removing an edge. Controls include Apply Edits, Start Run, Next Event, Run/Pause, Run Batch, Reset Graph, Save Preset, Load, and Save Batch Script.
- Added the research tool to `docs/operations/devscripts/script-inventory.md`.

## Run evidence

- GUI launched as `Nodes & Connections Lab — v1` and remained open. Computer Use inspection was stopped by the user with physical Escape before any GUI button interaction; the observed desktop had a MiMITA game window covering part of the new window. No further UI actions were issued. Human visual acceptance of the buttons and interaction remains pending.
- Headless batch manifest: `.dev/emergence-toy/first-pattern-batch.json`.
- Batch run: `.dev/emergence-toy/runs/experiment-2026-11-27T22-55-51.038387Z`.
- 32 runs across four rule variants and eight seeds produced 392 valid JSONL events. All causal-parent references resolved within their run.
- Pattern recurrence summary: edges in 32/32 runs across 4 rule sets; open paths in 24/32 across 3; triangles in 16/32 across 2. Full summary: `.dev/emergence-toy/runs/experiment-2026-11-27T22-55-51.038387Z/batch-summary.json`.
- Sample journal: `.dev/emergence-toy/runs/experiment-2026-11-27T22-55-51.038387Z/experiment-2026-11-27T22-55-51.038387Z-run-01-seed-0/events.jsonl`.

## Limits and next step

The toy demonstrates recurring graph motifs in this finite rewrite system. It does not demonstrate physical 3D space, fields, physical entropy, or elapsed time. The 2D/3D views are display layouts. Event causality defines local order; JSONL line order is only write order. Ask the user to bring the still-open GUI to the foreground and try Next Event, Save/Load Preset, and Run Batch. Keep the implementation issue open until manual interaction and requested design follow-up are complete.
