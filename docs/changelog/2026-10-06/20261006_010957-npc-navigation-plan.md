# NPC navigation migration plan document

Date (UTC): 2026-10-06T05:09:57Z
Display timezone: America/New_York
Display time: 2026-10-06 01:09:57 EDT
Branch: `afad20a-rebuild`

## Result

`PASS_WITH_HUMAN_REVIEW`

Created the requested planning document:
`docs/specs/20261006plan.md`.

## Purpose

Turn the NPC navigation library investigation into a repository-readable plan
for future human and AI agents. The plan records current ownership, duplicate
ownership risks, Recast/Detour research, the MiMITA boundary, dynamic-world
strategy, diagnostics, tests, risks, migration phases, and the smallest useful
first experiment.

## Files changed

- Added `docs/specs/20261006plan.md`.
- Added this changelog.

No C++ source, configuration, build file, test implementation, or runtime
behavior was changed.

## Documentation checks

- Confirmed the repository's dated plans are under `docs/specs/`.
- Confirmed there is no existing `docs/specs/20261005plan.md`.
- Used the established dated-plan filename convention and did not overwrite an
  existing plan.
- Ran `git diff --check` against the new plan with no whitespace errors.
- Read `docs/skills/documentation-checker-v1.md`.
- The new plan has a purpose, audience, explicit `Does not` section, authority
  references, staged implementation order, and acceptance gates.

## Existing TODOs noted, not edited

The repository-wide documentation TODO scan found pre-existing TODO material.
The most directly related item remains `docs/architecture/player-npc-systems/
npc-movement.md:3`, which says `jorj todo - explain`. The new plan records this
without rewriting that separate document.

## Validation and review still needed

No build or runtime test was run because this change only adds planning
documentation. Human review is still needed before implementation begins,
especially approval of Recast + Detour as the preferred backend, the shared
MiMITA movement boundary, the compare-mode parity phase, and the requirement
that old navigation owners are deleted only after evidence.

Pre-existing worktree changes were preserved and not attributed to this plan.
