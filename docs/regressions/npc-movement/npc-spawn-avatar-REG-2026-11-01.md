# NPC spawn exposed a placeholder avatar before the mode policy

Time created: 2026-11-01T19:49:06Z
Time last updated: 2026-11-01T19:49:06Z

Status: IMPLEMENTED IN SOURCE; RUNTIME EVIDENCE PARTIAL

Related specification:
`docs/specs/npc-behavior/npc-behavior.md`

Related changelog:
`docs/changelog/2026-11-01/20261101_194906-actor-support-ai-match-journal-rotation.md`

---

## Regression Occurrence 1

### Observed

Human report on Dust2 TDM: NPCs appear with random avatars and are then
corrected to the mode's avatar policy. The intermediate avatar can be a
monster/headless model that does not match the mode.

### Expected Behavior

A mode avatar policy selects the avatar before the first pose is broadcast.
`keep_per_npc` keeps a real previous avatar across respawns; it does not keep
the constructor's generic placeholder.

### Actual Behavior

`Npc::Npc` chose a per-life generic avatar. `finalizeServerNpcSpawn` then passed
that placeholder as `previous` to the mode policy, so a disallowed random
monster/headless avatar could be logged, applied, and immediately replaced.

### Correction

- Added an avatar placeholder marker to `Npc`.
- `finalizeServerNpcSpawn` clears a first-life placeholder before the mode
  policy runs, then records the real selection. Respawns keep the selected
  avatar for `keep_per_npc`.
- Non-mode spawns still receive their generic or role-forced avatar.

### Evidence

Runtime journal build 152 shows first-mode selections with an empty previous
avatar (`'' -> abusivegirl`, `'' -> clown`, etc.) and no monster-to-allowed
correction. Human visual confirmation is still required.
