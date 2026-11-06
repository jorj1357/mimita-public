# Regression Records

This folder contains confirmed behavior regressions and a dedicated cold-build debt record. Ordinary work history belongs in `docs/changelog/`.

## Domain folders

Each active regression has exactly one detail file in one domain folder. The domain `README.md` files are the indexes and evidence-backed summaries:

- [Collisions](collisions/README.md)
- [Performance](performance/README.md)
- [Networking](networking/README.md)
- [NPC movement](npc-movement/README.md)
- [Dev loop](dev-loop/README.md)
- [Gameplay](gameplay/README.md)

Detail filenames use `three-word-summary-REG-YYYY-MM-DD.md`. The date is the recorded first-report date, not a fabricated time. Preserve every original timestamp and chronology; if a source did not record a time, leave it unknown. Append new occurrences to the existing record when they concern the same underlying regression. A code change or build alone is not a confirmed solution; retain unresolved or attempted-fix status until required runtime and human evidence exists.

The archived `archive-ignore/regressions-v1.md` remains archive material and is not an active detail record.

## Cold-build debt

The canonical cold-build debt record is [Cold Build Required](dev-loop/cold-build-required-REG-2026-09-20.md). The root-level `cold-build-required-REG.md` is a historical template only.
