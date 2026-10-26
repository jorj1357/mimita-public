# Shared crouch and networking disagreement specs

- Task ID: 01a1787b-54ae-7393-baf1-885cff395511
- Summary: Specify cross-mode Left Ctrl crouching and per-reason server-disagreement visuals; summarize the gameplay issue timeline for the player.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-26T14:01:10Z (2026-10-26 10:01:10 EDT, America/New_York)
- Branch: `20261011zt`
- Base commit: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`
- Final commit: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd` (no commit created)

## Pre-existing changes

- The working tree contained numerous existing edits in gameplay configs, NPC/gamemode source, `teamdeathmatch.md`, and both gameplay-issues documents, plus five untracked changelogs from earlier work. They were preserved.
- The three files changed in this task (`counterstrike.md`, `supersmashbrosmelee.md`, and `networking.md`) were not marked modified at task start.
- `config/networkingconfig.json` and implementation code were not changed. The spec identifies per-reason toggles as a desired schema, not implemented behavior.

## Requested behavior

- Reuse the Left Ctrl crouch/down action and crouch animation already established for Melee in every gamemode.
- Show server disagreements as understandable player feedback and a debugging aid. Add global and per-reason true/false visual controls, including position corrections and attack cooldown/no-ammo examples.
- Preserve the idea of a deliberately bad-network mode where a server-like boss may take damage from disagreement events as exploratory design, not a final rule.
- Explain the open gameplay issues in chronological order using simple language.

## Specification alignment and source findings

- Counter-Strike's old crouch note said crouching did not exist and asked for generalized Left Ctrl binding. Melee's control proposal identifies Left Ctrl as `crouch_or_fast_fall`, while the mode spec left crouch pose implementation open. The user clarified crouch now exists in Melee; the specs record the remaining cross-mode wiring without claiming live acceptance.
- `src/config/networking-config.cpp` currently reads only `disagreement_visuals.enabled`. `src/network/disagreement-visuals.cpp::spawnDisagreementEffect()` suppresses effects when disabled except for `DISAGREEMENT_POSITION_CORRECTION`, which remains visible. Per-reason filtering is not implemented.
- The networking spec distinguishes actual server corrections from ordinary prediction smoothing. Per-reason visual toggles do not disable validation, state correction, or structured diagnostic events.
- The server-as-boss concept remains an exploratory proposal. Ordinary production corrections, bugs, and packet loss do not damage a boss unless a mode later defines a separate server-authoritative rule.

## Exact documentation changes

### `docs/specs/gamemodes/counterstrike.md`

- Section `19. Crouch`, lines 703–735: replaced the stale note that crouch did not exist with the shared Left Ctrl requirement, grounded crouch animation, release-to-stand behavior, airborne downward/fast-fall reuse, and a no-per-mode-duplicate rule. Tuning for speed/fatigue/cooldown remains open.

### `docs/specs/gamemodes/supersmashbrosmelee.md`

- Section 35, line 241: added the cross-mode contract for reusing the Melee Left Ctrl action and grounded crouch animation.
- Input table, line 388: changed `crouch_or_fast_fall` from “pose implementation is open” to grounded held crouch/animation and airborne downward behavior, with cross-mode wiring required.

### `docs/specs/networking/networking.md`

- Section 25, line 1186: replaced the unclear sphere TODO with a distinction between uncertainty visualization (no fresh authoritative position) and an actual correction/disagreement.
- Section 74, starting line 4351: records the rationale for visible disagreement feedback, the desired global plus `by_reason` Boolean configuration shape, and the fact that the loader currently supports only the global switch with a position-correction exception.
- Added an exploratory server-as-boss concept and stated that ordinary production network failures are not gameplay damage.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/gamemodes/counterstrike.md`, `docs/specs/gamemodes/supersmashbrosmelee.md`, `docs/specs/networking/networking.md`, `docs/specs/movement/movement.md`, `docs/specs/animations/animations.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/skills/documentation-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/regressions/README.md`.
- `git diff --check` passed for the three edited specs. New config file references resolve to the existing `config/networkingconfig.json` and loader. No code, config, tests, or build changed.
- The router-named `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md` are absent; the current regression index is `docs/regressions/README.md`.
- Remaining unrelated TODO notes in the touched spec files were left alone: `counterstrike.md:281` asks for a supported-map list/editor (suggestion: define the map eligibility and authorable spawn/bomb-site data contract); `counterstrike.md:2391` asks to explain live C++ editing (suggestion: route to the live-code-development feature and define safe compile/activation boundaries); `counterstrike.md:2536` says “todo explain better” about 60-tick aggregate diagnostics (suggestion: specify the aggregation window, summary fields, reset behavior, and sampling cost); `networking.md:107` asks to explain product discoverability/growth (suggestion: move product vision to its owning strategy doc); `networking.md:109` asks where to document console commands (suggestion: link to `docs/architecture/terminal-commands/terminal-commands.md`).

## Regression and acceptance

- Regression entry appended: no. This is specification clarification, not a confirmed behavior regression.
- Runtime/build/human acceptance: not performed. The shared crouch animation and cross-mode input need live verification after implementation. The per-reason networking controls are only specified and require implementation/loader support before runtime verification.
- Human design decision still needed for the server-boss mode, including what counts as an attack and how simulated network conditions are controlled.
