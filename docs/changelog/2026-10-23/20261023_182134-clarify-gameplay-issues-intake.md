# Clarify Dust 2 gameplay issue intake

- Status: `PASS_WITH_HUMAN_REVIEW`
- Recorded: 2026-10-23T22:21:34Z (2026-10-23 18:21:34 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD before this documentation edit: `2103321b`
- Scope: rewrite the user's existing Dust 2 Cyberia V4 play note in clearer prose and include the newly requested map-selection limit and map preview.
- Pre-existing work observed before editing: `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, and `docs/specs/open-gameplay-issues.md` were modified; `docs/jorj-docs/20261023plan.md` was untracked. The first four-line intake in `open-gameplay-issues.md` was already present as an uncommitted user edit. These unrelated edits were left untouched.

## Changed files and exact content

### `docs/specs/open-gameplay-issues.md`

- Lines 1-28, under `New intake — 2026-10-23, 18:14`.
- Before:

```text
2026 10 23 1814  dust 2 cberia v4 map, relates to map eidotr, npcs gettin gstuck in the  t spawn little beoxes, preformance seems better? idk 
also , npcs need the heavy shotgun to  be the sound that they actuallt use 
the heavt shotgun sound 
also we need ESP 
```

- After:

```markdown
## New intake — 2026-10-23, 18:14

Player report from Dust 2 Cyberia V4 (`dust2cyberiav4`):

- **NPC navigation:** NPCs are getting stuck in the small boxes at T spawn.
  The report relates this to the map editor; the cause has not been traced.
- **Performance:** Performance seemed better during this session, but the
  player was unsure. No measurement or comparison was recorded.
- **Heavy shotgun audio:** When NPCs use the heavy shotgun, they should play
  the heavy shotgun sound that belongs to that weapon.
- **Admin ESP:** Admins with high privileges should be able to see NPCs,
  players, and other actors through walls, including their health bars. The
  exact permission level, display details, and mode policy remain open.
- **Mode-specific map limits:** Some modes, such as Zombie Tower, should limit
  which maps can be selected. The allowed-map rules and where to configure
  them remain open.
- **Map preview:** Map selection should include a visual preview so players
  can see the map instead of choosing from text alone. The preview format and
  selection-screen details remain open.

These are user-reported observations and requests; they have not been
reproduced or accepted as implementation requirements. Candidate owners:
[`entity-editor/entity-editor.md`](entity-editor/entity-editor.md),
[`npc-behavior/npc-behavior.md`](npc-behavior/npc-behavior.md),
[`gamemodes/juggernaut.md`](gamemodes/juggernaut.md),
[`gamemodes/zombie-tower/zombie-tower.md`](gamemodes/zombie-tower/zombie-tower.md),
[`gui/guiv2.md`](gui/guiv2.md), and
[`moderation/moderation.md`](moderation/moderation.md).
```

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/open-gameplay-issues.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, and the relevant excerpts from candidate owner documents named above.
- Documentation Checker v1: all six candidate-owner links exist. The requested addition is framed as user report/request; it claims no diagnosis or runtime proof. The router points to `docs/doc-review-09-03-2026.md`, which is absent from this checkout.
- `git diff --check -- docs/specs/open-gameplay-issues.md`: passed.
- No code, build, or gameplay validation was requested or performed. Human review should decide the map allowlist policy, preview presentation, ESP permission level/policy, and the exact scope of NPC heavy-shotgun audio.
