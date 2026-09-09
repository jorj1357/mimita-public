# FFA code diagnosis update

Branch: `8292026stash`
Commit: working tree; no commit created
Timestamp: `2026-09-09T18:05:45Z`

## Scope

Updated `docs/features/gamemodes/ffa mode issues.md` only. No gameplay source,
configuration, tests, or runtime behavior was changed.

## Pre-existing changes preserved

The working tree already contained edits to `docs/features/TEMPLATE.md`,
`docs/features/camsway-realisticish/camsway.md`, and earlier changelog files.
Those files were not modified or claimed as part of this session.

## Documentation change

Added `## Code diagnosis: why current behavior does not meet the desired
behavior` to the FFA feature record. It compares desired and observed behavior
against the current owners and records:

- the single pending-kill slot that overwrites repeated kills;
- literal mode-specific scoring and incomplete membership handling;
- separate NPC→player and player→NPC killfeed/network paths;
- weapon ID versus display-name configuration-key mismatch;
- conditional inventory rebuilding during respawn/loadout application;
- duplicated map/NPC respawn paths that can preserve stale spawn positions;
- replicated results state without the requested results-panel renderer;
- delayed NPC removal without an immediate generic membership event; and
- the likely map-change damage and shotgun-popup owners, explicitly marked for
  focused runtime confirmation rather than asserted as proven causes.

Each major issue includes the current code pattern and the generalized fix
direction. The document distinguishes confirmed source causes from hypotheses
that require runtime tracing.

## Validation

- Read `docs/ROUTER.md`, the gamemode and networking specifications,
  `docs/features/README.md`, the feature template, time-formatting guidance,
  task-completion guidance, regression guidance, and the documentation checker.
- Confirmed the rewritten feature record contains the comparison, ownership,
  evidence, and acceptance sections.
- Ran `git diff --check`; warnings are pre-existing trailing whitespace in
  `docs/features/TEMPLATE.md`.
- No build was required because only documentation changed.
- Human review is still appropriate for confirming the intended FFA results
  duration and the exact runtime producer of the shotgun popup issue.
