# FFA feature record template migration

Branch: `8292026stash`
Commit: working tree; no commit created
Timestamp: `2026-09-09T17:56:54Z`

## Scope

Rewrote `docs/features/gamemodes/ffa mode issues.md` to follow
`docs/features/TEMPLATE.md`. This was documentation-only; no gameplay source,
configuration, tests, or runtime behavior was changed.

## Pre-existing changes preserved

Before this session, the working tree already contained changes to
`docs/features/TEMPLATE.md`, `docs/features/camsway-realisticish/camsway.md`,
and an untracked changelog file
`docs/changelog/2026-09-09/20260909_175616-camsway-feature-record.md`. These
were not modified or claimed as part of this task.

## File changed

`docs/features/gamemodes/ffa mode issues.md` was replaced from a free-form
issue list with the template sections:

- Purpose
- Desired behavior
- Current behavior
- Current status
- Decisions, including one clearly marked `NEEDS_SPEC_DECISION`
- Ownership
- Related authoritative documents
- Relevant files
- Tests and evidence
- Acceptance criteria
- Changelog and regression links

The original reported issues were preserved and clarified: inconsistent NPC
and player scoring, unreliable NPC killfeed identity and weapon names, missing
ranked results, stale-map NPC spawns, map-change damage recovery, leaderboard
background/layout, and shotgun popup aggregation.

The record distinguishes source/build evidence from runtime and human
playtest evidence. It does not claim unresolved behavior is fixed.

## Documents and review

Read and followed `docs/ROUTER.md`, `docs/doc-review-09-03-2026.md`,
`docs/features/TEMPLATE.md`, `docs/features/README.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/specs/gamemodes/gamemodes.md`,
`docs/regressions/regressions-v1.md`, and
`docs/skills/documentation-checker-v1.md`.

## Validation

- Confirmed the rewritten file has all sections required by the template.
- Confirmed links point to the current FFA, gamemode, GUI, weapon, networking,
  regression, and ownership documents.
- No source or configuration behavior changed, so no build was required.
- Human review is still appropriate for confirming that the wording and the
  single `NEEDS_SPEC_DECISION` accurately represent the intended FFA behavior.
