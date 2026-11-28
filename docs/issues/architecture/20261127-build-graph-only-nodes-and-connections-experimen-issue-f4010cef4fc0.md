# issue-f4010cef4fc0: Build graph-only nodes-and-connections experiment GUI v1

## Human report (verbatim, immutable)

> can u make the gui and run it  the v1  so i can see and then explainw aht we should add? we should add like press butotn to step forward, lots of fiedls to edit, save preset adn it auto save the preset like iso 8601 time name it optional but it should be quick and lots of diffrent presets u can save\n\nand how to relate with AI so ai cna make scripts that tests aa lot of thingstha\n\nhow to  describe 3d space with nods and conenctions i want to do that\n\nget a stalbe pattern that persists across time if time is events and stuff\n\nand then somehow it emerges 3d space not sure


## Review (partial implementation and batch evidence; manual GUI interaction pending)

### 2026-11-27T22:57:27.418Z — codex-node-toy-v1

Reviewer: codex-node-toy-v1

OBSERVED: Added `devscripts/emergence-toy.py`, a standalone Tk GUI and headless graph rewrite runner. The model uses adjacency sets only; opaque node references, rules, presets, event logs, and display layouts are separate experiment/tool data. The GUI has graph/rule editors, Apply Edits, Start Run, Next Event, Run/Pause, Run Batch, Reset Graph, Save Preset, Load, Save Batch Script, and a display-only 2D/3D selector. Presets use UTC ISO-like filename stamps and optional labels. Added the tool to `docs/operations/devscripts/script-inventory.md`.

OBSERVED: GUI window `Nodes & Connections Lab — v1` launched. Computer Use inspection was stopped by the user with physical Escape before any button input; the screenshot showed the existing MiMITA game window in front of part of the toy. Thus control behavior and human visual acceptance remain pending.

OBSERVED: Headless batch manifest `.dev/emergence-toy/first-pattern-batch.json` produced 32 runs (four rule variants × eight seeds), 392 parsed journal events. Every `caused_by` reference in each run resolved to an event ID in that run. Pattern summary: edges present in 32/32 runs across 4 rule sets; open paths in 24/32 across 3 rule sets; triangles in 16/32 across 2 rule sets. Batch: `.dev/emergence-toy/runs/experiment-2026-11-27T22-55-51.038387Z/batch-summary.json`. Sample journal: `.dev/emergence-toy/runs/experiment-2026-11-27T22-55-51.038387Z/experiment-2026-11-27T22-55-51.038387Z-run-01-seed-0/events.jsonl`.

LIMITS: This demonstrates recurrence of simple motifs in this finite rewrite toy only. It does not show physical 3D emergence, a field, or elapsed time. Causal edges define local event order; JSONL line order is logging order. The force-layout view is a deterministic drawing only.

NEXT: User brings the already-open GUI to the foreground and visually tries Next Event, Save Preset/Load, and Run Batch. Keep this issue/task open until that interaction is observed; then explain which experiment controls or rule/pattern concepts to add next.

## Agent suggestion

### 2026-11-27T23:01:16.516Z — rocket-league-match

OBSERVED (rocket-league-match, independent): v1 is live — PID 28276 running devscripts/emergence-toy.py --gui; headless batch artifact exists with 32 runs / 392 events and recurrence edge 32/32 (4 rule sets), open_path 24/32 (3), triangle 16/32 (2). No presets saved yet. I did not edit or relaunch the owner's work.

PROPOSAL for v2 increments (owner decides):
1) Parameter panel + autosave: expose max_events, seeds, match_policy, and rule weights as real fields; debounce-save a preset on every Apply/Step (ISO-8601 filename already works; keep label optional and non-destructive).
2) Replay/scrub: because events are append-only with caused_by, add back/step-to-event that rebuilds state by replay, plus a timeline slider and highlight of graph_delta.
3) Stable-pattern metric: persist a persistence score (fraction of steps a motif survives) and conserved-quantity panel, not just per-step counts; that is the measurable answer to 'pattern that persists across time'.
4) AI scripting hook: document the manifest + events.jsonl schema and add a headless pass/fail mode (expected motif counts / invariants / max steps) so an AI writes a manifest, runs it, and reads JSON. Avoid a bespoke DSL until the manifest is exhausted.
5) 3D-space honesty: layout is display-only. To claim emergent space, measure locality/dimension of the graph (neighbor-distance vs graph-distance correlation, intrinsic dimension estimate) across seeds, and only then compare to a projection. Do not call force layout 'space'.

LIMIT: these are design proposals; no runtime proof of persistence or 3D emergence exists yet.

## Agent suggestion

### 2026-11-27T23:01:58.851Z — rocket-league-dodge

OBSERVED (rocket-league-dodge, 2026-11-27T23:00Z): The v1 GUI already exists as devscripts/emergence-toy.py; PID 28276 was still running with window title Nodes and Connections Lab v1 and was brought to the foreground for user review. A fresh headless batch run produced 32/32 journals and batch-summary.json under .dev/emergence-toy/runs/experiment-2026-11-27T23-00-42.764839Z/. Runner and journal write path work. Human visual/interaction acceptance of Next Event, Save Preset, and Run Batch is still pending and must not be claimed.

V1 TODAY: adjacency-only undirected graph, two rule patterns (open_path to connect_ends, edge to extend_second/remove_edge), manual Next Event, Run/Pause, headless batches with seeds, ISO-8601 filename presets (manual save), display-only 2D/3D layout, append-only events.jsonl with explicit caused_by.

PROPOSED NEXT (in priority order, not yet implemented):
1. Point-and-click graph editing on the canvas: click to add node, drag to connect, right-click to delete, while keeping the adjacency text as the authoritative editor. This directly addresses lots of fields to edit.
2. Typed node/edge fields plus provenance and version (spec: nodes + typed connections + attributes + provenance + version). V1 stores adjacency only, so lots of fields needs a field editor and per-node/edge attribute panels. Keep attributes in graph state; keep x/y/z strictly out (display only).
3. Auto-save presets: debounce-edit autosave plus an explicit save button, unlimited ISO-8601 stamped presets with optional label, and a preset gallery/preview so many presets stay usable. V1 only saves when clicked.
4. Event timeline controls: step forward (exists), step back via snapshots, jump-to-event, and a scrollable causal DAG view; time stays the caused_by DAG, not file order.
5. AI scripting surface: document the JSON manifest schema and expose a stable importable API (for example from emergence_toy import run_batch) so an AI can generate many test scenarios, sweep rule/seed sets, and diff results. The v1 --batch is the start; it needs a scenario-script contract and a machine-readable comparison report.
6. Stable pattern persists across time metric: replace raw motif counts with a survival/persistence measure (motif present across N rewrites and seeds, plus survival curve and variance), so stable is falsifiable rather than eyeballed.
7. 3D space emerges research slice: two distinct deliverables. (a) 3D is a display projection (draft exists). (b) Test whether distance is recoverable from graph structure: compute shortest-path/graph distances, embed via MDS, and measure reproducibility across seeds. If the embedding is stable, that is evidence of a consistent geometric projection, not proof that physical 3D emerged. No claim beyond the measured result.

LIMITS: v1 shows recurrence of simple motifs in a finite rewrite toy. It does not show physical 3D, fields, or elapsed time. Force-layout coordinates are drawing-only and must stay out of presets/graph state.

NEXT: user visually exercises the foregrounded window; then pick the smallest of items 1-7 to implement under this issue.

## Agent suggestion

### 2026-11-27T23:02:00.240Z — rocket-league-crashfix

OBSERVED (rocket-league-crashfix): ctx-65935de46132 was also woken to rocket-league-crashfix/-dodge/-match although issue-f4010cef4fc0 is already owned by codex-node-toy-v1. I did not duplicate the implementation. The running v1 GUI (PID 28276, 'Nodes & Connections Lab - v1') had become hidden; I restored and raised its window so the user can see it. NEXT: owner should drive the manual button pass (Next Event, Save/Load Preset, Run Batch) and collect the user's v2 requests.

## Review (evidence-gap)

### 2026-11-28T03:03:39.255Z — daily-review-20261128

Reviewer: daily-review-20261128

Daily review: linked user context ctx-df... to the parent ontology issue. The graph-only v1 snapshot reports 32 batch runs and 392 valid events, but the GUI was not manually exercised and the results are limited to recurrence in the finite rewrite system. They do not establish physical space/time, decentralized storage, secure persistence, or cross-device collaboration. Retain this scope boundary; useful next proof is user interaction with the GUI plus a ground-truth graph experiment with stated position/time error metrics.
