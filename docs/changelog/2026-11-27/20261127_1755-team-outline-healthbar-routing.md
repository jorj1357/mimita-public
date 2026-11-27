# Team outline/healthbar policy: context routing and proof-gap note

Written 2026-11-27 by `rocket-league-settle`. Coordination-only session; no
source or config file was edited.

## Context routed

High-priority user context `ctx-7386cfc693ef` ("teammate = healthbar, enemy = no
healthbar; red/green outlines not good") was already linked to
`issue-e873cbbdbeeb` and assigned to `rocket-league-input-fix`. I acknowledged
the context and did not duplicate the issue.

## Findings

- `funcounterstrike.json` (Fun Counter-Strike, the reported session) still sets
  `player_outlines=true` and `friendly_outlines=true` with both healthbar flags
  `false`, so it renders the red/green outlines the user disliked.
- `tdm.json` already carries the requested policy in the working tree
  (`enemy_healthbars=false`, `friendly_healthbars=true`,
  `player_outlines=false`); it is uncommitted and was left untouched.
- `bloodtest.json` already has all four presentation flags off.
- `rocket-league-input-fix` holds active edit leases on
  `config/gamemodes/funcounterstrike.json` and
  `docs/specs/gamemodes/funcounterstrike.md` (taken 17:47Z), and those already
  describe the requested policy.

## Proof gap recorded on the issue

The issue acceptance names a "presentation policy event showing
friendly_healthbars/enemy_healthbars/outlines", but the existing
`gamemode.presentation.policy` event
(`src/gamemode/gamemode-runtime-resolver.cpp:143-167`) only emits `mode_id`,
`hud_scene_layer`, `proximity_fade`, `first_person_hide_cosmetics`, and
`gore_mode`. It carries no healthbar/outline fields, so the event alone cannot
prove the new policy. Suggested alternatives were recorded on the issue
(`issue suggest`) and sent to the owner: extend the policy event payload, or
prove with `player_visuals.outline` rows plus the existing `HEALTHBAR` cull
reasons.

Evidence is source/coordination only. No runtime proof or visual acceptance was
claimed.
