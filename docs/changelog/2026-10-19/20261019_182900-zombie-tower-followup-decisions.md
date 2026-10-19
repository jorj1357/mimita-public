# Zombie Tower follow-up decisions

- Status: `SPEC_UPDATED_WITH_REMAINING_OPEN_DETAILS`.
- Timestamp: 2026-10-19T22:29:00Z (2026-10-19 18:29:00 EDT, America/New_York).
- Request: Incorporate the user's answers about wipe retries, player-controlled zombies, and the Chainsaw Boss placeholder; answer whether the boss bar is specified.
- Skill: `docs/skills/spec-behavior-review-v1.md`.
- Pre-existing work: The new first-playable contract and raw notes below it were already uncommitted before this follow-up. Earlier unrelated working-tree changes were preserved.

## Changes

Updated `docs/specs/gamemodes/zombie-tower/zombie-tower.md:9-61`:

- Wipes with attempts remaining now show a five-second attempt-result overlay, respawn at the nearest previously activated checkpoint, and restart the countdown without returning to the lobby. Exhausting all three attempts enters Phase 4; final results show for five seconds before the lobby's 30-second intermission.
- Added the provisional `zt_team <humans|zombie|spectator>` team-choice command. A human can join the active human team and replace a party NPC. Players choosing zombie play as Normal Zombies and spawn from a checkpoint above the team's highest reached checkpoint, pushing down toward the humans.
- Clarified that the Chainsaw Boss with 2x base health is only a placeholder encounter, not the intended final boss.
- Clarified boss bar requirements: encounter clients see the boss's name and authoritative current health while the encounter is active. The spec defines information and visibility, not a detailed GUI layout or styling.

## Exact before and after

- Before the retry clarification, `docs/specs/gamemodes/zombie-tower/zombie-tower.md:9` said: `The server runs this loop indefinitely:`
- After, line 9 says: `The server runs this loop indefinitely. A wipe with attempts remaining is handled inside the active run: show a five-second attempt-result overlay, restore the team at its nearest previously activated checkpoint, then run the 3/2/1/GO countdown and resume. It does not return to the lobby between attempts. When the attempt count reaches zero, enter Phase 4 for the final run results, show them for five seconds, then return to Phase 1 and start the 30-second lobby intermission.`
- Before the boss-placeholder clarification, line 49 said the Chainsaw Boss-based actor was the first-version final boss. After, it says: `The first playable version uses the Chainsaw Boss as a placeholder boss encounter with twice the base Chainsaw Boss health. It is explicitly not the intended final boss. The Chainsaw Boss actor preset must be defined before this placeholder encounter is implemented; the real final boss is future design work.`
- New boss-bar text was added at line 50; no prior first-playable-contract text existed there. It requires the boss's name and authoritative health to be visible to encounter clients only during an active encounter, and points to sections 116, 117, and 156.

## Boss bar answer and remaining work

The Zombie Tower spec already requires all clients in the encounter to see a boss name and authoritative health bar (section 116, lines 3045-3056), says the bar represents the actual remaining fight with no hidden second health bar (section 117, lines 3060-3081), and limits visibility to the active encounter (section 156, lines 3742-3746). The GUI specs `docs/specs/gui/guiv2.md` and `docs/specs/gui/gui-general.md` do not define a boss-bar layout or styling. A visual design remains unspecified.

Open details retained in the contract: exact human join spawn and zombie respawn/fallback rules; exact music catalog entries and effects-toggle semantics; and concrete full-tower encounter/checkpoint/boss map data. No tests, build, or runtime validation were run. The current mode remains unverified in gameplay.
