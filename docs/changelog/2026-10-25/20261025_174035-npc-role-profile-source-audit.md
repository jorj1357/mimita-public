# NPC roles, presets, and aggression source audit

- **Time UTC:** `2026-10-25T17:40:35Z`
- **Display time:** `2026-10-25 13:40:35 EDT` (`America/New_York`)
- **Branch:** `20261011zt`
- **HEAD:** `0f33025c`
- **Result:** `PASS_WITH_HUMAN_REVIEW`

## Request

Explain `docs/jorj-docs/20261025plan.md` against the current NPC role, preset,
behavior profile, and difficulty code, including practical aggression tuning.
No gameplay or config edits requested.

## Source findings

- `config/gamemodes/sandbox.json` currently chooses
  `npc_behavior_profile: nervousbad`; that profile exists in
  `config/behavior-profiles.json` and is intentionally weak (aim error 82°,
  reaction delay 1.5 s, cadence 0.25, aggression 0.05, preferred range 30 m,
  and combat movement disabled).
- `config/roles.json` assigns Sandbox NPCs role `sandbox_juggernaut_npc`,
  which explicitly sets `movement_preset: source`. This role points to
  `juggernaut_arcade`, but role resolution uses the explicit role movement
  field. Therefore the NPC can move at Source speed despite its Juggernaut
  preset reference.
- `serverResolveActorSpawnProfile` in `src/network/server-gamemode.cpp` resolves
  NPC behavior precedence as per-actor descriptor, valid mode profile, role
  profile, then defaults. Humans do not receive NPC behavior. Actor presets own
  reusable movement/avatar/presentation policy; they are not synonymous with
  behavior profiles.
- Duel config currently names `juggernaut_swarm` as a mode `actor_preset`, but
  that ID exists in `config/behavior-profiles.json`, not
  `config/actor-presets/`. Duel also explicitly assigns `juggernaut_fighter`
  as its team role and NPC role. That role selects `juggernaut_swarm` and
  `source` movement. Its actor preset has `avatar.forced: false`; this snapshot
  does not establish that the NPC is forced to Boomer.
- `config/npc-difficulty.json` is a global fallback/tuning layer. Its
  `aggressionBonus`, fire-delay bounds, aim/error, damage multiplier, target
  rules, and randomized dash/chance values have different effects. In
  `src/npc/npc-combat.cpp`, `aggressionBonus` moves shot timing toward the
  configured minimum; it does not fix movement speed, aim error, or pursuit.
- `preferred_range` is a combat distance, not movement speed. Physical speed is
  selected by `movement_preset` from `config/movement/`.

## Files changed

- Added this source-audit changelog only. No game source, config, or plan content
  was modified. No exact old/new source content applies.

## Validation and limits

- Read the router-selected NPC behavior and actor preset specifications,
  JSON configuration architecture, specification behavior review, and task
  completion instructions.
- Read the current role/profile/mode/preset configuration and traced
  `serverResolveActorSpawnProfile`, NPC initialization, and fire cadence code.
- No build, runtime scenario, or human playtest was performed; this was a
  read-only source/config investigation. Human playtesting is required to decide
  how difficult the NPC should feel and whether avatar presentation is correct.
- The working tree had extensive pre-existing edits and untracked files before
  this audit. They were preserved.
