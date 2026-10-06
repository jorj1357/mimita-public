# NPC navigation implementation contract

Time: 2026-10-06 14:34:35 EDT
Branch: current working tree

## Outcome

PASS_WITH_HUMAN_REVIEW

Added a detailed implementation contract for NPC navigation and decision
making. The document is intended to be read by both humans and AI
contributors before they modify the Counter-Strike NPC behavior or its
navigation backends. It now includes Phase 0 integration readiness with the
project owner's decisions about offline/runtime navmeshes, static/dynamic
worlds, actor-derived geometry, bounded dynamic updates, server authority,
per-actor traversal capabilities, staged fallback, build ownership, and the
first `dust2cyberiav4` acceptance thresholds.

## Files changed

- `docs/architecture/player-npc-systems/npc-navigation-implementation.md`:
  added the implementation contract, including ownership boundaries, Phase 0
  integration readiness, backend selection, Recast/Detour/RVO2
  responsibilities, navigation and traversal schemas, strategic and tactical
  decisions, movement arbitration, progress recovery, authority/determinism,
  diagnostics, collaboration rules, implementation phases, runtime scenarios,
  `dust2cyberiav4` acceptance thresholds, deletion gates, and unresolved
  human decisions.
- `docs/changelog/2026-10-06/20261006_143435-npc-navigation-implementation-contract.md`:
  this session record.

## Relationship to existing specifications

The new contract is an implementation-facing companion to:

- `docs/features/gamemodes/counterstrike.md`, the Counter-Strike behavior
  specification.
- `docs/architecture/player-npc-systems/npc-movement.md`, the NPC movement
  architecture and navigation-library direction.
- `docs/specs/20261006plan.md`, the current investigation and migration plan.

It does not replace those documents, silently resolve their open TODOs, or
claim that the proposed Recast/Detour migration is implemented.

## Evidence

- Documentation route and focused documentation-review instructions were
  read before editing.
- The new document exists at the requested architecture path, contains 1,652
  lines and has no trailing-whitespace lines.
- No C++ source, configuration, build output, executable, or runtime state was
  changed for this documentation task.
- No build or gameplay runtime validation was performed because no executable
  behavior was changed.

## Review boundary

The repository already contained unrelated modified and untracked files;
they were preserved. The new contract deliberately leaves decisions such as
backend rollout mode, map-bake ownership, dynamic-obstacle policy, traversal
metadata completeness, and acceptance thresholds for explicit human review
instead of inventing project policy.
