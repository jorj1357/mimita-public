// 2026-11-20
/* purpose
* define the target node/connection ontology for project, agent, code, and runtime knowledge
* make dimensions, time, 3D, code, and agent context projections of one graph
* this file DOES NOT authorize an engine rewrite or a second source of truth
* this file DOES NOT replace existing ECS, physics, packet, or coordinator owners
*/

# Nodes and connections

## Status and authority

This is a **target architecture** and a phased research direction. It is not a
claim that the engine, the coordination brain, or the source tree has already
been rewritten as a graph.

The current owners remain authoritative:

- live gameplay state: existing ECS/actor/world owners;
- contact and damage: [contact authority](../networking/contact-authority.md);
- agents, issues, decisions, and leases:
  [agent coordination](../dev-loop/agent-coordination.md);
- evidence and proof:
  [agent proof and verification](../dev-loop/agent-proof-and-verification.md);
- build/test latency:
  [change-to-evidence latency](../dev-loop/change-to-evidence-latency.md).

This specification defines how those owners may expose a common graph view.
It does not create a second runtime, a second issue tracker, or a second source
of truth.

## Core idea

The canonical model is:

```text
nodes + typed connections + attributes + provenance + version
```

There is no inherent “space” or “time” in the model. Space, time, identity,
hierarchy, code structure, 3D layout, and UI layout are **projections** derived
from node attributes and edge types.

```text
one graph
  -> list view
  -> 2D board
  -> 3D scene
  -> timeline
  -> code/dependency view
  -> agent context bundle
```

Changing the projection changes presentation, not truth. Switching dimensions
must never mutate the underlying graph unless the user explicitly edits a fact.

## Node

A node is a stable identity plus typed attributes and provenance.

```text
node
  id: stable graph id
  kind: actor | weapon | contact | action | packet | event | file | symbol |
        spec | issue | task | test | build | run | agent | decision | note |
        asset | runtime | metric
  attributes: typed key/value data
  provenance: source authority, source path/run, confidence
  version: immutable revision or content hash
  lifecycle: proposed | active | stale | superseded | archived
```

Examples:

- actor node, weapon node, contact node, damage event node;
- source file node, symbol node, header node, packet schema node;
- spec section node, issue node, decision node, test node, proof node;
- build generation node, runtime node, agent node, lease node.

A node does not contain a hidden second copy of an authoritative value. A
projection may cache a value, but the source authority remains identified.

## Connection

A connection is a typed, directed edge with attributes and provenance.

```text
connection
  from: node id
  to: node id
  kind: owns | depends_on | includes | implements | specifies | validates |
        produces | observes | predicts | rejects | supersedes | contradicts |
        located_at | before | after | occurs_at | coordinates_with |
        assigned_to | blocked_by
  attributes: typed key/value data
  provenance: source authority, confidence
  version
```

Connections are first-class facts, not comments or duplicated fields. For
example:

- `contact --produced_by--> actor`
- `contact --targets--> actor`
- `contact --validated_by--> server resolver`
- `contact --damages--> actor`
- `contact --occurs_at--> tick`
- `file --depends_on--> header`
- `issue --specified_by--> spec section`
- `issue --tested_by--> test`
- `decision --supersedes--> decision`
- `agent --coordinates_with--> agent`

## Projections

### Space

Space is not a stored global coordinate requirement. Spatial facts are edges and
attributes such as `located_at`, `near`, `inside`, `contacts`, or a transform
attribute. A 3D view is a projection of those facts. Actors, contacts, effects,
maps, and debug overlays can share a spatial projection without storing a second
world.

### Time

Time is not an inherent node dimension. Temporal facts are edges and attributes:
`before`, `after`, `occurs_at`, `contact_tick`, `server_tick`, `build_time`,
`observed_at`. A timeline is a projection. Event ordering, causality, and
rewind windows are queries over temporal edges, not a second clock.

### 3D

3D is a view over a selected node set and spatial connections. A contact graph,
actor graph, or code graph may be laid out in 3D when that projection makes the
relationship clearer. The 3D view must declare:

- which node kinds and edge kinds are visible;
- which attributes map to position, orientation, scale, and time;
- which layout algorithm is used;
- whether layout is deterministic and reproducible;
- which facts are hidden by the projection.

### Live dimension switching

The user and agents may switch:

```text
list <-> graph <-> 2D board <-> 3D scene <-> timeline <-> code view
```

Rules:

- switching is read-only unless an explicit edit command is issued;
- the selected subgraph and query survive the switch;
- the view records its projection id and parameters;
- switching must not create duplicate facts or change semantic identity;
- hidden facts remain queryable and must be reported as hidden;
- round-trip view consistency is testable.

### Code

C++ source, headers, symbols, includes, dependency edges, packet schemas, build
inputs, objects, DLLs, tests, and runtime owners form a code graph. The source
text can remain the authoring surface while the graph provides:

- dependency and fan-out queries;
- duplicate/special-path detection;
- unused file/symbol candidates;
- ownership and ABI impact;
- build impact and test selection;
- architecture and migration views.

The graph does not immediately replace the C++ compiler. C++ is treated as one
projection/authoring language whose structure can be represented as nodes and
edges. Future graph-native or generated code is a research experiment, not a
current requirement.

## Agent knowledge and coordination

The shared brain already owns agents, tasks, issues, notes, decisions, leases,
messages, and evidence. The graph view adds relationships without replacing
those records:

- `issue --specified_by--> spec section`
- `issue --owned_by--> agent`
- `issue --tests--> test`
- `issue --proof--> run/journal`
- `decision --resolves--> issue`
- `lesson --prevents--> repeated issue`
- `agent --works_on--> issue`
- `agent --edits--> file/region`
- `agent --waits_for--> build/runtime/event`

An agent context bundle becomes a bounded subgraph:

```text
issue + active decisions + relevant spec sections + owner paths
+ tests/metrics + recent evidence + related issues + agents/leases
```

This is the intended replacement for unbounded document dumping. The graph query
must report provenance and confidence so an agent can distinguish a human
report, a spec requirement, a runtime observation, and an inference.

## Live graph editing

An agent or human may propose graph edits:

- add/update a node or connection;
- mark a fact stale or superseded;
- attach evidence and confidence;
- propose a projection or layout;
- move an issue/task relationship.

Edits use the existing lease/decision/review rules. A graph edit that changes
behavior, protocol, or authority is not a normal view edit; it follows the
normal spec and human-review path.

## Scientific method

Graph changes are measured, not assumed:

```text
hypothesis
baseline query/context/build/test latency
one graph or projection change
same pinned inputs
result: better / worse / neutral / inconclusive
keep or revert
regression if worse
```

Useful metrics:

- context bytes/tokens and precision for an issue;
- time to find owner/spec/test;
- duplicate and unused-code findings accepted/rejected;
- query latency;
- projection switch latency;
- cross-projection consistency failures;
- time-to-evidence and build/launch waits;
- recurrence of duplicate/confusing-path issues.

## Phased roadmap

1. **Read-only graph schema and import**
   - import existing issues, specs, decisions, tests, builds, and ownership;
   - no gameplay/ECS change.

2. **Context queries**
   - bounded subgraph for an issue;
   - owner/spec/test/evidence paths;
   - compare result with current `observe`.

3. **Board and timeline projections**
   - 2D issue/connection board;
   - build/test/runtime timeline;
   - live view state without semantic mutation.

4. **Spatial/contact projection**
   - project actors and contacts into a test-only 3D view;
   - compare with current contact IDs and events.

5. **Code/dependency graph**
   - import source, headers, symbols, dependencies, packets, and build objects;
   - query duplicate paths, unused files, fan-out, and test selection.

6. **Live dimension switching**
   - same query/subgraph across list, 2D, 3D, timeline, and code views;
   - projection metadata and round-trip tests.

7. **Graph-native experiments**
   - generated views or code from a bounded graph slice;
   - never claim a full engine or language replacement without proof.

## Safety boundaries

- No second source of truth.
- No silent graph mutation from a view.
- No behavior/protocol change from a layout or projection.
- No unbounded graph dump into a prompt.
- No performance-critical runtime path moved into a graph engine without
  measured budgets.
- No replacement of the compiler, ECS, physics, packet authority, or dev-loop
  without a separate reviewed specification.
- No graph node is trusted without provenance and confidence.
- Human review remains required for shared authority, protocol/ABI, production,
  and subjective acceptance.

## Acceptance

- A fact can be found through a list, 2D, 3D, timeline, or code projection
  without changing its identity.
- Switching projection is read-only and reports hidden/filtered facts.
- Agent context for an issue is a bounded subgraph with provenance.
- The same issue/spec/decision/test relationships are not duplicated across
  Markdown, brain tables, and graph storage as competing truths.
- Duplicate, hardcoded, unused, and confusing paths are discoverable by graph
  queries and linked to reviewable issues.
- The graph remains a view/relationship layer until a separate implementation
  proves otherwise.
