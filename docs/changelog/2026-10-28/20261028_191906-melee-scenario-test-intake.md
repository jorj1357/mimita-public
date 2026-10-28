# Melee scenario test-suite improvement intake

- Time: 2026-10-28 15:19:06 EDT (2026-10-28T19:19:06Z)
- Branch: `20261011zt`
- HEAD: `8f455b01`
- Result: `PASS_WITH_HUMAN_REVIEW` — the requested follow-up was added to the gameplay issue tracker; no implementation or gameplay verification was performed.

## Repository change

Updated `docs/specs/open-gameplay-issues.md`, the existing “SwordSword and mace physical hits fail inconsistently in Sandbox” issue, under the new “Dev-loop scenario and test-suite improvement requested” subsection (line 1720 at review time).

The entry proposes making the dev-loop scenario useful for diagnosing the reported damage disagreement: record and compare actors' client-rendered and server-authoritative state, drive recorded targets as temporary server proxies, require spawn activation and matching move-set revision before playback, and report setup failures instead of passing gameplay assertions. It calls for equivalent SwordSword, mace, and chainsaw hit/miss cases that assert visible overlap, authoritative contact, acceptance/rejection reason, and target health before/after, while keeping config checks, focused calculation checks, runtime evidence, and human visual acceptance distinct. The entry explicitly says this proposal is not implemented or verified.

## Review and validation

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- The router also names `docs/doc-review-09-03-2026.md`, but that file is absent in the current checkout.
- `git diff --check -- docs/specs/open-gameplay-issues.md` passed.
- No code, build, runtime scenario, or gameplay behavior was changed or verified. Human review is still needed for the proposed test design and any later implementation.

## Pre-existing workspace changes

Preserved all other workspace modifications and untracked files present during this documentation-only update. They include changes under `config/`, `devscripts/`, `docs/architecture/`, and `src/`, as well as untracked `docs/specs/gamemodes/aooni.md`, `src/devtools/scenario-tracks.cpp`, and `src/devtools/scenario-tracks.h`. No pre-existing changes were included in this issue entry beyond the requested documentation addition.
