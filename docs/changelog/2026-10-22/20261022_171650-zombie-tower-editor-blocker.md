// 2026-10-22T17:16:50Z

# Task

- Task ID: Payload spec readiness explanation and Zombie Tower editor blocker intake
- Summary: Assess whether Payload is specific enough for implementation and record the user's map-length/spawn-volume authoring blocker in the open gameplay issues inbox.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-22T17:16:50Z UTC; 2026-10-22 13:16:50 EDT, America/New_York
- Branch: `20261011zt`
- Base commit: `853366427f4285e43a21005e82838f000260668d`
- Final commit: `853366427f4285e43a21005e82838f000260668d` (no commit created)

# Pre-existing changes

- At task start, unrelated modified files included `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `docs/jorj-docs/20261022plan.md`, `shaders/basic.vert`, and existing changes in `docs/specs/open-gameplay-issues.md`; all were preserved.
- An untracked earlier changelog `docs/changelog/2026-10-22/20261022_171528-gameplay-issues-intake.md` was preserved.
- The issue document already had four uncommitted rows from earlier work. This task added only one new intake row and its matching index row; the earlier rows were not changed.

# Documents and review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, `docs/specs/gamemodes/payload.md`, `docs/specs/open-gameplay-issues.md`, `docs/specs/entity-editor/entity-editor.md`, and `docs/specs/gamemodes/zombie-tower/zombie-tower.md`.
- Router-listed `docs/doc-review-09-03-2026.md` does not exist in this checkout.
- Findings: Payload is a design draft with a useful broad mode contract and a follow-up implementation-owner list, but important behavior values remain approximate/proposed (clock, overtime qualification, cart push/rollback formula, interaction geometry/config owner, and host/session persistence). It is adequate to begin source/config ownership mapping and a small foundation slice after design approval; it is not a complete, fully deterministic implementation contract for the entire mode.
- Zombie Tower's current first-playable contract says the phases, team roster, encounter activation, retries, checkpoints, results, and boss placeholder are defined, while authored map volumes and actor presets remain required content. The Entity Editor currently documents `monster_zone` command creation, editing by commands, JSON save/reload; it does not document viewport drag editing or copy/paste.
- The reported tower length and editor workflow are user-reported, not independently measured or runtime-reproduced. This is intake, not root-cause confirmation.
- Memory consulted for related Zombie Tower context; current checkout specifications were read for the present assessment.

# Change

## `docs/specs/open-gameplay-issues.md`

- New intake row at line 80.
- New index row at line 117.
- Before: no Zombie Tower map-authoring/copy-paste issue was present in the issue table or index.
- After intake row: `| **Blocker — Zombie Tower map authoring and spawn-volume editing** | The player believes the tower may not be long enough and reports that it needs many specifically placed zombie spawn volumes. Adding them by repeating `entity_add` commands and editing JSON to change them is too cumbersome. They want a better map editor where spawn volumes can be selected, copied/pasted, and moved by clicking and dragging. | Human-reported blocker to continuing Zombie Tower map work; tower length has not been measured or confirmed as the cause. Current Entity Editor documents command-based creation and JSON save/edit flows, including `monster_zone`; it does not document drag handles or copy/paste. Investigate the actual map length and authoring workflow separately. Define editing behavior for selecting, dragging, duplicating, assigning unique IDs, and saving volumes before implementing the editor improvement. | [`entity-editor/entity-editor.md`](entity-editor/entity-editor.md), [`gamemodes/zombie-tower/zombie-tower.md`](gamemodes/zombie-tower/zombie-tower.md) |`
- After index row: `| Zombie Tower map authoring is a reported blocker | The player believes the tower may be too short and says many specific zombie spawn volumes must be placed. Repeating commands and editing JSON makes this slow; desired editor improvements are click-and-drag movement and copy/paste for spawn volumes. Map length and the exact editor interaction remain to be verified/defined. | [`entity-editor/entity-editor.md`](entity-editor/entity-editor.md), [`gamemodes/zombie-tower/zombie-tower.md`](gamemodes/zombie-tower/zombie-tower.md) |`
- Reason: preserve the player's description, distinguish a reported blocker from a verified map-size cause, and route follow-up to the existing Entity Editor and Zombie Tower documents.

# Validation and remaining work

- `git diff --check -- docs/specs/open-gameplay-issues.md` produced no whitespace errors; Git printed only its existing LF-to-CRLF working-copy warning.
- No source code, map, or editor behavior changed. No build, test, runtime reproduction, or human acceptance was performed.
- Human follow-up: measure the playable tower route and decide the desired editing interaction. At minimum, the editor issue needs selection, viewport drag, copy/paste, unique duplicate IDs, and persistence to the same map JSON clarified before implementation scope is finalized.

# Documentation note

- `docs/specs/gamemodes/payload.md:1` begins with the raw note `2026 10 06 1607 jorj todo - make it like`. Suggested future cleanup: retain it as clearly labeled historical context or replace it with a short design-origin note; do not let it be read as a current requirement. It was not changed in this task.
