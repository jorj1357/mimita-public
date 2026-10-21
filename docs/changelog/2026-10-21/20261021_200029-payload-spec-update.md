// 2026-10-21T20:00:29Z

# Task

- Task ID: Payload spec accuracy pass
- Summary: Update the working Payload specification with cart interaction geometry, health timing, map philosophy, NPC system findings, host handoff questions, Lua direction, and a follow-up file map.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-21T20:00:29Z, UTC
- Branch: `20261011zt`
- Base commit: `0ac54527`
- Final commit: `0ac54527` (no commit created)

# Pre-existing changes

- The working tree was already modified before this task. Existing changes included `config/accounts/default.json`, `docs/architecture/accessibility/accessibility.md`, `docs/architecture/terminal-commands/terminal-commands.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/specs/gamemodes/caravan.md`, `docs/specs/gamemodes/payload.md`, `docs/specs/lore/lore.md`, and `docs/specs/vehicles/vehicles.md`; deleted weapon drafts `docs/specs/weapons/hafs.md`, `docs/specs/weapons/melee-weapons.md`, and `docs/specs/weapons/swordsword.md`; and untracked folders `docs/architecture/vision-mentality/`, `docs/specs/code-wrapper/`, `docs/specs/config-load/`, `docs/specs/live-voice-generation/`, `docs/specs/weapons/melee/`.
- `docs/specs/gamemodes/payload.md` already contained an uncommitted 433-line expanded draft. That draft was preserved and updated in place.
- Other pre-existing files and deletions were not edited by this task.

# Requested behavior

Make the Payload document more accurate and detailed, explain the related NPC systems plainly, record the cart's configurable interaction area around an origin, change healing to one HP per ten ticks, describe the map-building approach, document the long-term Lua direction, and list other likely related files.

# Specification alignment

- Current specification: `docs/specs/gamemodes/payload.md`.
- Related specifications/architecture reviewed: `docs/specs/gamemodes/gamemodes.md`, `docs/specs/networking/networking.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/architecture/vision-mentality/mapping.md`, `docs/specs/code-wrapper/code-wrapper.md`, `docs/architecture/json-configuration/json-configuration.md`, and `docs/specs/config-load/config.load.md`.
- Focused skills: `docs/skills/documentation-checker-v1.md` and `docs/skills/spec-behavior-review-v1.md`.
- `docs/doc-review-09-03-2026.md`, named by the router, was not present in this checkout.
- The game-mode guide says shared NPC goals are mode-neutral and supports objective intent. The server has TeamBrain code for current bomb/site modes. No `config/gamemodes/payload.json` exists, and the checked code does not establish moving-cart targets or push/block behavior for Payload.
- The networking spec describes game-server migration as future architecture requiring replicated authoritative checkpoints. It does not support promising that a host can leave and the match will continue.
- Open choices remain explicitly open: cart interaction shape/anchor convention, exact timer values, statistics persistence, host-handoff scope, Payload configuration owner, and final map dimensions/geometry.

# Exact implementation changes

## File: `docs/specs/gamemodes/payload.md`

- Existing working-draft areas updated: §§1, 3, 5, 6, 7, 10, 11, 12, 14, 17, and 20; new sections added as §§18–21.
- Previous server-statistics text: `- The server tracks persistent session statistics, including team victories and individual actor kills.`
- New server-statistics text at line 34: `- The server should track statistics during the match session, including team victories and actor kills. Whether those statistics survive a host leaving, a server closing, or a restart is still open; do not describe them as safely persistent until that behavior is checked and specified.`
- Previous match-duration note: `**Important unresolved detail:** An initial 5 minutes plus two 5-minute bonuses creates 15 minutes of total standard clock time, whereas the desired typical match duration was described as 20–30 minutes. The exact timer design needs another decision.`
- New match-duration text at line 92: `Match length is not a fixed target. A match may be short or long depending on how the teams play. The initial clock, checkpoint bonuses, and overtime rules still need playtesting and final values; the numbers above are starting examples, not a promise that a match should last 15, 20, or 30 minutes.`
- Previous proximity text: `- Team A actors near the cart push it forward.` and `- Team B actors near the cart contest it and prevent forward progress.`
- New proximity text at lines 113–119 states that actor collision bodies must overlap the configured interaction area and one Team B actor blocks forward movement.
- New §5 subsection at lines 127–155 defines an origin-relative interaction area, the approximate 3 m × 2.25 m × 1.5 m size, configuration ownership questions, server-tick pseudocode, and a note that this describes desired behavior rather than existing code.
- Previous healing estimate: `Desired behavior: approximately 20 HP to 100 HP in 10 seconds.` and `That corresponds to an average of 8 HP per second.`
- New healing rule at line 176: `**Starting rule:** Give a nearby friendly attacker 1 HP for every 10 server ticks. At 60 ticks per second, that is 6 HP per second, so going from 20 HP to 100 HP takes about 13⅓ seconds. Keep a fractional remainder between ticks so healing does not get lost to rounding, and never heal above the actor's normal maximum. This replaces the earlier 10-second estimate.`
- New map guidance at lines 185–215 labels the visual environment as inspiration, identifies the approximately 100 m ramp and 30-degree incline as a starting idea, and describes blockout followed by finished geometry.
- Previous friendly-fire text: `- Friendly fire undecided: preference toward bullets passing through teammates rather than heavy friendly-fire punishment.`
- New friendly-fire text at line 273: `- Friendly fire is disabled in v1: attacks pass through teammates and deal no teammate damage. A possible later rule is 10% teammate damage, but that is not part of v1.`
- New actor-role movement rule at line 289 states that roles share the movement system and use different presets.
- New §12 NPC explanation at lines 299–334 separates goal choice from physical movement and records the current-code limits found during review.
- New §14 subsection at lines 369–377 describes host handoff and statistic-persistence questions without promising migration.
- New §17 JSON note at lines 421–439 warns that a listed config family does not prove a Payload loader exists and adds cart-area configuration to the desired data list.
- New §18 map-building subsection at lines 441–448 explains blockout, playtesting, final geometry, and links the mapping and entity-editor documents.
- New §19 at lines 449–455 records Lua as a future, sandboxed scripting goal, leaves the exact API and language safety design open, and links `code-wrapper.md`.
- New §21 at lines 480–516 lists related specifications/configuration and implementation code owners to review. It states that this is a review list, not a requirement to edit every file, and that the potential Payload config file does not currently exist.
- The old section numbered 18 was renumbered to 20; specification status now says this document records desired behavior and research needs, not existing implementation claims.
- Reason: preserve the user's design direction while separating established decisions, starting values, unverified current behavior, and future work.
- Unrelated code, configs, and pre-existing document edits were preserved.

# NPC systems review

- `src/npc/npc.cpp` builds perception and utility context; behavior profiles tune NPC decisions.
- `src/npc/npc-state-machine.cpp` scores combat states; `src/npc/npc-states.cpp` turns selected states into movement/action intent; `src/npc/npc-combat.cpp` uses shared weapon definitions and firing rules.
- `src/npc/npc-navigation.cpp`, `src/npc/npc-navigator.cpp`, `src/npc/npc-nav-request.cpp`, `src/npc/recast-navigation.cpp`, and `src/npc/npc-traversal.cpp` provide route and traversal work. Shared actor movement/physics remains responsible for actual movement.
- `src/npc/team-brain.cpp` and `src/network/server-gamemode.cpp` provide current team planning and bomb/site objective context. The current objective context does not define a moving Payload cart route or its push/block rules.
- Plain-language finding: behavior chooses the NPC's goal; movement finds and follows a physical route. Payload needs both to coordinate, and the cart-specific behavior is not proven by the existing bomb/site logic.

# Follow-up file preview

The new §21 review list groups the likely follow-up documents and code owners. Not every listed path is a change target. Confirm the active owner and loader before implementation. The router names `docs/specs/gui/gui.md`, which is absent; the current GUI docs found are `docs/specs/gui/guiv2.md` and `docs/specs/gui/gui-general.md`.

# Diagnostics

- None added. This was a documentation-only task.

# Validation

- Checked the related document links in §21 against repository paths; the referenced mapping, entity-editor, code-wrapper, vehicle, weapon, movement, collision, HUD, and voice documents exist.
- `git diff --check -- docs/specs/gamemodes/payload.md` reports three trailing-whitespace lines that were already part of the pre-existing draft: line 11 (`but expand this `), line 17 (the existing Status line), and line 18 (the existing Target line). No new whitespace issue was introduced.
- The documentation checker scan found 207 broad TODO/explanation-style matches across `docs/`; unrelated notes were not edited. Task-related notes and suggested follow-up wording:
  - `docs/architecture/vision-mentality/mapping.md:1`: `2026 10 21 1550 jorj todo epxlain`. Expand it into the blockout, playtest, and final-geometry philosophy now summarized in Payload §18.
  - `docs/specs/code-wrapper/code-wrapper.md:1`: `2026 10 21 1554 jorj todo explain`. Specify the safe script sandbox, approved engine calls, and server/client authority described in Payload §19.
  - `docs/specs/entity-editor/entity-editor.md:1`: `2026 10 21 1551 jorj todo o make it so funworld3 has  the spawns defined in the entitt enditor, not in the .glb`. Continue documenting the map JSON as the editable override layer for map entities and spawn points.
  - `docs/specs/npc-behavior/npc-behavior.md:1`: `2026 10 20 1733 jorj  todo the avatrs need better coliiosnssssss`. Keep the avatar/body collision question in the NPC collision scope; it is separate from Payload cart interaction overlap.
  - `docs/specs/gamemodes/gamemodes.md:3`: `todo this not good but move tothis version more  more general things  for gamemodes`. Continue replacing the informal note with the shared, mode-neutral lifecycle and objective contract; this document already contains related structured sections.
  - `docs/specs/gamemodes/payload.md:1`: `2026 10 06 1607 jorj todo - make it like`. Treat this as historical context; the structured Payload draft now carries the active design, so a future cleanup can label or move the raw note.
- No tests or build were run because only documentation changed and the user did not request test/build verification.
- Runtime/gameplay behavior and human review remain unverified.

# Regression review

- Regression entry appended: no.
- Why: no confirmed behavior regression was reported or changed; this task clarified the specification only.

# Human acceptance

- Human review: still needed for open choices listed in §§5, 6, 7, 14, 17, and 19.
- Gameplay, visual, and multiplayer acceptance: not performed.
