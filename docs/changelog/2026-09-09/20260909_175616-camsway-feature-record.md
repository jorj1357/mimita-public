# Rewrite camera-sway feature record

Time: 2026-09-09T17:56:16Z

## Work performed

Rewrote `docs/features/camsway-realisticish/camsway.md` according to the current `docs/features/TEMPLATE.md`.

The new record contains:

- purpose and desired observable behavior;
- current behavior and exact code evidence;
- current implementation status;
- explicit decisions and NEEDS_SPEC_DECISION items;
- code, configuration, runtime/event, network, and animation/physics ownership;
- related specifications, architecture documents, workflow, focused review skills, and regression-system documentation;
- relevant source/configuration files;
- automated, runtime, log, build, and human-playtest evidence;
- pass/fail acceptance criteria;
- changelog and pending regression links;
- preserved original informal notes.

No source code, configuration, regression record, README, or template was changed.

## Regression-system review

Read `docs/regressions/README.md`, `docs/regressions/regressions-v1.md`, the dated regression example files, and `docs/workflows/fix-repeated-bug.md`.

The current system uses separate files under `docs/regressions/YYYY-MM-DD/`, with lowercase kebab-case names ending in `-REG.md`. Each regression has creation/update timestamps, a status, occurrence sections, exact expected/actual behavior, specification, wrong/corrected code, confirmed cause, fix attempts, proof, and related changelog. Repeated occurrences remain in the same regression file. Normal AI work belongs in `docs/changelog/`, not regression records.

The new README conflicts with older text in `docs/regressions/regressions-v1.md`: the README says dated regression files are editable and independent, while the older index still describes itself as append-only. The dated-file README is the newer operational system and should be treated as the active routing guidance; the index is historical unless reconciled in a separate documentation task.

## Validation

- Read the feature template and current feature README.
- Read the active regression README and workflow.
- Verified the rewritten feature record has all template sections.
- Ran `git diff --check -- docs/features/camsway-realisticish/camsway.md`; no content error was reported.
- No build was needed because only documentation changed.
- Pre-existing `docs/features/TEMPLATE.md` edits were preserved and not claimed.

## Remaining follow-up

The shared camera punch/recoil coupling should receive its own dated regression file only when formally recorded as a confirmed regression under the new system. This session did not create that regression file.

