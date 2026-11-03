# NPC and Movement regressions

Append-only master ledger. Each entry links to the detailed dated record that owns the full evidence.

Entries: 8

---

## Entry 1 — NPC Killfeed and FFA Scoring Broken

- Recorded: 2026-09-07T00:00:00Z
- Status: ATTEMPTED FIX (4)
- Summary: docs/specs/networking/networking.md
- Detail: `docs/regressions/2026-09-07/npc-killfeed-scoring-REG.md`

## Entry 2 — NPC Damage Loses Team Identity

- Recorded: 2026-10-20T22:29:03Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/juggernaut.md
- Detail: `docs/regressions/2026-10-20/npc-damage-loses-team-identity-REG.md`

## Entry 3 — Juggernaut Fighter Role Preset Regression

- Recorded: 2026-10-30T13:29:27Z
- Status: ATTEMPTED FIX (1)
- Summary: Time: 2026-10-30 (user report; exact time not supplied)
- Detail: `docs/regressions/2026-10-30/jugger-role-preset-REG.md`

## Entry 4 — NPC spawn exposed a placeholder avatar before the mode policy

- Recorded: 2026-11-01T19:49:06Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME EVIDENCE PARTIAL
- Summary: docs/specs/npc-behavior/npc-behavior.md
- Detail: `docs/regressions/2026-11-01/npc-spawn-avatar-placeholder-REG.md`

## Entry 5 — AI body aimed at enemies while the camera stayed fixed

- Recorded: 2026-11-01T22:55:00Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME EVIDENCE PASS
- Summary: docs/architecture/terminal-commands/terminal-commands.md
- Detail: `docs/regressions/2026-11-01/ai-camera-stale-body-facing-REG.md`

## Entry 6 — TDM NPCs loitered near spawn instead of pushing enemies

- Recorded: 2026-11-01T22:55:00Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME EVIDENCE PARTIAL
- Summary: In TDM, team NPCs appeared to follow each other near spawn rather than advance to find and kill enemies.
- Detail: `docs/regressions/2026-11-01/tdm-spawn-following-REG.md`

## Entry 7 — Counter-Strike NPC targeting and movement regression

- Recorded: unknown
- Status: see detail
- Summary: During a human Counter-Strike playtest on dust2cyberiav4, the team-spawn problem appeared fixed, but some NPCs did not attack the nearby human. Terrorist NPCs escaped spawn farther than Counter-Terrorist NPCs, while other NPCs held a
- Detail: `docs/regressions/2026-10-04/counterstrike-npc-targeting-movement-REG.md`

## Entry 8 — NPC Waves kept the player dead at the death position

- Recorded: unknown
- Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED
- Summary: - Recorded: 2026-10-25T00:14:05Z - Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED
- Detail: `docs/regressions/2026-10-25/npc-waves-stale-death-REG.md`
