# Zombie Tower actor presets and first-slice readiness

- Status: `SPEC_UPDATED_FIRST_SLICE_READY_TO_BEGIN`.
- Timestamp: 2026-10-19T22:40:30Z (2026-10-19 18:40:30 EDT, America/New_York).
- Request: Clarify the Zombie Tower first-playable contract using the user's checkpoint, player-zombie, music, map-authoring, and actor-preset decisions; explain whether this is enough to begin implementation and define how monster presets relate to roles and modes.
- Skill: `docs/skills/spec-behavior-review-v1.md`; checked changed-document TODOs using `docs/skills/documentation-checker-v1.md`.
- Pre-existing work: The Zombie Tower contract and raw notes were already present as uncommitted work. The actor-presets document was newly created in this task. Unrelated working-tree changes were preserved.

## Changes

- Expanded `docs/specs/actor-presets/actor-presets.md` with a target explanation of the chain `gamemode JSON → role in roles.json → actor preset → shared actor systems`, JSON ownership, intended inheritance/override rules, current implementation limitations, hot-reload boundaries, and a first behavior pass for Boomer, Stabber, Crawler, Brute, Chainsaw Boss, and Charger.
- Updated `docs/specs/gamemodes/zombie-tower/zombie-tower.md` to include the agreed human join checkpoint, zombie role/respawn rule, intro-plus-random soundtrack assets, and an explicit `zt_effects <on|off>` scope. Added an implementation-readiness section separating agreed run behavior from map/preset content and the checkpoint edge case.
- Added `docs/ROUTER.md:79-80` routes for actor preset/role/mode roster work and Zombie Tower phases/encounters.
- Kept the user's informal source notes verbatim below the formal sections.

## Exact before and after

- `docs/specs/actor-presets/actor-presets.md:1-27` is new formal text; there was no prior formal specification. It opens with: `# Actor Presets, Roles, and Game Modes` and explains `GAME MODE JSON → ROLE in config/roles.json → ACTOR PRESET in config/actor-presets/<id>.json → shared actor systems`.
- Before, the actor-presets file contained only informal notes beginning `2026 10 19 1834 jroj todo explain this stuff is so cool like`. That note remains at line 176, after the new explanation.
- Before the final status/readiness clarification, Zombie Tower line 3 said: `**Status:** Agreed behavior from the 2026-10-19 design discussion, with the open decisions at the end still requiring a choice before implementation.` After, line 3 says: `**Status:** Core first-playable behavior is agreed. The implementation-readiness section below separates work that can begin now from authored map data and remaining edge cases.`
- New Zombie Tower readiness text at lines 57-61 says the mode-flow contract is detailed enough to begin the first implementation slice, while authored map data and actor/monster presets, roles, and supported attacks remain required for an end-to-end playable map. The visual layout of the results screen and boss bar is still unspecified.
- Before, line 54 described an unnamed effects command and left its scope open. After, line 54 specifies: `Provide the developer console command zt_effects <on|off>. It disables or enables fog, rain, and dynamic torch lighting together. It does not change gameplay or forced first-person view.`
- The music contract at lines 68-95 lists the user's exact GO track and 11 random-pool paths as mode-JSON asset paths. It says the GO track plays on unfreeze and clients hear the same current selection.

## Readiness and evidence boundaries

- Enough to begin implementation of the first run-flow slice: yes. This is a specification-readiness judgment, not implementation or playability evidence.
- End-to-end playability still depends on the user-authored lobby/tower map data, checkpoint/spawn/encounter placement, actor and monster roles/presets, and available shared attack definitions. The edge case for a player zombie when every checkpoint has already been reached remains open.
- The actor-preset document is explicitly a target contract. The current role/preset loader does not yet implement full generic preset inheritance; the Zombie Tower gamemode JSON lacks the required role references and phase/roster behavior.
- The Zombie Tower spec requires boss name and authoritative health only during an active encounter. Existing GUI documents do not define boss-bar styling/layout; this remains an art/UI detail.
- Documentation-only change. No tests, build, runtime session, or visual/human acceptance were run.

## Documentation TODOs found

- `docs/specs/actor-presets/actor-presets.md:176`: `2026 10 19 1834 jroj todo explain this stuff is so cool like`. The new formal sections 1-8 now explain the recipe, the role/mode relationship, ownership, override intent, and current hot-reload/implementation boundaries; keep the original note for history or remove it in a later cleanup.
- `docs/specs/gamemodes/zombie-tower/zombie-tower.md:1453`: `todo explain better  enshittification ?`. The following text already defines platform decay and says systems should not overload players. Suggested expansion: connect those principles to specific Zombie Tower choices (for example, keep rewards fair and introduce threats clearly), or label the raw note as historical.
- `docs/specs/gamemodes/zombie-tower/zombie-tower.md:5591`: `2026 10 06 1603 jorj todo - explain zombie tower`. Suggested replacement: add a short purpose paragraph describing the early Roblox tower-survival inspiration, the co-op human/NPC team, and the floor-by-floor encounter run; or label the note as historical.

`git diff --check` reported trailing whitespace in the preserved raw Zombie Tower notes; those historical user notes were left untouched. No whitespace error was reported in the newly written formal sections.
