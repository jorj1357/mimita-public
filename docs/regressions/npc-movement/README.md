# NPC and Movement regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 8
---

## Evidence-backed recurring problems and fixes

| Recurring pattern | Evidence-backed response |
|---|---|
| NPC role, spawn, targeting, or map geometry resolves inconsistently. | Trace shared NPC ownership from authored identity through spawn and movement. |
| NPC containment or per-actor work is too expensive. | Measure actor/collision phases and remove redundant work while preserving cached broadphase. |

---

## Entry 1 — NPC Killfeed and FFA Scoring Broken

- Recorded: 2026-09-07T00:00:00Z
- Status: ATTEMPTED FIX (4)
- Summary: docs/specs/networking/networking.md
- Detail: [npc-killfeed-scoring-REG-2026-09-07.md](npc-killfeed-scoring-REG-2026-09-07.md)

## Entry 2 — NPC Damage Loses Team Identity

- Recorded: 2026-10-20T22:29:03Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/juggernaut.md
- Detail: [npc-damage-loses-REG-2026-10-20.md](npc-damage-loses-REG-2026-10-20.md)

## Entry 3 — Juggernaut Fighter Role Preset Regression

- Recorded: 2026-10-30T13:29:27Z
- Status: ATTEMPTED FIX (1)
- Summary: Time: 2026-10-30 (user report; exact time not supplied)
- Detail: [jugger-role-preset-REG-2026-10-30.md](jugger-role-preset-REG-2026-10-30.md)

## Entry 4 — NPC spawn exposed a placeholder avatar before the mode policy

- Recorded: 2026-11-01T19:49:06Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME EVIDENCE PARTIAL
- Summary: docs/specs/npc-behavior/npc-behavior.md
- Detail: [npc-spawn-avatar-REG-2026-11-01.md](npc-spawn-avatar-REG-2026-11-01.md)

## Entry 5 — AI body aimed at enemies while the camera stayed fixed

- Recorded: 2026-11-01T22:55:00Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME EVIDENCE PASS
- Summary: docs/architecture/terminal-commands/terminal-commands.md
- Detail: [ai-camera-stale-REG-2026-11-01.md](ai-camera-stale-REG-2026-11-01.md)

## Entry 6 — TDM NPCs loitered near spawn instead of pushing enemies

- Recorded: 2026-11-01T22:55:00Z
- Status: IMPLEMENTED IN SOURCE; RUNTIME EVIDENCE PARTIAL
- Summary: In TDM, team NPCs appeared to follow each other near spawn rather than advance to find and kill enemies.
- Detail: [tdm-spawn-following-REG-2026-11-01.md](tdm-spawn-following-REG-2026-11-01.md)

## Entry 7 — Counter-Strike NPC targeting and movement regression

- Recorded: unknown
- Status: see detail
- Summary: During a human Counter-Strike playtest on dust2cyberiav4, the team-spawn problem appeared fixed, but some NPCs did not attack the nearby human. Terrorist NPCs escaped spawn farther than Counter-Terrorist NPCs, while other NPCs held a
- Detail: [counterstrike-npc-targeting-REG-2026-10-04.md](counterstrike-npc-targeting-REG-2026-10-04.md)

## Entry 8 — NPC Waves kept the player dead at the death position

- Recorded: unknown
- Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED
- Summary: - Recorded: 2026-10-25T00:14:05Z - Status: FIXED IN SOURCE; RUNTIME RECHECK PASSED
- Detail: [npc-waves-stale-REG-2026-10-25.md](npc-waves-stale-REG-2026-10-25.md)
