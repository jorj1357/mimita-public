# Gameplay issue tracker refresh

Time created: 2026-11-05T18:05:31Z  
Time last updated: 2026-11-05T18:05:31Z  
Display timezone: America/New_York (2026-11-05 13:05:31 EST)

- Task: reorganize the gameplay issue tracker, recheck statuses with the user, and route dated product ideas to owner specifications.
- Result: `BLOCKED` for repository completion. The requested documentation edits and link checks are complete, but the required agent gate reported 82 damage-authority findings against baseline 81. No baseline or source-code change was made in this documentation task.
- Branch: `20261011zt`
- HEAD at session start: `afe5b69210eb43cc7be917a22283041a8750099b`
- Runtime build identity: not collected; no gameplay/runtime validation was performed.

## Changes

- `docs/specs/open-gameplay-issues.md`: replaced the approximately 500-line raw transcript prefix and duplicated issue-detail section with a short tracker grouped as Active, Needs recheck, Resolved or no longer wanted, Deferred, and a dated idea index. Kept the existing OGI IDs and archive/reopen records. The old note beginning `2026 11 0 1232 jojr leaving a game...` is summarized as OGI-022; the rejoin team/outline report is OGI-023; local hosting without coordinator availability is OGI-024. OGI-025–027 preserve older uncertain damage-presentation and avatar reports. The tracker records the user’s 2026-11-05 updates as user reports rather than independent verification.
- `docs/specs/networking/networking.md`: added local-only hosting and coordinator-unavailable notice behavior, the F3 server-Hz request, and avatar identity/remote-presentation direction.
- `docs/specs/gamemodes/gamemodes.md` and `docs/specs/gamemodes/counterstrike.md`: added the shared interaction/prompt direction, active input-context direction, and the rejoin team/outline contract. Prior team retention and the exact defuse range remain undecided.
- `docs/architecture/accessibility/accessibility.md`, `docs/specs/weapons/weapons.md`, `docs/specs/animations/animations.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/specs/visuals/blood-bulletholes-viscera.md`, and `docs/architecture/vision-mentality/vision.md`: routed input contexts, inventory UI/grenade direction, avatar animation coverage, live editor rotation, blood/gore appearance direction, and creator/social/persistence vision to existing owners. Exploratory items are labeled as directions rather than implementation commitments.
- `docs/specs/weapons/weapons.md`: repaired three existing relative links to files under `weapons/melee/` after the documentation link check found them unresolved.

## Reasoning and unresolved spec decisions

The user reports OGI-006 and OGI-008 are fine; OGI-013 and OGI-016 are done; OGI-020 is no longer wanted; and OGI-021 is fine. OGI-003, OGI-005, OGI-014, OGI-015, OGI-025, OGI-026, and OGI-027 remain rechecks because current evidence is uncertain. OGI-012 remains as previously recorded at the user’s request.

The `2026-11-05` local-host request is consistent with the existing networking spec’s direct local client/server path and background room registration. This edit adds user-visible behavior for coordinator failure; it does not claim runtime proof. The user’s “one interaction popup / one central Interact” request is recorded as a shared direction; implementation ownership must still be traced against the existing action surface before code work. The F3 request is limited to server Hz; no extra metrics were selected. Python GUI presentation, avatar-loading admin/server policy, data-storage topology, and exact interaction range remain open.

## Documents and focused skills

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, `docs/regressions/README.md`, and `docs/workflows/runtime-scenario-validation.md`, plus the linked issue-owner specifications.
- Documentation Checker: links passed across 10 edited documents; headings and tracker categories are present. Existing informal TODO notes listed below were left unchanged.
- Specification and Behavior Review: no code was changed or behavior verified. User-reported observations are labeled as such; exact interaction range and team-choice persistence remain unresolved.

## Validation and evidence

- Markdown relative-link check across the 10 edited documents: `PASS`.
- `git diff --check` across the edited documents: `PASS` (Git emitted only line-ending normalization warnings).
- `python devscripts/quality/agent-gate.py`: `FAIL`, `[lint] GATE FAILED`, `damage-authority.findings: 82 > baseline 81`; the output also reported gamemode sync `PASS`. No baseline edit was made.
- `python devscripts/quality/check_damage_authority.py`: 82 findings. The new/extra finding was not isolated to a change from this documentation task.
- Runtime journal, visual review, multiplayer acceptance, and human gameplay review: not performed.

The shared working tree also contained unrelated edits during validation. These were not changed by this task: `AGENTS.md`, `config/analytics.json`, `devscripts/blender-ai` (submodule), `devscripts/dev-launch-modes.json`, `devscripts/dev-loop.py`, `devscripts/quality/agent-gate.py`, `docs/specs/dev-loop/dev-loop.md`, `docs/regressions/2026-11-05/rmb-weapon-aim-reversal-REG.md`, `docs/regressions/master/gameplay-REG.md`, `src/network/server-players.cpp`, plus an untracked changelog and regression record for Fun Counter Strike loadout override. The cause of the gate’s additional finding was not established; the source and gate-script changes remain untouched.

## Existing documentation TODO notes found

These existing comments remain unchanged. Suggested follow-up wording is listed for the owner of each note:

- `docs/specs/networking/networking.md:107` — “todo - explain little quality of life things and discoverability and scaling of mimita engine”. Keep the product-discoverability direction in the project vision and replace the raw note with a link to that owner.
- `docs/specs/networking/networking.md:109` — “todo explain all console commands either in here or a centralized document”. Link the terminal-command specification and state that it owns command syntax.
- `docs/specs/gamemodes/gamemodes.md:471` — “Todo explain this 9 2 2026 0848”. Replace with the dated mode concept and its behavior owner, or mark it as historical.
- `docs/specs/gamemodes/counterstrike.md:2397` — “todo explain the direction id like to go in where, everything is hot reloadable”. Link the live-development owner and limit this mode spec to Counter-Strike data that must reload.
- `docs/specs/gamemodes/counterstrike.md:2542` — “todo explain better”. Specify the intended aggregation window and diagnostic owner, or remove the note once the surrounding text defines it.
- `docs/specs/visuals/blood-bulletholes-viscera.md:2` — “jorj todo explain”. Replace with a short scope/status note; the document already contains the behavior contract below.
- `docs/specs/weapons/weapons.md:74` — “2026 10 20 1655 jorj todo explain”. Replace with the specific revolver behavior question or link its owning section.
- `docs/specs/gui/guiv2.md:23` and `:25` — terminal-first/data-command GUI TODOs. Define the shared action owner and have buttons dispatch through it, or link the existing terminal/action architecture.
- `docs/specs/gamemodes/objective-hud.md:1` — “todo this combine this with the gui spec needs to eb all at one file”. Keep the authoritative objective-state contract separate if ownership differs, and link the GUI rendering owner rather than duplicating requirements.
- `docs/architecture/player-npc-systems/npc-movement.md:22` — “todo - explain”. Replace with the specific missing navigation behavior and acceptance evidence.

## Human review remaining

The user should confirm the still-active/recheck items through current play and evidence. Visual or multiplayer acceptance is not claimed. The gate failure needs a separate cause review before the repository can be reported green.