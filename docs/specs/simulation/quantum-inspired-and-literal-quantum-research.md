# Quantum-inspired primitives and literal-quantum research

Status: research branch. Not a gameplay requirement. Not implemented.
Scope: separate the practical primitive-state emergence model from speculative
literal-quantum simulation.

## Decision

- Main engine direction: **quantum-inspired discrete primitive state** plus
  deterministic event propagation. Spin/up/down are orientation/state bits, not
  amplitudes.
- Literal quantum amplitudes, superposition, measurement, and decoherence are a
  separate research branch and do not enter authoritative gameplay in this
  pass.

## Why

Authoritative multiplayer needs deterministic, reproducible, low-latency state.
Literal quantum simulation would add probabilistic measurement and decoherence
semantics that do not map cleanly to server authority, rollback, or replay.
It remains an interesting toy/research direction, not a current engine
foundation.

## Discrete branch (main)

Primitive state per entity:

- stable ID;
- position/velocity;
- orientation and up vector;
- spin;
- mass;
- health;
- team/owner;
- alive/dead;
- held action/item;
- discrete bits (grounded, up/down, hostile, supported).

Local transition rules and a deterministic event queue produce movement,
contact, support, push, damage, death, NPC target changes, projectile/explosion,
objective, and GUI/audio projections.

Acceptance is defined in `time-and-ordering.md`: one impulse must produce a
traceable causal chain, and the same primitive state plus event order must
reproduce the macro outcome.

## Literal-quantum branch (separate)

A feasibility document/toy only. It must define:

- state amplitudes versus discrete state;
- measurement and decoherence policy;
- authority and determinism for multiplayer;
- numerical cost and stability;
- how results are compared to the discrete baseline.

It may not be used to claim current gameplay behavior, and it may not replace
server authority without a separate specification and human decision.

## Relationship to existing architecture

- ECS components are the practical primitive-state container.
- `docs/specs/architecture/nodes-and-connections.md` provides the graph/time
  projection ontology.
- `docs/architecture/live-development/hot-kernel.md` keeps behavior hot while
  the kernel owns stable mechanism.
- This research does not authorize an engine rewrite or a second runtime.

## Open questions

- Which primitive attributes are truly necessary versus derived?
- How much uncertainty/confidence should be represented explicitly?
- Can a literal-quantum toy produce any useful engineering insight without
  destabilizing authority?
- What is the smallest toy that demonstrates propagation into NPC behavior,
  rocket explosion, collision, vehicle response, and GUI?
