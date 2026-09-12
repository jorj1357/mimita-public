# Gold reference: the live-code hot-reload journey and lessons

- EST timestamp: 2026-09-12 13:48:26 EDT (UTC 2026-09-12T17:48:26Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS_WITH_HUMAN_REVIEW` (documentation; runtime proof already observed by the developer)

## Task

Write a gold reference document explaining the journey from "kill the exe over
and over to rebuild and recompile C++" to the working first artifact where
editing `src/hot-reload/modules/rocket-behavior.cpp` changes live rocket damage
against an NPC, plus the thought process, what helped in the repo, what could
have made it easier, how to expand, and how to resolve the "design general from
the start vs get it working first" tension.

## Changes

- New `docs/gold/2026-09-12-live-code-hot-reload-journey.md`:
  - the first working artifact (developer-observed live rocket damage change);
  - where the project started (effect-only `GameAPI` v2, blocking mtime reload,
    `build_agent.py` taskkill, cold-only authoritative damage);
  - nine milestones grounded in the repository;
  - the "entity stays / behavior changes / universe does not restart" model;
  - the thought process (preserve behavior first, one owner per concept, visible
    failures, invariants not tuned constants, sacred running process,
    independent per-process state);
  - what helped in the repo;
  - what could have made it easier (general seam first, no cold call sites per
    feature, process identity and per-process build paths from day one, explicit
    retry, per-process lifecycle checklist);
  - the "get it working behind a seam, then generalize the seam" resolution;
  - expansion order linking to `hot-kernel-next-steps.md`;
  - evidence and status.
- `docs/architecture/live-development/hot-kernel.md`: linked the gold doc from
  its Related section.

## Evidence

- Documentation-only change; no code changed.
- Build/self-tests unchanged from the previous session and still passing
  (`--live-code-selftest`, `--hot-authoritative-selftest`,
  `--entity-slice-selftest`).
- The working artifact itself is the developer's live in-game observation, not a
  self-test: editing `rocket-behavior.cpp` changed rocket damage against an NPC
  with the game running.

## Human review / status

- The document records the developer's observed acceptance as the first
  artifact.
- The latest observability/retry/protocol round still needs its one bootstrap
  cold build and running-server proof (see the previous changelog:
  `20260912_173242-live-reload-per-process-retry-observability.md`).

## Documentation TODOs noted (not edited, per router)

- `docs/architecture/ecs-entity-etc/ecs.md:1-4` — "todo explain ecs". Suggestion:
  add a short "what ECS means here" section that states the vocabulary (entity,
  component, event, behavior, capability) and links `ecs-migration.md` and
  `hot-kernel.md`.
- `docs/features/README.md:29-35` — informal todo about explaining feature-record
  workflow. Suggestion: add two sentences on "write the desired behavior first,
  then link tests/evidence", which the live-code feature record already models.
- `docs/architecture/player-npc-systems/npc-movement.md` — six-line stub.
  Suggestion: describe the shared NPC movement path and its relation to the
  hot actor module.

## Files

New: `docs/gold/2026-09-12-live-code-hot-reload-journey.md`.
Changed: `docs/architecture/live-development/hot-kernel.md`.

## Pre-existing edits preserved

Unrelated working-tree changes were not reverted or claimed.
