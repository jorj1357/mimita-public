# Field toy experiment GUI design

## Summary

Expanded the Emergence Toy v1 spec with a small field set, AI/human experiment loop, live `events.jsonl` envelope and event kinds, switchable 2D/3D screen layout, run invocation contract, and reuse boundary for MiMITA's Tk dashboard.

## Evidence reviewed

- `docs/specs/dev-loop/dashboard.md` defines a standalone Python/Tk view over dev-loop state and separates presentation from the daemon's source of truth.
- `docs/specs/dev-loop/dev-loop.md` defines MiMITA's dev-loop as build/run authority and identifies runtime `events.jsonl` as evidence.
- `devscripts/dev_loop/gui.py:710` exposes a GUI `run(status_provider, control, title, ...)`; its window, refresh, and widgets are nested into the dashboard implementation and controls are tied to checkout/build/runtime operations.
- `docs/specs/gui/guiv2.md:656` supports appropriate 2D and 3D presentation without a second in-game GUI architecture. The proposed research lab is a standalone Python tool, not an in-game GUI.

## Design decision

Use one standalone Python/Tk research window and one experiment runner. Reuse dashboard interaction and style patterns where useful; do not route the toy through dev-loop daemon controls. Each toy run writes its own append-only `events.jsonl`; the GUI tails it and updates the scene, timeline, and results. Human and AI proposals share the runner. Batch jobs can run headlessly through the same runner.

The minimal dense-cluster experiment uses explicit 3D positions, per-cell scalar density, and neighbor links with weights/distances. Optional activity/energy, flow vectors, and destruction stress/strength are declared only when a run needs them. The user-facing meaning of "entropy" remains a chosen distribution statistic until a physical mapping is defined.

## Implementation status

No Python runner or GUI was implemented or launched. The command `python devscripts/emergence-toy.py --gui` is a proposed contract only. No build or test was requested or run.

The expanded spec is at `C:\Users\guita\.codex\visualizations\2026\11\27\01a21f16-1286-7901-95bb-2244c375c7ec\spec-emergence-toy-v1.md`.
