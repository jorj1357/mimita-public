# Nodes, space, time, and 3D experiment proposal

## Request

Explore a 3D GUI and experiments for recovering space and time from nodes and connections, before introducing fields.

## Existing contract reviewed

- `docs/specs/architecture/nodes-and-connections.md` defines one graph with typed edges, attributes, provenance, and version; space, time, and 3D are read-only projections. Its roadmap places a test-only spatial/contact view before broader live dimension switching or graph-native work.
- `docs/specs/simulation/time-and-ordering.md` says current gameplay remains fixed at 60 Hz and calls for a deterministic toy world before shadow comparison with real event chains.
- Existing architecture issue: `issue-75220121d993`.

## Proposed first experiment

Use the five-node adjacency example as topology only. Attach explicit event facts (stable IDs, edge kinds, causal parent, logical step, and provenance) and display the same selected graph in a 3D scene, event timeline/scrubber, and node/edge inspector. Support pause, single-step, deterministic replay, and a saved run record. Version and expose projection/layout parameters and filters.

The adjacency list alone contains no physical coordinates, distances, directions, or timestamps. Its BFS depth illustrates graph distance and its cause chain illustrates event ordering; neither establishes physical 3D location or continuous time. Label generated layout separately from observations. For metric reconstruction, generate known 3D ground-truth points, expose edge lengths and enough anchors, then measure error. For time reconstruction, provide event timestamps or a declared ordering model.

### Hypothesis

Given identical versioned graph facts and projection settings, switching scene/timeline/list views preserves node and edge identity and deterministic replay. When observations sufficiently constrain a synthetic world, recovered positions and event times meet predeclared error tolerances.

### Measurements

- position error after rigid alignment;
- pairwise-distance error;
- event-order accuracy;
- graph identity consistency after projection switching;
- replay determinism and query/projection latency.

Persist seed, graph/input hash, ground truth, observed and inferred values, projection ID/version and parameters, errors, and elapsed time. Run ablations that remove edge-distance or time observations to show which reconstruction claims cease to be identifiable.

Only after this is repeatable, explore a scalar/vector field as a derived query over primitive node/edge values and neighborhoods, with explicit interpolation and provenance. Do not make it a new authoritative source of truth. The next implementation step should be a separate bounded task under the architecture issue; no code, GUI, build, or runtime experiment was performed in this session.

## Tracking

Submitted the experiment proposal to `issue-75220121d993` using the coordination issue discussion. Existing ownership and proof gaps on that issue remain for its owner to resolve; this proposal does not close the issue or claim implementation/proof.
